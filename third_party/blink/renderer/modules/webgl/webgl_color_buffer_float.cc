/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_color_buffer_float.h"

namespace blink {

WebGLColorBufferFloat::WebGLColorBufferFloat(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  // https://github.com/KhronosGroup/WebGL/pull/2830
  // Spec requires EXT_float_blend needs to be turned on implicitly here
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_float_blend");
}

WebGLExtensionName WebGLColorBufferFloat::GetName() const {
  return kWebGLColorBufferFloatName;
}

WebGLColorBufferFloat* WebGLColorBufferFloat::Create(
    WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<WebGLColorBufferFloat>(context);
}

bool WebGLColorBufferFloat::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_OES_texture_float") &&
         context->ExtensionsUtil()->SupportsExtension(
             "GL_CHROMIUM_color_buffer_float_rgba");
}

const char* WebGLColorBufferFloat::ExtensionName() {
  return "WEBGL_color_buffer_float";
}

}  // namespace blink
