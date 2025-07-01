// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_url_response_info.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_url_response_info_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsURLResponseInfo(PP_Resource resource) {
  VLOG(4) << "PPB_URLResponseInfo::IsURLResponseInfo()";
  EnterResource<PPB_URLResponseInfo_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

struct PP_Var GetProperty(PP_Resource response,
                          PP_URLResponseProperty property) {
  VLOG(4) << "PPB_URLResponseInfo::GetProperty()";
  EnterResource<PPB_URLResponseInfo_API> enter(response, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetProperty(property);
}

PP_Resource GetBodyAsFileRef(PP_Resource response) {
  VLOG(4) << "PPB_URLResponseInfo::GetBodyAsFileRef()";
  EnterResource<PPB_URLResponseInfo_API> enter(response, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetBodyAsFileRef();
}

const PPB_URLResponseInfo_1_0 g_ppb_urlresponseinfo_thunk_1_0 = {
    &IsURLResponseInfo, &GetProperty, &GetBodyAsFileRef};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_URLResponseInfo_1_0*
GetPPB_URLResponseInfo_1_0_Thunk() {
  return &g_ppb_urlresponseinfo_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
