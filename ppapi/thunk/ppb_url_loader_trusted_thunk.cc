// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From trusted/ppb_url_loader_trusted.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_url_loader_api.h"

namespace ppapi {
namespace thunk {

namespace {

void GrantUniversalAccess(PP_Resource loader) {
  VLOG(4) << "PPB_URLLoaderTrusted::GrantUniversalAccess()";
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return;
  enter.object()->GrantUniversalAccess();
}

void RegisterStatusCallback(PP_Resource loader,
                            PP_URLLoaderTrusted_StatusCallback cb) {
  VLOG(4) << "PPB_URLLoaderTrusted::RegisterStatusCallback()";
  EnterResource<PPB_URLLoader_API> enter(loader, true);
  if (enter.failed())
    return;
  enter.object()->RegisterStatusCallback(cb);
}

const PPB_URLLoaderTrusted_0_3 g_ppb_urlloadertrusted_thunk_0_3 = {
    &GrantUniversalAccess, &RegisterStatusCallback};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_URLLoaderTrusted_0_3*
GetPPB_URLLoaderTrusted_0_3_Thunk() {
  return &g_ppb_urlloadertrusted_thunk_0_3;
}

}  // namespace thunk
}  // namespace ppapi
