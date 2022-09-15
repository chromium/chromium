// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_instance_shared.h"

#include <string>

#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"

namespace ppapi {

// static
const int PPB_Instance_Shared::kExtraCharsForTextInput = 100;

PPB_Instance_Shared::~PPB_Instance_Shared() {}

void PPB_Instance_Shared::Log(PP_Instance instance,
                              PP_LogLevel level,
                              PP_Var value) {
  LogWithSource(instance, level, PP_MakeUndefined(), value);
}

void PPB_Instance_Shared::LogWithSource(PP_Instance instance,
                                        PP_LogLevel level,
                                        PP_Var source,
                                        PP_Var value) {
  // The source defaults to empty if it's not a string. The PpapiGlobals
  // implementation will convert the empty string to the module name if
  // possible.
  std::string source_str;
  if (source.type == PP_VARTYPE_STRING)
    source_str = Var::PPVarToLogString(source);
  std::string value_str = Var::PPVarToLogString(value);
  PpapiGlobals::Get()->LogWithSource(instance, level, source_str, value_str);
}

int32_t PPB_Instance_Shared::ValidateRequestInputEvents(
    bool is_filtering,
    uint32_t event_classes) {
  // See if any bits are set we don't know about.
  if (event_classes & ~static_cast<uint32_t>(PP_INPUTEVENT_CLASS_MOUSE |
                                             PP_INPUTEVENT_CLASS_KEYBOARD |
                                             PP_INPUTEVENT_CLASS_WHEEL |
                                             PP_INPUTEVENT_CLASS_TOUCH |
                                             PP_INPUTEVENT_CLASS_IME))
    return PP_ERROR_NOTSUPPORTED;

  // Everything else is valid.
  return PP_OK;
}

bool PPB_Instance_Shared::ValidateSetCursorParams(PP_MouseCursor_Type type,
                                                  PP_Resource image,
                                                  const PP_Point* hot_spot) {
  if (static_cast<int>(type) < static_cast<int>(PP_MOUSECURSOR_TYPE_CUSTOM) ||
      static_cast<int>(type) > static_cast<int>(PP_MOUSECURSOR_TYPE_GRABBING))
    return false;  // Cursor type out of range.
  if (type != PP_MOUSECURSOR_TYPE_CUSTOM) {
    // The image must not be specified if the type isn't custom. However, we
    // don't require that the hot spot be null since the C++ wrappers and proxy
    // pass the point by reference and it will normally be specified.
    return image == 0;
  }

  if (!hot_spot)
    return false;  // Hot spot must be specified for custom cursor.

  thunk::EnterResourceNoLock<thunk::PPB_ImageData_API> enter(image, true);
  if (enter.failed())
    return false;  // Invalid image resource.

  // Validate the image size. A giant cursor can arbitrarily overwrite parts
  // of the screen resulting in potential spoofing attacks. So we force the
  // cursor to be a reasonably-sized image.
  PP_ImageDataDesc desc;
  if (!PP_ToBool(enter.object()->Describe(&desc)))
    return false;
  if (desc.size.width > 32 || desc.size.height > 32)
    return false;

  // Validate image format.
  if (desc.format != PPB_ImageData_Shared::GetNativeImageDataFormat())
    return false;

  // Validate the hot spot location.
  if (hot_spot->x < 0 || hot_spot->x >= desc.size.width || hot_spot->y < 0 ||
      hot_spot->y >= desc.size.height)
    return false;
  return true;
}

}  // namespace ppapi
