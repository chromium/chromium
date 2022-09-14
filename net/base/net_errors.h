// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_ERRORS_H__
#define NET_BASE_NET_ERRORS_H__

#include <string>

#include "base/files/file.h"
#include "base/logging.h"
#include "net/base/net_export.h"

namespace net {

// Error values are negative.
enum Error {
  // No error. Change NetError.template after changing value.
  OK = 0,

#define NET_ERROR(label, value) ERR_ ## label = value,
#include "net/base/net_error_list.h"
#undef NET_ERROR

  // The value of the first certificate error code.
  ERR_CERT_BEGIN = ERR_CERT_COMMON_NAME_INVALID,
};

// Returns a textual representation of the error code for logging purposes.
NET_EXPORT std::string ErrorToString(int error);

// Same as above, but leaves off the leading "net::".
NET_EXPORT std::string ErrorToShortString(int error);

// Returns a textual representation of the error code and the extended eror
// code.
NET_EXPORT std::string ExtendedErrorToString(int error,
                                             int extended_error_code);

// Returns true if |error| is a certificate error code. Note this does not
// include errors for client certificates.
NET_EXPORT bool IsCertificateError(int error);

// Returns true if |error| is a client certificate authentication error. This
// does not include ERR_SSL_PROTOCOL_ERROR which may also signal a bad client
// certificate.
NET_EXPORT bool IsClientCertificateError(int error);

// Returns true if |error| is an error from hostname resolution.
NET_EXPORT bool IsHostnameResolutionError(int error);

// Returns true if |error| means that the request has been blocked.
NET_EXPORT bool IsRequestBlockedError(int error);

// Map system error code to Error.
NET_EXPORT Error MapSystemError(logging::SystemErrorCode os_error);

// A convenient function to translate file error to net error code.
NET_EXPORT Error FileErrorToNetError(base::File::Error file_error);

}  // namespace net

#endif  // NET_BASE_NET_ERRORS_H__
