// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_url_loader.idl modified Tue May  7 14:43:00 2013.

#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_url_loader_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_URLLoader::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateURLLoader(instance);
}

PP_Bool IsURLLoader(PP_Resource resource) {
  VLOG(4) << "PPB_URLLoader::IsURLLoader()";
  EnterResource<PPB_URLLoader_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Open(PP_Resource loader,
             PP_Resource request_info,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_URLLoader::Open()";
  EnterResource<PPB_URLLoader_API> enter(loader, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Open(request_info, enter.callback()));
}

int32_t FollowRedirect(PP_Resource loader,
                       struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_URLLoader::FollowRedirect()";
  EnterResource<PPB_URLLoader_API> enter(loader, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->FollowRedirect(enter.callback()));
}

PP_Bool GetUploadProgress(PP_Resource loader,
                          int64_t* bytes_sent,
                          int64_t* total_bytes_to_be_sent) {
  VLOG(4) << "PPB_URLLoader::GetUploadProgress()";
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed()) {
    memset(bytes_sent, 0, sizeof(*bytes_sent));
    memset(total_bytes_to_be_sent, 0, sizeof(*total_bytes_to_be_sent));
    return PP_FALSE;
  }
  return enter.object()->GetUploadProgress(bytes_sent, total_bytes_to_be_sent);
}

PP_Bool GetDownloadProgress(PP_Resource loader,
                            int64_t* bytes_received,
                            int64_t* total_bytes_to_be_received) {
  VLOG(4) << "PPB_URLLoader::GetDownloadProgress()";
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed()) {
    memset(bytes_received, 0, sizeof(*bytes_received));
    memset(total_bytes_to_be_received, 0, sizeof(*total_bytes_to_be_received));
    return PP_FALSE;
  }
  return enter.object()->GetDownloadProgress(bytes_received,
                                             total_bytes_to_be_received);
}

PP_Resource GetResponseInfo(PP_Resource loader) {
  VLOG(4) << "PPB_URLLoader::GetResponseInfo()";
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetResponseInfo();
}

int32_t ReadResponseBody(PP_Resource loader,
                         void* buffer,
                         int32_t bytes_to_read,
                         struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_URLLoader::ReadResponseBody()";
  EnterResource<PPB_URLLoader_API> enter(loader, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ReadResponseBody(buffer, bytes_to_read,
                                                          enter.callback()));
}

int32_t FinishStreamingToFile(PP_Resource loader,
                              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_URLLoader::FinishStreamingToFile()";
  EnterResource<PPB_URLLoader_API> enter(loader, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->FinishStreamingToFile(enter.callback()));
}

void Close(PP_Resource loader) {
  VLOG(4) << "PPB_URLLoader::Close()";
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

const PPB_URLLoader_1_0 g_ppb_urlloader_thunk_1_0 = {&Create,
                                                     &IsURLLoader,
                                                     &Open,
                                                     &FollowRedirect,
                                                     &GetUploadProgress,
                                                     &GetDownloadProgress,
                                                     &GetResponseInfo,
                                                     &ReadResponseBody,
                                                     &FinishStreamingToFile,
                                                     &Close};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_URLLoader_1_0* GetPPB_URLLoader_1_0_Thunk() {
  return &g_ppb_urlloader_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
