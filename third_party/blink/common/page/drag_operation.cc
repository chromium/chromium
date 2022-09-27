// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/drag_operation.h"

#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"

namespace blink {

using ::ui::mojom::DragOperation;

// Ensure that the blink::DragOperationsMask enum values stay in sync with
// ui::mojom::DragOperation.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "enum mismatch: " #a)
STATIC_ASSERT_ENUM(kDragOperationNone, DragOperation::kNone);
STATIC_ASSERT_ENUM(kDragOperationCopy, DragOperation::kCopy);
STATIC_ASSERT_ENUM(kDragOperationLink, DragOperation::kLink);
STATIC_ASSERT_ENUM(kDragOperationMove, DragOperation::kMove);

}  // namespace blink
