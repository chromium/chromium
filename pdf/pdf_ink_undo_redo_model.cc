// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_undo_redo_model.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/types/expected.h"
#include "pdf/pdf_ink_ids.h"

namespace chrome_pdf {

namespace {

bool IsAllowedInCommandsStack(IdType id) {
  return std::holds_alternative<InkStrokeId>(id) ||
         std::holds_alternative<InkTextId>(id);
}

}  // namespace

PdfInkUndoRedoModel::Commands::Commands() = default;

PdfInkUndoRedoModel::Commands::Commands(const Commands&) = default;

PdfInkUndoRedoModel::Commands& PdfInkUndoRedoModel::Commands::operator=(
    const Commands&) = default;

PdfInkUndoRedoModel::Commands::~Commands() = default;

PdfInkUndoRedoModel::PdfInkUndoRedoModel() = default;

PdfInkUndoRedoModel::~PdfInkUndoRedoModel() = default;

base::expected<std::optional<IdType>, std::monostate>
PdfInkUndoRedoModel::Start() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  if (has_started_) {
    return base::unexpected(std::monostate());
  }

  std::optional<IdType> lowest_discard;
  auto& commands = commands_stack_[stack_position_];
  if (stack_position_ < commands_stack_.size() - 1) {
    // Invariant 2.
    CHECK(!commands.adds.empty() || !commands.removes.empty());

    // Find the lowest add command to discard.
    for (size_t i = stack_position_; i < commands_stack_.size(); ++i) {
      const auto& current_commands = commands_stack_[i];
      if (!current_commands.adds.empty()) {
        lowest_discard = *current_commands.adds.begin();
        break;
      }
    }

    // Discard rest of stack.
    //
    // The vector capacity is never reduced when resizing to smaller size. Thus
    // references to `commands_stack_` elements are not invalidated and safe to
    // use.
    commands_stack_.resize(stack_position_ + 1);
  }

  // Safe to reuse `commands`, which references an element inside of
  // `commands_stack_`. See note above the resize() call regarding safety.
  commands = Commands();
  has_started_ = true;
  return lowest_discard;
}

bool PdfInkUndoRedoModel::Add(IdType id) {
  CHECK(!commands_stack_.empty());

  if (!has_started_) {
    return false;
  }

  if (!IsAllowedInCommandsStack(id)) {
    return false;  // Failed invariant 7.
  }

  // Check the last add in the commands stack to ensure IDs are added in
  // strictly increasing order.
  for (const Commands& commands : base::Reversed(commands_stack_)) {
    if (!commands.adds.empty()) {
      // Checking the last ID in the set is sufficient because IDs are added in
      // strictly increasing order.
      const IdType& last_id = *commands.adds.rbegin();
      // Compare underlying values, as the default variant operator compares
      // indices first.
      if (GetIdTypeValue(id) <= GetIdTypeValue(last_id)) {
        return false;
      }
      break;
    }
  }

  // Invariant 4 holds if invariant 6 holds.
  CHECK(!HasIdInRemoveCommands(id));
  commands_stack_.back().adds.insert(id);
  return true;
}

bool PdfInkUndoRedoModel::Finish() {
  CHECK(!commands_stack_.empty());

  if (!has_started_) {
    return false;
  }

  has_started_ = false;
  auto& commands = commands_stack_.back();
  if (!commands.adds.empty() || !commands.removes.empty()) {
    commands_stack_.push_back(Commands());
    ++stack_position_;
  }
  return true;
}

bool PdfInkUndoRedoModel::Remove(IdType id) {
  CHECK(!commands_stack_.empty());

  if (!has_started_) {
    return false;
  }

  if (HasIdInRemoveCommands(id)) {
    return false;  // Failed invariant 5.
  }

  if (IsAllowedInCommandsStack(id) && !HasIdInAddCommands(id)) {
    return false;  // Failed invariant 6.
  }

  commands_stack_.back().removes.insert(id);
  return true;
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Undo() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  if (has_started_ || stack_position_ == 0) {
    // Cannot undo while adding/removing or at the bottom of the stack.
    return Commands();
  }

  // Result is the reverse of the recorded commands.
  --stack_position_;
  const auto& recorded = commands_stack_[stack_position_];
  Commands result;
  result.adds = recorded.removes;
  result.removes = recorded.adds;
  return result;
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Redo() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  if (has_started_ || stack_position_ == commands_stack_.size() - 1) {
    // Cannot redo while adding/removing or at the top of the stack.
    return Commands();
  }

  // Result is the recorded commands as-is.
  return commands_stack_[stack_position_++];
}

bool PdfInkUndoRedoModel::HasIdInAddCommands(IdType id) const {
  for (const auto& commands : commands_stack_) {
    if (commands.adds.contains(id)) {
      return true;
    }
  }
  return false;
}

bool PdfInkUndoRedoModel::HasIdInRemoveCommands(IdType id) const {
  for (const auto& commands : commands_stack_) {
    if (commands.removes.contains(id)) {
      return true;
    }
  }
  return false;
}

}  // namespace chrome_pdf
