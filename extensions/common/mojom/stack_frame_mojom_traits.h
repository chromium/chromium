// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_STACK_FRAME_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_STACK_FRAME_MOJOM_TRAITS_H_

#include <string>

#include "extensions/common/mojom/stack_frame.mojom-shared.h"
#include "extensions/common/stack_frame.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::StackFrameDataView,
                    extensions::StackFrame> {
  static uint32_t line_number(const extensions::StackFrame& frame) {
    return frame.line_number;
  }

  static uint32_t column_number(const extensions::StackFrame& frame) {
    return frame.column_number;
  }

  static const std::u16string& source(const extensions::StackFrame& frame) {
    return frame.source;
  }

  static const std::u16string& function(const extensions::StackFrame& frame) {
    return frame.function;
  }

  static bool Read(extensions::mojom::StackFrameDataView data,
                   extensions::StackFrame* out);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_STACK_FRAME_MOJOM_TRAITS_H_
