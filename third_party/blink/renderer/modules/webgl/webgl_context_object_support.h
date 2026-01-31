// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_CONTEXT_OBJECT_SUPPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_CONTEXT_OBJECT_SUPPORT_H_

#include <bitset>

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension_name.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl31.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu::gles2 {
class GLES2Interface;
}

namespace blink {

// Base classes for WebGL rendering contexts that contains all the information
// needed by WebGLObject and child classes. This is used to avoid a dependency
// on WebGLRenderingContextBase and keep logic and effects more local.
class MODULES_EXPORT WebGLContextObjectSupport : public ScriptWrappable {
 public:
  // The GLES2Interface to use for the underlying GL context, it is nullptr when
  // the context is lost.
  gpu::gles2::GLES2Interface* ContextGL() const { return gles2_interface_; }

  bool IsWebGL2() const { return is_webgl2_; }
  bool IsLost() const { return is_lost_; }

  // How many context losses there were, to check whether a WebGLObject was
  // created since the last context resoration or before that (and hence invalid
  // to use).
  uint32_t NumberOfContextLosses() const { return number_of_context_losses_; }

  bool ExtensionEnabled(WebGLExtensionName name) const {
    return extensions_enabled_.test(name);
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetContextTaskRunner();
  void Trace(Visitor*) const override;

 protected:
  WebGLContextObjectSupport(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool is_webgl2);

  // Must be called when the WebGL context is lost.
  void OnContextLost();
  // Must be called when the WebGL context is created or restored, also sets up
  // the GLES2Interface that will be used for the duration that this context is
  // active.
  void OnContextRestored(gpu::gles2::GLES2Interface*);

  void MarkExtensionEnabled(WebGLExtensionName name);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::bitset<kWebGLExtensionNameCount> extensions_enabled_ = {};
  raw_ptr<gpu::gles2::GLES2Interface> gles2_interface_ = nullptr;

  uint32_t number_of_context_losses_ = 0;
  bool is_lost_ = true;
  bool is_webgl2_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_CONTEXT_OBJECT_SUPPORT_H_
