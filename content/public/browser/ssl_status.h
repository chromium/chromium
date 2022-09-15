// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SSL_STATUS_H_
#define CONTENT_PUBLIC_BROWSER_SSL_STATUS_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace net {
class SSLInfo;
}

namespace content {

// Collects the SSL information for this NavigationEntry.
struct CONTENT_EXPORT SSLStatus {
  // Flags used for the page security content status.
  enum ContentStatusFlags {
    // HTTP page, or HTTPS page with no insecure content.
    NORMAL_CONTENT = 0,

    // HTTPS page containing "displayed" HTTP resources (e.g. images, CSS).
    DISPLAYED_INSECURE_CONTENT = 1 << 0,

    // HTTPS page containing "executed" HTTP resources (i.e. script).
    RAN_INSECURE_CONTENT = 1 << 1,

    // HTTPS page containing "displayed" HTTPS resources (e.g. images,
    // CSS) loaded with certificate errors.
    DISPLAYED_CONTENT_WITH_CERT_ERRORS = 1 << 2,

    // HTTPS page containing "executed" HTTPS resources (i.e. script)
    // loaded with certificate errors.
    RAN_CONTENT_WITH_CERT_ERRORS = 1 << 3,

    // HTTPS page containing a form targeting an insecure action url.
    DISPLAYED_FORM_WITH_INSECURE_ACTION = 1 << 6,
  };

  SSLStatus();
  explicit SSLStatus(const net::SSLInfo& ssl_info);
  SSLStatus(const SSLStatus& other);
  SSLStatus& operator=(SSLStatus other);
  ~SSLStatus();

  bool initialized;
  scoped_refptr<net::X509Certificate> certificate;
  net::CertStatus cert_status;
  uint16_t key_exchange_group;
  uint16_t peer_signature_algorithm;
  int connection_status;
  // A combination of the ContentStatusFlags above. Flags are cleared when a
  // navigation commits.
  int content_status;
  // True if PKP was bypassed due to a local trust anchor.
  bool pkp_bypassed;
  // Whether the page's main resource complied with the Certificate Transparency
  // policy.
  net::ct::CTPolicyCompliance ct_policy_compliance;

  // If you add new fields here, be sure to add them in the copy constructor and
  // copy assignment operator definitions in ssl_status.cc.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SSL_STATUS_H_
