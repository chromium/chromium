/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/inspector/inspector_history.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

class UndoableStateMark final : public InspectorHistory::Action {
 public:
  UndoableStateMark() : InspectorHistory::Action("[UndoableState]") {}

  bool Perform(ExceptionState&) override { return true; }

  bool Undo(ExceptionState&) override { return true; }

  bool Redo(ExceptionState&) override { return true; }

  bool IsUndoableStateMark() override { return true; }
};

}  // namespace

InspectorHistory::Action::Action(const String& name) : name_(name) {}

InspectorHistory::Action::~Action() = default;

void InspectorHistory::Action::Trace(Visitor* visitor) const {}

String InspectorHistory::Action::ToString() {
  return name_;
}

bool InspectorHistory::Action::IsUndoableStateMark() {
  return false;
}

String InspectorHistory::Action::MergeId() {
  return "";
}

void InspectorHistory::Action::Merge(Action*) {}

InspectorHistory::InspectorHistory() : after_last_action_index_(0) {}

bool InspectorHistory::Perform(Action* action,
                               ExceptionState& exception_state) {
  if (!action->Perform(exception_state))
    return false;
  AppendPerformedAction(action);
  return true;
}

void InspectorHistory::AppendPerformedAction(Action* action) {
  if (!action->MergeId().empty() && after_last_action_index_ > 0 &&
      action->MergeId() == history_[after_last_action_index_ - 1]->MergeId()) {
    history_[after_last_action_index_ - 1]->Merge(action);
    if (history_[after_last_action_index_ - 1]->IsNoop())
      --after_last_action_index_;
    history_.resize(after_last_action_index_);
  } else {
    history_.resize(after_last_action_index_);
    history_.push_back(action);
    ++after_last_action_index_;
  }
}

void InspectorHistory::MarkUndoableState() {
  Perform(MakeGarbageCollected<UndoableStateMark>(),
          IGNORE_EXCEPTION_FOR_TESTING);
}

bool InspectorHistory::Undo(ExceptionState& exception_state) {
  while (after_last_action_index_ > 0 &&
         history_[after_last_action_index_ - 1]->IsUndoableStateMark())
    --after_last_action_index_;

  while (after_last_action_index_ > 0) {
    Action* action = history_[after_last_action_index_ - 1].Get();
    if (!action->Undo(exception_state)) {
      Reset();
      return false;
    }
    --after_last_action_index_;
    if (action->IsUndoableStateMark())
      break;
  }

  return true;
}

bool InspectorHistory::Redo(ExceptionState& exception_state) {
  while (after_last_action_index_ < history_.size() &&
         history_[after_last_action_index_]->IsUndoableStateMark())
    ++after_last_action_index_;

  while (after_last_action_index_ < history_.size()) {
    Action* action = history_[after_last_action_index_].Get();
    if (!action->Redo(exception_state)) {
      Reset();
      return false;
    }
    ++after_last_action_index_;
    if (action->IsUndoableStateMark())
      break;
  }
  return true;
}

void InspectorHistory::Reset() {
  after_last_action_index_ = 0;
  history_.clear();
}

void InspectorHistory::Trace(Visitor* visitor) const {
  visitor->Trace(history_);
}

}  // namespace blink
