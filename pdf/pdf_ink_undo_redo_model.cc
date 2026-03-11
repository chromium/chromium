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
#include "base/notreached.h"
#include "base/types/strong_alias.h"

namespace chrome_pdf {

namespace {

PdfInkUndoRedoModel::AddCommands& GetModifiableAddCommands(
    PdfInkUndoRedoModel::Commands& commands) {
  return std::get<PdfInkUndoRedoModel::AddCommands>(commands);
}

PdfInkUndoRedoModel::RemoveCommands& GetModifiableRemoveCommands(
    PdfInkUndoRedoModel::Commands& commands) {
  return std::get<PdfInkUndoRedoModel::RemoveCommands>(commands);
}

}  // namespace

PdfInkUndoRedoModel::PdfInkUndoRedoModel() = default;

PdfInkUndoRedoModel::~PdfInkUndoRedoModel() = default;

std::optional<PdfInkUndoRedoModel::DiscardedAddCommands>
PdfInkUndoRedoModel::StartAdd() {
  return StartImpl<AddCommands>();
}

bool PdfInkUndoRedoModel::Add(InkStrokeId id) {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kAdd)) {
    // Can only add at top of the stack, and the entry there must be for adding.
    return false;
  }
  if (HasIdInAddCommands(id)) {
    return false;  // Failed invariant 3.
  }

  // Invariant 4 holds if invariant 6 holds.
  CHECK(!HasIdInRemoveCommands(id));
  GetModifiableAddCommands(commands_stack_.back())->insert(id);
  return true;
}

bool PdfInkUndoRedoModel::FinishAdd() {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kAdd)) {
    // Can only add at top of the stack, and the entry there must be for adding.
    return false;
  }

  auto& commands = commands_stack_.back();
  if (GetAddCommands(commands)->empty()) {
    commands = std::monostate();  // Reuse top of stack if empty.
  } else {
    // Otherwise push new item onto the stack.
    ++stack_position_;
    commands_stack_.push_back(std::monostate());
  }
  return true;
}

std::optional<PdfInkUndoRedoModel::DiscardedAddCommands>
PdfInkUndoRedoModel::StartRemove() {
  return StartImpl<RemoveCommands>();
}

bool PdfInkUndoRedoModel::Remove(IdType id) {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kRemove)) {
    // Can only remove at top of the stack, and the entry there must be for
    // erasing.
    return false;
  }

  if (HasIdInRemoveCommands(id)) {
    return false;  // Failed invariant 5.
  }

  if (std::holds_alternative<InkStrokeId>(id) && !HasIdInAddCommands(id)) {
    return false;  // Failed invariant 6.
  }

  GetModifiableRemoveCommands(commands_stack_.back())->insert(id);
  return true;
}

bool PdfInkUndoRedoModel::FinishRemove() {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kRemove)) {
    // Can only remove at top of the stack, and the entry there must be for
    // erasing.
    return false;
  }

  auto& commands = commands_stack_.back();
  if (GetRemoveCommands(commands)->empty()) {
    commands = std::monostate();  // Reuse top of stack if empty.
  } else {
    // Otherwise push new item onto the stack.
    ++stack_position_;
    commands_stack_.push_back(std::monostate());
  }
  return true;
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Undo() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  if (stack_position_ == 0) {
    return std::monostate();  // Already at bottom of stack.
  }

  // Result is reverse of the recorded commands.
  --stack_position_;
  const auto& commands = commands_stack_[stack_position_];
  switch (GetCommandsType(commands)) {
    case CommandsType::kNone: {
      NOTREACHED();  // Invariant 2.
    }
    case CommandsType::kAdd: {
      RemoveCommands result;
      auto add_commands = GetAddCommands(commands).value();
      result->insert(add_commands.begin(), add_commands.end());
      return result;
    }
    case CommandsType::kRemove: {
      AddCommands result;
      auto remove_commands = GetRemoveCommands(commands).value();
      result->insert(remove_commands.begin(), remove_commands.end());
      return result;
    }
  }
  NOTREACHED();
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Redo() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  if (stack_position_ == commands_stack_.size() - 1) {
    return std::monostate();  // Already at top of stack.
  }

  // Result is the recorded commands as-is.
  const auto& commands = commands_stack_[stack_position_];
  ++stack_position_;
  switch (GetCommandsType(commands)) {
    case CommandsType::kNone: {
      NOTREACHED();  // Invariant 2.
    }
    case CommandsType::kAdd: {
      return GetAddCommands(commands);
    }
    case CommandsType::kRemove: {
      return GetRemoveCommands(commands);
    }
  }
  NOTREACHED();
}

