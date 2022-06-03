// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_flash_font_file.idl modified Wed Mar  9 12:48:51 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_font_file.h"
#include "ppapi/shared_impl/ppb_flash_font_file_shared.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_flash_font_file_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   const struct PP_BrowserFont_Trusted_Description* description,
                   PP_PrivateFontCharset charset) {
  VLOG(4) << "PPB_Flash_FontFile::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFlashFontFile(instance, description, charset);
}

PP_Bool IsFlashFontFile(PP_Resource resource) {
  VLOG(4) << "PPB_Flash_FontFile::IsFlashFontFile()";
  EnterResource<PPB_Flash_FontFile_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool GetFontTable(PP_Resource font_file,
                     uint32_t table,
                     void* output,
                     uint32_t* output_length) {
  VLOG(4) << "PPB_Flash_FontFile::GetFontTable()";
  EnterResource<PPB_Flash_FontFile_API> enter(font_file, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetFontTable(table, output, output_length);
}

PP_Bool IsSupportedForWindows(void) {
  VLOG(4) << "PPB_Flash_FontFile::IsSupportedForWindows()";
  return PPB_Flash_FontFile_Shared::IsSupportedForWindows();
}

const PPB_Flash_FontFile_0_1 g_ppb_flash_fontfile_thunk_0_1 = {
    &Create, &IsFlashFontFile, &GetFontTable};

const PPB_Flash_FontFile_0_2 g_ppb_flash_fontfile_thunk_0_2 = {
    &Create, &IsFlashFontFile, &GetFontTable, &IsSupportedForWindows};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Flash_FontFile_0_1*
GetPPB_Flash_FontFile_0_1_Thunk() {
  return &g_ppb_flash_fontfile_thunk_0_1;
}

PPAPI_THUNK_EXPORT const PPB_Flash_FontFile_0_2*
GetPPB_Flash_FontFile_0_2_Thunk() {
  return &g_ppb_flash_fontfile_thunk_0_2;
}

}  // namespace thunk
}  // namespace ppapi
