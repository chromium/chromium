// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/cookie_manager_impl.h"

#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace {

fuchsia::web::Cookie ConvertCanonicalCookie(
    const net::CanonicalCookie& canonical_cookie,
    net::CookieChangeCause cause) {
  fuchsia::web::CookieId id;
  id.set_name(canonical_cookie.Name());
  id.set_domain(canonical_cookie.Domain());
  id.set_path(canonical_cookie.Path());

  fuchsia::web::Cookie cookie;
  cookie.set_id(std::move(id));
  switch (cause) {
    case net::CookieChangeCause::INSERTED:
      cookie.set_value(canonical_cookie.Value());
      break;
    case net::CookieChangeCause::EXPLICIT:
    case net::CookieChangeCause::UNKNOWN_DELETION:
    case net::CookieChangeCause::OVERWRITE:
    case net::CookieChangeCause::EXPIRED:
    case net::CookieChangeCause::EVICTED:
    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      break;
  };

  return cookie;
}

class CookiesIteratorImpl : public fuchsia::web::CookiesIterator,
                            public network::mojom::CookieChangeListener {
 public:
  // |this| will delete itself when |mojo_request| or |changes| disconnect.
  CookiesIteratorImpl(
      mojo::PendingReceiver<network::mojom::CookieChangeListener> mojo_receiver,
      fidl::InterfaceRequest<fuchsia::web::CookiesIterator> changes)
      : CookiesIteratorImpl(std::move(changes)) {
    mojo_receiver_.Bind(std::move(mojo_receiver));
    mojo_receiver_.set_disconnect_handler(base::BindOnce(
        &CookiesIteratorImpl::OnMojoError, base::Unretained(this)));
  }
  // |this| will delete itself when |iterator| disconnects, or if a GetNext()
  // leaves |queued_cookies_| empty.
  CookiesIteratorImpl(
      const std::vector<net::CanonicalCookie>& cookies,
      fidl::InterfaceRequest<fuchsia::web::CookiesIterator> iterator)
      : CookiesIteratorImpl(std::move(iterator)) {
    for (const auto& cookie : cookies) {
      queued_cookies_[cookie.UniqueKey()] =
          ConvertCanonicalCookie(cookie, net::CookieChangeCause::INSERTED);
    }
  }
  // Same as above except it takes CookieStatusList instead of just CookieList.
  CookiesIteratorImpl(
      const std::vector<net::CookieWithStatus>& cookies_with_statuses,
      fidl::InterfaceRequest<fuchsia::web::CookiesIterator> iterator)
      : CookiesIteratorImpl(std::move(iterator)) {
    for (const auto& cookie_with_status : cookies_with_statuses) {
      queued_cookies_[cookie_with_status.cookie.UniqueKey()] =
          ConvertCanonicalCookie(cookie_with_status.cookie,
                                 net::CookieChangeCause::INSERTED);
    }
  }
  ~CookiesIteratorImpl() final = default;

  // fuchsia::web::CookiesIterator implementation:
  void GetNext(GetNextCallback callback) final {
    DCHECK(!get_next_callback_);
    get_next_callback_ = std::move(callback);
    MaybeSendQueuedCookies();
  }

 private:
  explicit CookiesIteratorImpl(
      fidl::InterfaceRequest<fuchsia::web::CookiesIterator> iterator)
      : mojo_receiver_(this), fidl_binding_(this) {
    fidl_binding_.Bind(std::move(iterator));
    fidl_binding_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
          << "CookieChangeListener disconnected.";
      delete this;
    });
  }

  void OnMojoError() {
    LOG(ERROR) << "NetworkService disconnected CookiesIterator.";
    fidl_binding_.Close(ZX_ERR_UNAVAILABLE);
    delete this;
  }

  void MaybeSendQueuedCookies() {
    // Assuming cookies values never exceed 4KB in size, plus some overhead for
    // the name, domain and path, and that Zircon messages can be up to 64KB.
    constexpr int kMaxCookiesPerMessage = 8;

    if (!get_next_callback_)
      return;
    if (mojo_receiver_.is_bound() && queued_cookies_.empty())
      return;

    // Build a vector of Cookies to return to the caller.
    fuchsia::web::CookiesIterator::GetNextCallback callback(
        std::move(get_next_callback_));
    std::vector<fuchsia::web::Cookie> cookies;
    while (!queued_cookies_.empty() && cookies.size() < kMaxCookiesPerMessage) {
      auto cookie = queued_cookies_.begin();
      cookies.emplace_back(std::move(cookie->second));
      queued_cookies_.erase(cookie);
    }
    callback(std::move(cookies));

    // If this is a one-off CookieIterator then tear down once |queued_cookies_|
    // is empty.
    if (queued_cookies_.empty() && !mojo_receiver_.is_bound())
      delete this;
  }

  // network::mojom::CookieChangeListener implementation:
  void OnCookieChange(const net::CookieChangeInfo& change) final {
    queued_cookies_[change.cookie.UniqueKey()] =
        ConvertCanonicalCookie(change.cookie, change.cause);
    MaybeSendQueuedCookies();
  }

  mojo::Receiver<network::mojom::CookieChangeListener> mojo_receiver_;
  fidl::Binding<fuchsia::web::CookiesIterator> fidl_binding_;

  GetNextCallback get_next_callback_;

  // Map from "unique key"s (see net::CanonicalCookie::UniqueKey()) to the
  // corresponding fuchsia::web::Cookie.
  std::map<std::tuple<std::string, std::string, std::string>,
           fuchsia::web::Cookie>
      queued_cookies_;

  DISALLOW_COPY_AND_ASSIGN(CookiesIteratorImpl);
};

