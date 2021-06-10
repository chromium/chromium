// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_PREDEFINED_COLOR_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_PREDEFINED_COLOR_SPACE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Some values for PredefinedColorSpace are specified in the IDL but are
// supposed to be guarded behind the CanvasColorManagementV2 feature (e.g,
// 'rec2020'). This function will throw the exception that the IDL would
// have thrown, if CanvasColorManagementV2 is not enabled.
bool CORE_EXPORT ColorSpaceNameIsValid(const String& color_space_name,
                                       ExceptionState& exception_state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_PREDEFINED_COLOR_SPACE_H_
