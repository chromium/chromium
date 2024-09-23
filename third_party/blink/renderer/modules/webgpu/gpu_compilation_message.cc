// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_message.h"

#include "base/notreached.h"

namespace blink {

GPUCompilationMessage::GPUCompilationMessage(String message,
                                             wgpu::CompilationMessageType type,
                                             uint64_t line_num,
                                             uint64_t line_pos,
                                             uint64_t offset,
                                             uint64_t length)
    : message_(message),
      line_num_(line_num),
      line_pos_(line_pos),
      offset_(offset),
      length_(length) {
  switch (type) {
    case wgpu::CompilationMessageType::Error:
      type_string_ = "error";
      break;
    case wgpu::CompilationMessageType::Warning:
      type_string_ = "warning";
      break;
    case wgpu::CompilationMessageType::Info:
      type_string_ = "info";
      break;
  }
}

}  // namespace blink
