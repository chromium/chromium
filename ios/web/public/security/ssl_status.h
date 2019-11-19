// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_SSL_STATUS_H_
#define IOS_WEB_PUBLIC_SECURITY_SSL_STATUS_H_

#include <memory>
#include <string>

#include "ios/web/public/security/security_style.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace web {

// Collects the SSL information for this NavigationItem.
struct SSLStatus {
  // SSLStatus consumers can attach instances of derived UserData classes to an
  // SSLStatus. This allows an embedder to attach data to the NavigationItem
  // without SSLStatus having to know about it. Derived UserData classes have to
  // be cloneable since NavigationItems are cloned during navigations.
  class UserData {
   public:
    UserData() {}
    virtual ~UserData() = default;
    virtual std::unique_ptr<UserData> Clone() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(UserData);
  };

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
    // |cert_status_host| is not used for comparison intentionally.
    // |user_data| also not used for comparison.
  }

  web::SecurityStyle security_style;
  scoped_refptr<net::X509Certificate> certificate;
  net::CertStatus cert_status;
  // A combination of the ContentStatusFlags above.
  int content_status;
  // Host which was used for |cert_status| calculation. It is not an actual part
  // of SSL status, hence it's not taken into account in |Equals| method.
  // Used to check if |cert_status| is still valid or needs to be recalculated
  // (e.g. after redirect).
  std::string cert_status_host;
  // Embedder-specific data attached to the SSLStatus is cloned when an
  // |SSLStatus| is assigned or copy-constructed, and is cleared when a
  // navigation commits.
  std::unique_ptr<UserData> user_data;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_SSL_STATUS_H_
