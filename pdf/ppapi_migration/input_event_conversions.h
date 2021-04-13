// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_INPUT_EVENT_CONVERSIONS_H_
#define PDF_PPAPI_MIGRATION_INPUT_EVENT_CONVERSIONS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace pp {
class InputEvent;
}  // namespace pp

namespace chrome_pdf {

std::unique_ptr<blink::WebInputEvent> GetWebInputEvent(
    const pp::InputEvent& event);

PP_CursorType_Dev PPCursorTypeFromCursorType(ui::mojom::CursorType cursor_type);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_INPUT_EVENT_CONVERSIONS_H_
