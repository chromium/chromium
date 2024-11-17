// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_message.h"

#include "base/notreached.h"

namespace blink {

namespace {
V8GPUCompilationMessageType::Enum FromDawnEnum(
    wgpu::CompilationMessageType type) {
  switch (type) {
    case wgpu::CompilationMessageType::Error:
      return V8GPUCompilationMessageType::Enum::kError;
    case wgpu::CompilationMessageType::Warning:
      return V8GPUCompilationMessageType::Enum::kWarning;
    case wgpu::CompilationMessageType::Info:
      return V8GPUCompilationMessageType::Enum::kInfo;
  }
  NOTREACHED();
}

}  // namespace

GPUCompilationMessage::GPUCompilationMessage(String message,
                                             wgpu::CompilationMessageType type,
                                             uint64_t line_num,
                                             uint64_t line_pos,
                                             uint64_t offset,
                                             uint64_t length)
    : message_(message),
      type_(FromDawnEnum(type)),
      line_num_(line_num),
      line_pos_(line_pos),
      offset_(offset),
      length_(length) {}

}  // namespace blink
