// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {
class ClientCertStore;
class SSLPrivateKey;
class X509Certificate;
}  // namespace net

namespace content {

// This class handles the approval and selection of a certificate for SSL client
// authentication by the user. Should only be used on the UI thread. If the
// SSLClientAuthHandler is destroyed before the certificate is selected, the
// selection is canceled and the delegate never called.
class SSLClientAuthHandler {
 public:
  // Delegate interface for SSLClientAuthHandler. Method implementations may
  // delete the handler when called.
  class Delegate {
   public:
    Delegate() {}

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called to continue the request with |cert|. |cert| may be nullptr.
    virtual void ContinueWithCertificate(
        scoped_refptr<net::X509Certificate> cert,
        scoped_refptr<net::SSLPrivateKey> private_key) = 0;

    // Called to cancel the certificate selection and abort the request.
    virtual void CancelCertificateSelection() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates a new SSLClientAuthHandler. The caller ensures that the handler
  // does not outlive `delegate`.
  // `browser_context` is always set, but may become invalid if the caller is
  // destroyed. `web_contents` may be null for cases where the calling context
  // is not associated with a document, such as service workers. If
  // `web_contents` is not null, it is guaranteed to be associated with the same
  // BrowserContext as `browser_context`.
  // `process_id` corresponds to the ID of the renderer process initiating the
  // request.
  SSLClientAuthHandler(std::unique_ptr<net::ClientCertStore> client_cert_store,
                       base::WeakPtr<BrowserContext> browser_context,
                       int process_id,
                       base::WeakPtr<WebContents> web_contents,
                       net::SSLCertRequestInfo* cert_request_info,
                       Delegate* delegate);

  SSLClientAuthHandler(const SSLClientAuthHandler&) = delete;
  SSLClientAuthHandler& operator=(const SSLClientAuthHandler&) = delete;

  ~SSLClientAuthHandler();

  // Selects a certificate and resumes the URL request with that certificate.
  void SelectCertificate();

 private:
  class ClientCertificateDelegateImpl;

  // Called when |core_| is done retrieving the cert list.
  void DidGetClientCerts(net::ClientCertIdentityList client_certs);

  void DidGetClientCertsOnPostTask(net::ClientCertIdentityList client_certs);

  // A callback that may be set by the UI implementation. If set, the callback
  // will cancel the dialog corresponding to this certificate request.
  base::OnceClosure cancellation_callback_;

  base::WeakPtr<BrowserContext> browser_context_;
  const int process_id_;
  base::WeakPtr<WebContents> web_contents_;

  // The certs to choose from.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  // The ClientCertStore to retrieve the certs from.
  std::unique_ptr<net::ClientCertStore> client_cert_store_;

  // The delegate to call back with the result.
  raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<SSLClientAuthHandler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
