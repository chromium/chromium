// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/drag_mojom_traits.h"

#include "base/notreached.h"

namespace {

constexpr int allow_all = blink::kDragOperationCopy |
                          blink::kDragOperationLink | blink::kDragOperationMove;

}  // namespace

namespace mojo {

// static
blink::mojom::DragOperation
EnumTraits<blink::mojom::DragOperation, blink::DragOperation>::ToMojom(
    blink::DragOperation op) {
  switch (op) {
    case blink::kDragOperationNone:
      return blink::mojom::DragOperation::kNone;
    case blink::kDragOperationCopy:
      return blink::mojom::DragOperation::kCopy;
    case blink::kDragOperationLink:
      return blink::mojom::DragOperation::kLink;
    case blink::kDragOperationMove:
      return blink::mojom::DragOperation::kMove;
    default:
      // blink::kDragOperationEvery is not handled on purpose, as
      // DragOperation should only represent a single operation.
      NOTREACHED();
      return blink::mojom::DragOperation::kNone;
  }
}

// static
bool EnumTraits<blink::mojom::DragOperation, blink::DragOperation>::FromMojom(
    blink::mojom::DragOperation op,
    blink::DragOperation* out) {
  switch (op) {
    case blink::mojom::DragOperation::kNone:
      *out = blink::kDragOperationNone;
      return true;
    case blink::mojom::DragOperation::kCopy:
      *out = blink::kDragOperationCopy;
      return true;
    case blink::mojom::DragOperation::kLink:
      *out = blink::kDragOperationLink;
      return true;
    case blink::mojom::DragOperation::kMove:
      *out = blink::kDragOperationMove;
      return true;
  }
  NOTREACHED();
  return false;
}

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
