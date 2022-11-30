// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_ERROR_CONVERSION_H_
#define PPAPI_PROXY_ERROR_CONVERSION_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace ppapi {
namespace proxy {

// When |private_api| is true, coverts all network-related errors +;
// PP_ERROR_NOACCESS to PP_ERROR_FAILED. Otherwise, returns |pp_error|
// as is.
PPAPI_PROXY_EXPORT int32_t ConvertNetworkAPIErrorForCompatibility(
    int32_t pp_error,
    bool private_api);

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_ERROR_CONVERSION_H_
