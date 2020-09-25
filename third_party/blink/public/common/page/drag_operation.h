/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_DRAG_OPERATION_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_DRAG_OPERATION_H_

#include <limits.h>

namespace blink {

// "Verb" of a drag-and-drop operation as negotiated between the source and
// destination.
// (These constants match their equivalents in WebCore's DragActions.h and
// should not be renumbered.)
// TODO(hferreiro): replace this enum and the corresponding one in
// drag_actions.h with blink::mojom::DragOperation.
enum DragOperation {
  kDragOperationNone = 0,
  kDragOperationCopy = 1,
  kDragOperationLink = 2,
  kDragOperationGeneric = 4,
  kDragOperationPrivate = 8,
  kDragOperationMove = 16,
  kDragOperationDelete = 32,
  kDragOperationEvery = UINT_MAX
};

// Alternate typedef to make it clear when this is being used as a mask
// with potentially multiple value bits set.
typedef DragOperation DragOperationsMask;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_DRAG_OPERATION_H_
