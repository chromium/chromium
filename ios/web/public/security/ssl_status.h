// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_SSL_STATUS_H_
#define IOS_WEB_PUBLIC_SECURITY_SSL_STATUS_H_

#include <string>

#include "ios/web/public/security/security_style.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace web {

// Collects the SSL information for this NavigationItem.
struct SSLStatus {
  // Flags used for the page security content status.
  enum ContentStatusFlags {
    // HTTP page, or HTTPS page with no insecure content.
    NORMAL_CONTENT = 0,

    // HTTPS page containing "displayed" HTTP resources (e.g. images, CSS).
    DISPLAYED_INSECURE_CONTENT = 1 << 0,

    // The RAN_INSECURE_CONTENT flag is intentionally omitted on iOS because
    // there is no way to tell when insecure content is run in a web view.
  };

  SSLStatus();
  SSLStatus(const SSLStatus& other);
  SSLStatus& operator=(SSLStatus other);
  ~SSLStatus();

  bool Equals(const SSLStatus& status) const {
    return security_style == status.security_style &&
           !!certificate == !!status.certificate &&
           (certificate
                ? certificate->EqualsIncludingChain(status.certificate.get())
                : true) &&
           cert_status == status.cert_status &&
           content_status == status.content_status;
    // `cert_status_host` is not used for comparison intentionally.
  }

  web::SecurityStyle security_style;
  scoped_refptr<net::X509Certificate> certificate;
  net::CertStatus cert_status;
  // A combination of the ContentStatusFlags above.
  int content_status;
  // Host which was used for `cert_status` calculation. It is not an actual part
  // of SSL status, hence it's not taken into account in `Equals` method.
  // Used to check if `cert_status` is still valid or needs to be recalculated
  // (e.g. after redirect).
  std::string cert_status_host;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_SSL_STATUS_H_
