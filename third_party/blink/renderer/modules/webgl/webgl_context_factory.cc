// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_context_factory.h"

#include "base/notimplemented.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_webgpu.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_event.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_webgpu.h"
#include "third_party/blink/renderer/platform/graphics/predefined_color_space.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
std::unique_ptr<WebGLContextFactory> WebGLContextFactory::MakeWebGL1() {
  return std::make_unique<WebGLContextFactory>(/* is_webgl2 */ false);
}

// static
std::unique_ptr<WebGLContextFactory> WebGLContextFactory::MakeWebGL2() {
  return std::make_unique<WebGLContextFactory>(/* is_webgl2 */ true);
}

WebGLContextFactory::WebGLContextFactory(bool is_webgl2)
    : is_webgl2_(is_webgl2) {}

CanvasRenderingContext* WebGLContextFactory::Create(
    ExecutionContext* execution_context,
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  if (RuntimeEnabledFeatures::WebGLOnWebGPUEnabled()) {
    return CreateInternalWebGPU(host, attrs);
  } else {
    return CreateInternal(execution_context, host, attrs);
  }
}

CanvasRenderingContext* WebGLContextFactory::CreateInternal(
    ExecutionContext* execution_context,
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  // Create a copy of attrs so flags can be modified if needed before passing
  // into the WebGLRenderingContext constructor.
  CanvasContextCreationAttributesCore attribs = attrs;

  // The xr_compatible attribute needs to be handled before creating the context
  // because the GPU process may potentially be restarted in order to be XR
  // compatible. This scenario occurs if the GPU process is not using the GPU
  // that the VR headset is plugged into. If the GPU process is restarted, the
  // WebGraphicsContext3DProvider must be created using the new one.
  if (attribs.xr_compatible &&
      !WebGLRenderingContextBase::MakeXrCompatibleSync(host)) {
    // If xr compatibility is requested and we can't be xr compatible, return a
    // context with the flag set to false.
    attribs.xr_compatible = false;
  }

  // Create the Context3DProvider
  Platform::WebGLContextInfo context_info;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider(
      WebGLRenderingContextBase::CreateWebGraphicsContext3DProvider(
          host, attribs, GetContextType(), &context_info));
  if (!context_provider) {
    // CreateWebGraphicsContext3DProvider already dispatches a
    // webglcontextcreationerror so we don't skip generating one here.
    return nullptr;
  }

  // Attempt to add a marker to make this context easier to find.
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);
  if (extensions_util->SupportsExtension("GL_EXT_debug_marker")) {
    String context_label(
        String::Format("%s-%p", GetContextName(), context_provider.get()));
    gl->PushGroupMarkerEXT(0, context_label.Ascii().c_str());
  }

  auto Initialize = [&](auto rendering_context) -> CanvasRenderingContext* {
    if (!rendering_context->GetDrawingBuffer()) {
      // We must dispose immediately so that when rendering_context is
      // garbage-collected, it will not interfere with a subsequently created
      // rendering context.
      rendering_context->Dispose();

      host->HostDispatchEvent(WebGLContextEvent::Create(
          event_type_names::kWebglcontextcreationerror,
          String::Format("Failed to create %s.", GetContextName())));
      return nullptr;
    }

    rendering_context->InitializeNewContext();
    rendering_context->RegisterContextExtensions();
    return rendering_context;
  };

  // Report WebDXFeatures and use counters.
  if (attribs.desynchronized) {
    UseCounter::Count(execution_context,
                      WebFeature::kHTMLCanvasElementLowLatency_WebGL);
    UseCounter::CountWebDXFeature(
        execution_context, is_webgl2_ ? WebDXFeature::kWebgl2Desynchronized
                                      : WebDXFeature::kWebglDesynchronized);
    UseCounter::Count(
        execution_context,
        attribs.preserve_drawing_buffer
            ? WebFeature::kHTMLCanvasElementLowLatency_WebGL_Preserve
            : WebFeature::kHTMLCanvasElementLowLatency_WebGL_Discard);
  }

  if (is_webgl2_) {
    return Initialize(MakeGarbageCollected<WebGL2RenderingContext>(
        host, std::move(context_provider), context_info, attribs));
  } else {
    return Initialize(MakeGarbageCollected<WebGLRenderingContext>(
        host, std::move(context_provider), context_info, attribs));
  }
}

CanvasRenderingContext* WebGLContextFactory::CreateInternalWebGPU(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  if (is_webgl2_) {
    return MakeGarbageCollected<WebGL2RenderingContextWebGPU>(host, attrs);
  } else {
    return MakeGarbageCollected<WebGLRenderingContextWebGPU>(host, attrs);
  }
}

CanvasRenderingContext::CanvasRenderingAPI
WebGLContextFactory::GetRenderingAPI() const {
  if (is_webgl2_) {
    return CanvasRenderingContext::CanvasRenderingAPI::kWebgl2;
  }
  return CanvasRenderingContext::CanvasRenderingAPI::kWebgl;
}

void WebGLContextFactory::OnError(HTMLCanvasElement* canvas,
                                  const String& error) {
  canvas->DispatchEvent(*WebGLContextEvent::Create(
      event_type_names::kWebglcontextcreationerror, error));
}

const char* WebGLContextFactory::GetContextName() const {
  if (is_webgl2_) {
    return "WebGL2RenderingContext";
  }
  return "WebGLRenderingContext";
}

Platform::WebGLContextType WebGLContextFactory::GetContextType() const {
  if (is_webgl2_) {
    return Platform::kWebGL2ContextType;
  }
  return Platform::kWebGL1ContextType;
}

}  // namespace blink