// static
PdfInkUndoRedoModel::CommandsType PdfInkUndoRedoModel::GetCommandsType(
    const Commands& commands) {
  if (std::holds_alternative<std::monostate>(commands)) {
    return CommandsType::kNone;
  }
  if (std::holds_alternative<AddCommands>(commands)) {
    return CommandsType::kAdd;
  }
  CHECK(std::holds_alternative<RemoveCommands>(commands));
  return CommandsType::kRemove;
}

// static
const PdfInkUndoRedoModel::AddCommands& PdfInkUndoRedoModel::GetAddCommands(
    const Commands& commands) {
  return std::get<AddCommands>(commands);
}

// static
const PdfInkUndoRedoModel::RemoveCommands&
PdfInkUndoRedoModel::GetRemoveCommands(const Commands& commands) {
  return std::get<RemoveCommands>(commands);
}

template <typename T>
std::optional<PdfInkUndoRedoModel::DiscardedAddCommands>
PdfInkUndoRedoModel::StartImpl() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  DiscardedAddCommands discarded_commands;
  auto& commands = commands_stack_[stack_position_];
  const bool has_commands = GetCommandsType(commands) != CommandsType::kNone;
  if (stack_position_ == commands_stack_.size() - 1) {
    if (has_commands) {
      // Cannot start when adding/removing already started.
      return std::nullopt;
    }
  } else {
    CHECK(has_commands);  // Invariant 2.

    // Record the add commands to discard.
    for (size_t i = stack_position_; i < commands_stack_.size(); ++i) {
      if (GetCommandsType(commands_stack_[i]) == CommandsType::kAdd) {
        for (IdType id : GetAddCommands(commands_stack_[i]).value()) {
          bool inserted =
              discarded_commands.insert(std::get<InkStrokeId>(id)).second;
          CHECK(inserted);
        }
      }
    }

    // Discard rest of stack.
    //
    // The vector capacity is never reduced when resizing to smaller size. Thus
    // references to `commands_stack_` elements are not invalidated and safe to
    // use.
    commands_stack_.resize(stack_position_ + 1);
  }

  // Set the top of the stack to the appropriate command type.
  //
  // Safe to reuse `commands`, which references an element inside of
  // `commands_stack_`. See note above the resize() call regarding safety.
  commands = T();
  return discarded_commands;
}

bool PdfInkUndoRedoModel::IsAtTopOfStackWithGivenCommandType(
    CommandsType type) const {
  if (stack_position_ != commands_stack_.size() - 1) {
    return false;
  }
  return GetCommandsType(commands_stack_.back()) == type;
}

bool PdfInkUndoRedoModel::HasIdInAddCommands(IdType id) const {
  for (const auto& commands : commands_stack_) {
    if (GetCommandsType(commands) == CommandsType::kAdd &&
        GetAddCommands(commands)->contains(id)) {
      return true;
    }
  }
  return false;
}

bool PdfInkUndoRedoModel::HasIdInRemoveCommands(IdType id) const {
  for (const auto& commands : commands_stack_) {
    if (GetCommandsType(commands) == CommandsType::kRemove &&
        GetRemoveCommands(commands)->contains(id)) {
      return true;
    }
  }
  return false;
}

}  // namespace chrome_pdf
