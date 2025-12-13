// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_CONTEXT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_CONTEXT_FACTORY_H_

#include <memory>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"

namespace blink {

class CanvasContextCreationAttributesCore;
class ExecutionContext;

// Used to tell CanvasRenderingContext how to create a WebGL 1/2 context in
// <canvas>.getContext(). Handles set up of the WebGL context and error
// handling to generate a WebGLContextEvent on error.
class WebGLContextFactory : public CanvasRenderingContextFactory {
 public:
  static std::unique_ptr<WebGLContextFactory> MakeWebGL1();
  static std::unique_ptr<WebGLContextFactory> MakeWebGL2();

  explicit WebGLContextFactory(bool is_webgl2);
  ~WebGLContextFactory() override = default;

  WebGLContextFactory(const WebGLContextFactory&) = delete;
  WebGLContextFactory& operator=(const WebGLContextFactory&) = delete;

  CanvasRenderingContext* Create(
      ExecutionContext*,
      CanvasRenderingContextHost*,
      const CanvasContextCreationAttributesCore&) override;
  CanvasRenderingContext::CanvasRenderingAPI GetRenderingAPI() const override;
  void OnError(HTMLCanvasElement*, const String& error) override;

 private:
  CanvasRenderingContext* CreateInternal(
      ExecutionContext*,
      CanvasRenderingContextHost*,
      const CanvasContextCreationAttributesCore&);
  CanvasRenderingContext* CreateInternalWebGPU(
      CanvasRenderingContextHost*,
      const CanvasContextCreationAttributesCore&);

  const char* GetContextName() const;
  Platform::WebGLContextType GetContextType() const;

  bool is_webgl2_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_CONTEXT_FACTORY_H_
