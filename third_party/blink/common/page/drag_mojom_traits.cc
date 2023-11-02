// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/drag_mojom_traits.h"

namespace {

constexpr int allow_all = blink::kDragOperationCopy |
                          blink::kDragOperationLink | blink::kDragOperationMove;

}  // namespace

namespace mojo {

// static
bool StructTraits<blink::mojom::AllowedDragOperationsDataView,
                  blink::DragOperationsMask>::
    Read(blink::mojom::AllowedDragOperationsDataView data,
         blink::DragOperationsMask* out) {
  int op_mask = blink::kDragOperationNone;
  if (data.allow_copy())
    op_mask |= blink::kDragOperationCopy;
  if (data.allow_link())
    op_mask |= blink::kDragOperationLink;
  if (data.allow_move())
    op_mask |= blink::kDragOperationMove;
  if (op_mask == allow_all)
    op_mask = blink::kDragOperationEvery;
  *out = static_cast<blink::DragOperationsMask>(op_mask);
  return true;
}

}  // namespace mojo
