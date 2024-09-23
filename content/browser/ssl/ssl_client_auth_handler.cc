// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_client_auth_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_private_key.h"

namespace content {

class SSLClientAuthHandler::ClientCertificateDelegateImpl
    : public ClientCertificateDelegate {
 public:
  explicit ClientCertificateDelegateImpl(
      base::WeakPtr<SSLClientAuthHandler> handler)
      : handler_(std::move(handler)) {}

  ClientCertificateDelegateImpl(const ClientCertificateDelegateImpl&) = delete;
  ClientCertificateDelegateImpl& operator=(
      const ClientCertificateDelegateImpl&) = delete;

  ~ClientCertificateDelegateImpl() override {
    if (!continue_called_ && handler_) {
      handler_->delegate_->CancelCertificateSelection();
    }
  }

  // ClientCertificateDelegate implementation:
  void ContinueWithCertificate(scoped_refptr<net::X509Certificate> cert,
                               scoped_refptr<net::SSLPrivateKey> key) override {
    DCHECK(!continue_called_);
    continue_called_ = true;
    if (handler_) {
      handler_->delegate_->ContinueWithCertificate(std::move(cert),
                                                   std::move(key));
    }
  }

 private:
  base::WeakPtr<SSLClientAuthHandler> handler_;
  bool continue_called_ = false;
};

SSLClientAuthHandler::SSLClientAuthHandler(
    std::unique_ptr<net::ClientCertStore> client_cert_store,
    base::WeakPtr<BrowserContext> browser_context,
    int process_id,
    base::WeakPtr<WebContents> web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    Delegate* delegate)
    : browser_context_(browser_context),
      process_id_(process_id),
      web_contents_(web_contents),
      cert_request_info_(cert_request_info),
      client_cert_store_(std::move(client_cert_store)),
      delegate_(delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (web_contents_) {
    CHECK_EQ(web_contents_->GetBrowserContext(), browser_context_.get());
  }
}

SSLClientAuthHandler::~SSLClientAuthHandler() {
  // Invalidate our WeakPtrs in case invoking the cancellation callback would
  // cause |this| to be destructed again.
  weak_factory_.InvalidateWeakPtrs();
  if (cancellation_callback_) {
    std::move(cancellation_callback_).Run();
  }
}

void SSLClientAuthHandler::SelectCertificate() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (client_cert_store_) {
    client_cert_store_->GetClientCerts(
        cert_request_info_,
        base::BindOnce(&SSLClientAuthHandler::DidGetClientCerts,
                       weak_factory_.GetWeakPtr()));
  } else {
    DidGetClientCerts(net::ClientCertIdentityList());
  }
}

void SSLClientAuthHandler::DidGetClientCerts(
    net::ClientCertIdentityList client_certs) {
  // Run this on a PostTask to avoid reentrancy problems.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SSLClientAuthHandler::DidGetClientCertsOnPostTask,
                     weak_factory_.GetWeakPtr(), std::move(client_certs)));
}

void SSLClientAuthHandler::DidGetClientCertsOnPostTask(
    net::ClientCertIdentityList client_certs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!browser_context_) {
    delegate_->CancelCertificateSelection();
    return;
  }

  // SelectClientCertificate() may call back into |delegate_| synchronously and
  // destroy this object, so guard the cancellation callback logic by a WeakPtr.
  base::WeakPtr<SSLClientAuthHandler> weak_self = weak_factory_.GetWeakPtr();
  base::OnceClosure cancellation_callback =
      GetContentClient()->browser()->SelectClientCertificate(
          browser_context_.get(), process_id_, web_contents_.get(),
          cert_request_info_.get(), std::move(client_certs),
          std::make_unique<ClientCertificateDelegateImpl>(weak_self));
  if (weak_self) {
    cancellation_callback_ = std::move(cancellation_callback);
  } else if (!cancellation_callback.is_null()) {
    std::move(cancellation_callback).Run();
  }
}

}  // namespace content
