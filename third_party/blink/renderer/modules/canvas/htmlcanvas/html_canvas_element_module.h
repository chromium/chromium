// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_HTMLCANVAS_HTML_CANVAS_ELEMENT_MODULE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_HTMLCANVAS_HTML_CANVAS_ELEMENT_MODULE_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CanvasContextCreationAttributesModule;
class HTMLCanvasElement;
class OffscreenCanvas;

class MODULES_EXPORT HTMLCanvasElementModule {
  STATIC_ONLY(HTMLCanvasElementModule);

  friend class HTMLCanvasElementModuleTest;

 public:
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static V8RenderingContext* getContext(
      HTMLCanvasElement& canvas,
      const String& context_id,
      const CanvasContextCreationAttributesModule* attributes,
      ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static void getContext(HTMLCanvasElement&,
                         const String&,
                         const CanvasContextCreationAttributesModule*,
                         RenderingContext&,
                         ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static OffscreenCanvas* transferControlToOffscreen(ExecutionContext*,
                                                     HTMLCanvasElement&,
                                                     ExceptionState&);

 private:
  static OffscreenCanvas* TransferControlToOffscreenInternal(ExecutionContext*,
                                                             HTMLCanvasElement&,
                                                             ExceptionState&);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_HTMLCANVAS_HTML_CANVAS_ELEMENT_MODULE_H_
