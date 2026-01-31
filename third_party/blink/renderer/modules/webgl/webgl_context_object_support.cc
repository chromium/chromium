// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_context_object_support.h"

#include "base/task/single_thread_task_runner.h"

namespace blink {

scoped_refptr<base::SingleThreadTaskRunner>
WebGLContextObjectSupport::GetContextTaskRunner() {
  return task_runner_;
}

void WebGLContextObjectSupport::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

WebGLContextObjectSupport::WebGLContextObjectSupport(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool is_webgl2)
    : task_runner_(std::move(task_runner)), is_webgl2_(is_webgl2) {}

void WebGLContextObjectSupport::OnContextLost() {
  DCHECK(!is_lost_);
  number_of_context_losses_++;
  is_lost_ = true;
  gles2_interface_ = nullptr;
  extensions_enabled_.reset();
}

void WebGLContextObjectSupport::OnContextRestored(
    gpu::gles2::GLES2Interface* gl) {
  DCHECK(is_lost_);
  is_lost_ = false;
  gles2_interface_ = gl;
}

void WebGLContextObjectSupport::MarkExtensionEnabled(WebGLExtensionName name) {
  extensions_enabled_.set(name);
}

}  // namespace blink
