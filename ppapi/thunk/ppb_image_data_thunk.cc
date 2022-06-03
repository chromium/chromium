// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_image_data.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_image_data_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_ImageDataFormat GetNativeImageDataFormat(void) {
  VLOG(4) << "PPB_ImageData::GetNativeImageDataFormat()";
  return PPB_ImageData_Shared::GetNativeImageDataFormat();
}

PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format) {
  VLOG(4) << "PPB_ImageData::IsImageDataFormatSupported()";
  return PPB_ImageData_Shared::IsImageDataFormatSupported(format);
}

PP_Resource Create(PP_Instance instance,
                   PP_ImageDataFormat format,
                   const struct PP_Size* size,
                   PP_Bool init_to_zero) {
  VLOG(4) << "PPB_ImageData::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateImageData(instance, format, size,
                                            init_to_zero);
}

PP_Bool IsImageData(PP_Resource image_data) {
  VLOG(4) << "PPB_ImageData::IsImageData()";
  EnterResource<PPB_ImageData_API> enter(image_data, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool Describe(PP_Resource image_data, struct PP_ImageDataDesc* desc) {
  VLOG(4) << "PPB_ImageData::Describe()";
  EnterResource<PPB_ImageData_API> enter(image_data, true);
  if (enter.failed()) {
    memset(desc, 0, sizeof(*desc));
    return PP_FALSE;
  }
  return enter.object()->Describe(desc);
}

void* Map(PP_Resource image_data) {
  VLOG(4) << "PPB_ImageData::Map()";
  EnterResource<PPB_ImageData_API> enter(image_data, true);
  if (enter.failed())
    return NULL;
  return enter.object()->Map();
}

void Unmap(PP_Resource image_data) {
  VLOG(4) << "PPB_ImageData::Unmap()";
  EnterResource<PPB_ImageData_API> enter(image_data, true);
  if (enter.failed())
    return;
  enter.object()->Unmap();
}

const PPB_ImageData_1_0 g_ppb_imagedata_thunk_1_0 = {
    &GetNativeImageDataFormat,
    &IsImageDataFormatSupported,
    &Create,
    &IsImageData,
    &Describe,
    &Map,
    &Unmap};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_ImageData_1_0* GetPPB_ImageData_1_0_Thunk() {
  return &g_ppb_imagedata_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
