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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_UNDO_STACK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_UNDO_STACK_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class LocalFrame;
class UndoStep;

// |UndoStack| is owned by and always 1:1 to |Editor|. Since |Editor| is 1:1 to
// |LocalFrame|, |UndoStack| is also 1:1 to |LocalFrame|.
class UndoStack final : public GarbageCollected<UndoStack> {
  using UndoStepStack = HeapDeque<Member<UndoStep>>;

 public:
  UndoStack();

  void RegisterUndoStep(UndoStep*);
  void RegisterRedoStep(UndoStep*);
  bool CanUndo() const;
  bool CanRedo() const;
  void Undo();
  void Redo();
  void Clear();

  class UndoStepRange {
    STACK_ALLOCATED();

   public:
    using ConstIterator = UndoStepStack::const_reverse_iterator;
    ConstIterator begin() { return step_stack_.rbegin(); }
    ConstIterator end() { return step_stack_.rend(); }

    explicit UndoStepRange(const UndoStepStack&);

   private:
    const UndoStepStack& step_stack_;
  };

  UndoStepRange UndoSteps() const;

  void Trace(Visitor*);

 private:
  bool in_redo_;
  UndoStepStack undo_stack_;
  UndoStepStack redo_stack_;

  DISALLOW_COPY_AND_ASSIGN(UndoStack);
};

}  // namespace blink

#endif
