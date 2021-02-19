/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SUBTREE_LAYOUT_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SUBTREE_LAYOUT_SCOPE_H_

#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

// This is the way to mark a subtree as needing layout during layout,
// e.g. for the purposes of doing a multipass layout.
//
// It should only be used during layout. Outside of layout, you should
// just call layoutObject->setNeedsLayout() directly.
//
// It ensures that you don't accidentally mark part of the tree as
// needing layout and not actually lay it out.

namespace blink {

class LayoutObject;

class SubtreeLayoutScope {
  STACK_ALLOCATED();

 public:
  SubtreeLayoutScope(LayoutObject& root);
  ~SubtreeLayoutScope();

  void SetNeedsLayout(LayoutObject* descendant,
                      LayoutInvalidationReasonForTracing);
  void SetChildNeedsLayout(LayoutObject* descendant);

  LayoutObject& Root() { return root_; }
  void RecordObjectMarkedForLayout(LayoutObject*);

 private:
  LayoutObject& root_;

#if DCHECK_IS_ON()
  HashSet<LayoutObject*> layout_objects_to_layout_;
#endif
};

}  // namespace blink

#endif
