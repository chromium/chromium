// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_H_

#include <memory>

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"

namespace blink {

class ExceptionState;

class WebGL2RenderingContext : public WebGL2RenderingContextBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  WebGL2RenderingContext(
      CanvasRenderingContextHost*,
      std::unique_ptr<WebGraphicsContext3DProvider>,
      const Platform::WebGLContextInfo&,
      const CanvasContextCreationAttributesCore& requested_attributes);

  ImageBitmap* TransferToImageBitmap(ScriptState*, ExceptionState&) final;
  String ContextName() const override { return "WebGL2RenderingContext"; }
  void RegisterContextExtensions() override;
  V8RenderingContext* AsV8RenderingContext() final;
  V8OffscreenRenderingContext* AsV8OffscreenRenderingContext() final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_H_
