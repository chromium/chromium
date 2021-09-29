// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
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
  class CONTENT_EXPORT Delegate {
   public:
    Delegate() {}

    // Called to continue the request with |cert|. |cert| may be nullptr.
    virtual void ContinueWithCertificate(
        scoped_refptr<net::X509Certificate> cert,
        scoped_refptr<net::SSLPrivateKey> private_key) = 0;

    // Called to cancel the certificate selection and abort the request.
    virtual void CancelCertificateSelection() = 0;

   protected:
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Creates a new SSLClientAuthHandler. The caller ensures that the handler
  // does not outlive |delegate|.
  SSLClientAuthHandler(std::unique_ptr<net::ClientCertStore> client_cert_store,
                       WebContents::Getter web_contents_getter,
                       net::SSLCertRequestInfo* cert_request_info,
                       Delegate* delegate);
  ~SSLClientAuthHandler();

  // Selects a certificate and resumes the URL request with that certificate.
  void SelectCertificate();

 private:
  class ClientCertificateDelegateImpl;
  class Core;

  // Called when |core_| is done retrieving the cert list.
  void DidGetClientCerts(net::ClientCertIdentityList client_certs);

  // A reference-counted core so the ClientCertStore may outlive
  // SSLClientAuthHandler if the handler is destroyed while an operation on the
  // ClientCertStore is in progress.
  scoped_refptr<Core> core_;

  // A callback that may be set by the UI implementation. If set, the callback
  // will cancel the dialog corresponding to this certificate request.
  base::OnceClosure cancellation_callback_;

  WebContents::Getter web_contents_getter_;

  // The certs to choose from.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  // The delegate to call back with the result.
  Delegate* delegate_;

  base::WeakPtrFactory<SSLClientAuthHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