void OnAllCookiesReceived(
    fidl::InterfaceRequest<fuchsia::web::CookiesIterator> iterator,
    const std::vector<net::CanonicalCookie>& cookies) {
  new CookiesIteratorImpl(cookies, std::move(iterator));
}

void OnCookiesAndExcludedReceived(
    fidl::InterfaceRequest<fuchsia::web::CookiesIterator> iterator,
    const std::vector<net::CookieWithStatus>& cookies_with_statuses,
    const std::vector<net::CookieWithStatus>& excluded_cookies) {
  // Since CookieOptions::set_return_excluded_cookies() is not used when calling
  // the Mojo GetCookieList() API, |excluded_cookies| should be empty.
  DCHECK(excluded_cookies.empty());
  new CookiesIteratorImpl(cookies_with_statuses, std::move(iterator));
}

}  // namespace

CookieManagerImpl::CookieManagerImpl(
    GetNetworkContextCallback get_network_context)
    : get_network_context_(std::move(get_network_context)) {}

CookieManagerImpl::~CookieManagerImpl() = default;

void CookieManagerImpl::ObserveCookieChanges(
    fidl::StringPtr url,
    fidl::StringPtr name,
    fidl::InterfaceRequest<fuchsia::web::CookiesIterator> changes) {
  EnsureCookieManager();

  mojo::PendingRemote<network::mojom::CookieChangeListener> mojo_listener;
  new CookiesIteratorImpl(mojo_listener.InitWithNewPipeAndPassReceiver(),
                          std::move(changes));

  if (url) {
    base::Optional<std::string> maybe_name;
    if (name)
      maybe_name = *name;
    cookie_manager_->AddCookieChangeListener(GURL(*url), maybe_name,
                                             std::move(mojo_listener));
  } else {
    cookie_manager_->AddGlobalChangeListener(std::move(mojo_listener));
  }
}

void CookieManagerImpl::GetCookieList(
    fidl::StringPtr url,
    fidl::StringPtr name,
    fidl::InterfaceRequest<fuchsia::web::CookiesIterator> iterator) {
  EnsureCookieManager();

  if (!url && !name) {
    cookie_manager_->GetAllCookies(
        base::BindOnce(&OnAllCookiesReceived, std::move(iterator)));
  } else {
    if (!name) {
      // Include HTTP and 1st-party-only cookies in those returned.
      net::CookieOptions options;
      options.set_include_httponly();
      options.set_same_site_cookie_context(
          net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

      cookie_manager_->GetCookieList(
          GURL(*url), options,
          base::BindOnce(&OnCookiesAndExcludedReceived, std::move(iterator)));
    } else {
      // TODO(858853): Support filtering by name.
      iterator.Close(ZX_ERR_NOT_SUPPORTED);
    }
  }
}

void CookieManagerImpl::EnsureCookieManager() {
  if (cookie_manager_.is_bound())
    return;
  get_network_context_.Run()->GetCookieManager(
      cookie_manager_.BindNewPipeAndPassReceiver());
  cookie_manager_.set_disconnect_handler(base::BindOnce(
      &CookieManagerImpl::OnMojoDisconnect, base::Unretained(this)));
}

void CookieManagerImpl::OnMojoDisconnect() {
  LOG(ERROR) << "NetworkService disconnected CookieManager.";
  if (on_mojo_disconnected_for_test_)
    std::move(on_mojo_disconnected_for_test_).Run();
  cookie_manager_.reset();
}
