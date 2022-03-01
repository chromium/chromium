// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ppapi.h"

#include "base/notreached.h"
#include "ppapi/c/pp_errors.h"

namespace chrome_pdf {

int32_t PPP_InitializeModule(PP_Module /*module_id*/,
                             PPB_GetInterface /*get_browser_interface*/) {
  NOTREACHED();
  return PP_ERROR_FAILED;
}

void PPP_ShutdownModule() {
  NOTREACHED();
}

const void* PPP_GetInterface(const char* /*interface_name*/) {
  NOTREACHED();
  return nullptr;
}

}  // namespace chrome_pdf
