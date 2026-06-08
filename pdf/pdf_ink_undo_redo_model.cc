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
#include "base/containers/adapters.h"
#include "base/containers/span.h"
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
  if (has_started_) {
    return base::unexpected(std::monostate());
  }

  std::optional<IdType> lowest_discard;
  if (stack_position_ < commands_stack_.size()) {
    // Find the lowest add command to discard.
    for (size_t i = stack_position_; i < commands_stack_.size(); ++i) {
      const auto& current_commands = commands_stack_[i];
      if (!current_commands.adds.empty()) {
        lowest_discard = *current_commands.adds.begin();
        break;
      }
    }

    // Discard rest of stack.
    commands_stack_.resize(stack_position_);
  }

  commands_stack_.push_back(Commands());
  has_started_ = true;
  return lowest_discard;
}

bool PdfInkUndoRedoModel::Add(IdType id) {
  if (!has_started_) {
    return false;
  }

  CHECK(!commands_stack_.empty());

  if (!IsAllowedInCommandsStack(id)) {
    return false;  // Failed invariant 5.
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

  // Invariant 2 holds if invariant 4 holds.
  CHECK(!HasIdInRemoveCommands(id));
  commands_stack_.back().adds.insert(id);
  return true;
}

bool PdfInkUndoRedoModel::Remove(IdType id) {
  if (!has_started_) {
    return false;
  }

  CHECK(!commands_stack_.empty());

  if (HasIdInRemoveCommands(id)) {
    return false;  // Failed invariant 3.
  }

  if (!HasIdInPreviousAddCommands(id)) {
    if (std::holds_alternative<InkStrokeId>(id) ||
        std::holds_alternative<InkTextId>(id)) {
      return false;  // Failed invariant 4.
    }
  }

  commands_stack_.back().removes.insert(id);
  return true;
}


bool PdfInkUndoRedoModel::Finish() {
  if (!has_started_) {
    return false;
  }

  CHECK(!commands_stack_.empty());

  has_started_ = false;
  auto& commands = commands_stack_.back();
  if (!commands.adds.empty() || !commands.removes.empty()) {
    ++stack_position_;
  } else {
    commands_stack_.pop_back();
  }
  return true;
}

std::optional<TextId> PdfInkUndoRedoModel::GetUndoTextId() const {
  if (commands_stack_.empty() || stack_position_ == 0) {
    return std::nullopt;
  }

  const auto& recorded = commands_stack_[stack_position_ - 1];
  if (recorded.removes.size() != 1 || recorded.adds.size() > 1) {
    return std::nullopt;
  }

  const IdType& id = *recorded.removes.begin();
  if (std::holds_alternative<InkTextId>(id)) {
    return std::get<InkTextId>(id);
  }
  if (std::holds_alternative<InkLoadedTextId>(id)) {
    return std::get<InkLoadedTextId>(id);
  }
  return std::nullopt;
}

std::optional<InkTextId> PdfInkUndoRedoModel::GetRedoInkTextId() const {
  if (commands_stack_.empty() || stack_position_ == commands_stack_.size()) {
    return std::nullopt;
  }

  const auto& recorded = commands_stack_[stack_position_];
  if (recorded.adds.size() != 1 || recorded.removes.size() > 1) {
    return std::nullopt;
  }

  const IdType& id = *recorded.adds.begin();
  if (!std::holds_alternative<InkTextId>(id)) {
    return std::nullopt;
  }
  return std::get<InkTextId>(id);
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Undo() {
  if (has_started_ || stack_position_ == 0) {
    // Cannot undo while adding/removing or at the bottom of the stack.
    return Commands();
  }

  CHECK(!commands_stack_.empty());

  // Result is the reverse of the recorded commands.
  --stack_position_;
  const auto& recorded = commands_stack_[stack_position_];
  Commands result;
  result.adds = recorded.removes;
  result.removes = recorded.adds;
  return result;
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Redo() {
  if (has_started_ || stack_position_ == commands_stack_.size()) {
    // Cannot redo while adding/removing or at the top of the stack.
    return Commands();
  }

  CHECK(!commands_stack_.empty());

  // Result is the recorded commands as-is.
  return commands_stack_[stack_position_++];
}

bool PdfInkUndoRedoModel::HasIdInPreviousAddCommands(IdType id) const {
  if (commands_stack_.back().adds.contains(id)) {
    // Fails invariant 4. The ID must exist in a different `Commands` in the
    // stack, so it cannot be in the currently active one.
    return false;
  }

  for (const auto& commands :
       base::span(commands_stack_).first(commands_stack_.size() - 1)) {
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
