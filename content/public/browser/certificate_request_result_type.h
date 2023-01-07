// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CERTIFICATE_REQUEST_RESULT_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_CERTIFICATE_REQUEST_RESULT_TYPE_H_

namespace content {

// Used to specify synchronous result codes when processing a certificate
// request.
enum CertificateRequestResultType {
  // Continue processing the request. Result will be returned asynchronously.
  CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE,

  // Cancels the request synchronously using a net::ERR_ABORTED.
  CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL,

  // Denies the request synchronously using the certificate error code that was
  // encountered.
  CERTIFICATE_REQUEST_RESULT_TYPE_DENY,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CERTIFICATE_REQUEST_RESULT_TYPE_H_
