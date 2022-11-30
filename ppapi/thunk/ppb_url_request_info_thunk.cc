// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_url_request_info.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_URLRequestInfo::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateURLRequestInfo(instance);
}

PP_Bool IsURLRequestInfo(PP_Resource resource) {
  VLOG(4) << "PPB_URLRequestInfo::IsURLRequestInfo()";
  EnterResource<PPB_URLRequestInfo_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool SetProperty(PP_Resource request,
                    PP_URLRequestProperty property,
                    struct PP_Var value) {
  VLOG(4) << "PPB_URLRequestInfo::SetProperty()";
  EnterResource<PPB_URLRequestInfo_API> enter(request, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->SetProperty(property, value);
}

PP_Bool AppendDataToBody(PP_Resource request, const void* data, uint32_t len) {
  VLOG(4) << "PPB_URLRequestInfo::AppendDataToBody()";
  EnterResource<PPB_URLRequestInfo_API> enter(request, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->AppendDataToBody(data, len);
}

PP_Bool AppendFileToBody(PP_Resource request,
                         PP_Resource file_ref,
                         int64_t start_offset,
                         int64_t number_of_bytes,
                         PP_Time expected_last_modified_time) {
  VLOG(4) << "PPB_URLRequestInfo::AppendFileToBody()";
  EnterResource<PPB_URLRequestInfo_API> enter(request, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->AppendFileToBody(
      file_ref, start_offset, number_of_bytes, expected_last_modified_time);
}

const PPB_URLRequestInfo_1_0 g_ppb_urlrequestinfo_thunk_1_0 = {
    &Create, &IsURLRequestInfo, &SetProperty, &AppendDataToBody,
    &AppendFileToBody};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_URLRequestInfo_1_0*
GetPPB_URLRequestInfo_1_0_Thunk() {
  return &g_ppb_urlrequestinfo_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
