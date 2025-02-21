// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"

#include <memory>

#include "base/check.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace content {

std::unique_ptr<PrefetchCookieListener> PrefetchCookieListener::MakeAndRegister(
    const GURL& url,
    network::mojom::CookieManager* cookie_manager) {
  DCHECK(cookie_manager);

  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      std::make_unique<PrefetchCookieListener>(url);

  // |cookie_listener| will get updates whenever host cookies for |url| or
  // domain cookies that match |url| are changed.
  cookie_manager->AddCookieChangeListener(
      url, std::nullopt,
      cookie_listener->cookie_listener_receiver_.BindNewPipeAndPassRemote());

  return cookie_listener;
}

PrefetchCookieListener::PrefetchCookieListener(const GURL& url) : url_(url) {}

PrefetchCookieListener::~PrefetchCookieListener() = default;

void PrefetchCookieListener::StopListening() {
  cookie_listener_receiver_.reset();
}

void PrefetchCookieListener::OnCookieChange(
    const net::CookieChangeInfo& change) {
  DCHECK(url_.DomainIs(change.cookie.DomainWithoutDot()));
  have_cookies_changed_ = true;

  // Once we record one change to the cookies associated with |url_|, we don't
  // care about any subsequent changes.
  StopListening();
}

}  // namespace content