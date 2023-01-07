// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_URL_RESPONSE_INFO_API_H_
#define PPAPI_THUNK_PPB_URL_RESPONSE_INFO_API_H_

#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/shared_impl/url_response_info_data.h"

namespace ppapi {
namespace thunk {

class PPB_URLResponseInfo_API {
 public:
  virtual ~PPB_URLResponseInfo_API() {}

  virtual PP_Var GetProperty(PP_URLResponseProperty property) = 0;
  virtual PP_Resource GetBodyAsFileRef() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_URL_RESPONSE_INFO_API_H_
