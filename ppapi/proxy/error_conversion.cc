// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/error_conversion.h"

#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace proxy {

int32_t ConvertNetworkAPIErrorForCompatibility(int32_t pp_error,
                                               bool private_api) {
  // The private API doesn't return network-specific error codes or
  // PP_ERROR_NOACCESS. In order to preserve the behavior, we convert those to
  // PP_ERROR_FAILED.
  if (private_api &&
      (pp_error <= PP_ERROR_CONNECTION_CLOSED ||
       pp_error == PP_ERROR_NOACCESS)) {
    return PP_ERROR_FAILED;
  }
  return pp_error;
}

}  // namespace proxy
}  // namespace ppapi
