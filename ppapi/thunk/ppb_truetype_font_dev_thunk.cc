// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_truetype_font_dev.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_truetype_font_api.h"
#include "ppapi/thunk/ppb_truetype_font_singleton_api.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetFontFamilies(PP_Instance instance,
                        struct PP_ArrayOutput output,
                        struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TrueTypeFont_Dev::GetFontFamilies()";
  EnterInstanceAPI<PPB_TrueTypeFont_Singleton_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.functions()->GetFontFamilies(instance, output, enter.callback()));
}

int32_t GetFontsInFamily(PP_Instance instance,
                         struct PP_Var family,
                         struct PP_ArrayOutput output,
                         struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TrueTypeFont_Dev::GetFontsInFamily()";
  EnterInstanceAPI<PPB_TrueTypeFont_Singleton_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.functions()->GetFontsInFamily(
      instance, family, output, enter.callback()));
}

PP_Resource Create(PP_Instance instance,
                   const struct PP_TrueTypeFontDesc_Dev* desc) {
  VLOG(4) << "PPB_TrueTypeFont_Dev::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTrueTypeFont(instance, desc);
}

PP_Bool IsTrueTypeFont(PP_Resource resource) {
  VLOG(4) << "PPB_TrueTypeFont_Dev::IsTrueTypeFont()";
  EnterResource<PPB_TrueTypeFont_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Describe(PP_Resource font,
                 struct PP_TrueTypeFontDesc_Dev* desc,
                 struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TrueTypeFont_Dev::Describe()";
  EnterResource<PPB_TrueTypeFont_API> enter(font, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Describe(desc, enter.callback()));
}

int32_t GetTableTags(PP_Resource font,
                     struct PP_ArrayOutput output,
                     struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TrueTypeFont_Dev::GetTableTags()";
  EnterResource<PPB_TrueTypeFont_API> enter(font, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetTableTags(output, enter.callback()));
}

int32_t GetTable(PP_Resource font,
                 uint32_t table,
                 int32_t offset,
                 int32_t max_data_length,
                 struct PP_ArrayOutput output,
                 struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_TrueTypeFont_Dev::GetTable()";
  EnterResource<PPB_TrueTypeFont_API> enter(font, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetTable(
      table, offset, max_data_length, output, enter.callback()));
}

const PPB_TrueTypeFont_Dev_0_1 g_ppb_truetypefont_dev_thunk_0_1 = {
    &GetFontFamilies, &GetFontsInFamily, &Create,  &IsTrueTypeFont,
    &Describe,        &GetTableTags,     &GetTable};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_TrueTypeFont_Dev_0_1*
GetPPB_TrueTypeFont_Dev_0_1_Thunk() {
  return &g_ppb_truetypefont_dev_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
