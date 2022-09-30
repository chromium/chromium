// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/stack_frame_mojom_traits.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"

namespace mojo {

bool StructTraits<
    extensions::mojom::StackFrameDataView,
    extensions::StackFrame>::Read(extensions::mojom::StackFrameDataView data,
                                  extensions::StackFrame* out) {
  out->line_number = data.line_number();
  out->column_number = data.column_number();
  return data.ReadSource(&out->source) && data.ReadFunction(&out->function);
}

}  // namespace mojo
