// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_ERROR_CONVERSION_H_
#define PPAPI_HOST_ERROR_CONVERSION_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/host/ppapi_host_export.h"

namespace ppapi {
namespace host {

// Converts a net::Error code to a PP_Error code.
// Returns the same value as |net_error| if |net_error| is a positive number.
PPAPI_HOST_EXPORT int32_t NetErrorToPepperError(int net_error);

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_ERROR_CONVERSION_H_
