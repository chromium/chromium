// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_undo_redo_model.h"

#include <stddef.h>

#include <set>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace chrome_pdf {

namespace {

PdfInkUndoRedoModel::DrawCommands& GetModifiableDrawCommands(
    PdfInkUndoRedoModel::Commands& commands) {
  return absl::get<PdfInkUndoRedoModel::DrawCommands>(commands);
}

PdfInkUndoRedoModel::EraseCommands& GetModifiableEraseCommands(
    PdfInkUndoRedoModel::Commands& commands) {
  return absl::get<PdfInkUndoRedoModel::EraseCommands>(commands);
}

}  // namespace

PdfInkUndoRedoModel::PdfInkUndoRedoModel() = default;

PdfInkUndoRedoModel::~PdfInkUndoRedoModel() = default;

bool PdfInkUndoRedoModel::StartDraw() {
  return StartImpl<DrawCommands>();
}

bool PdfInkUndoRedoModel::Draw(size_t id) {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kDraw)) {
    // Can only draw at top of the stack, and the entry there must be for
    // drawing.
    return false;
  }
  if (HasIdInDrawCommands(id)) {
    return false;  // Failed invariant 3.
  }

  // Invariant 4 holds if invariant 6 holds.
  CHECK(!HasIdInEraseCommands(id));
  GetModifiableDrawCommands(commands_stack_.back())->insert(id);
  return true;
}

bool PdfInkUndoRedoModel::FinishDraw() {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kDraw)) {
    // Can only draw at top of the stack, and the entry there must be for
    // drawing.
    return false;
  }

  auto& commands = commands_stack_.back();
  if (GetDrawCommands(commands)->empty()) {
    commands = absl::monostate();  // Reuse top of stack if empty.
  } else {
    // Otherwise push new item onto the stack.
    ++stack_position_;
    commands_stack_.push_back(absl::monostate());
  }
  return true;
}

bool PdfInkUndoRedoModel::StartErase() {
  return StartImpl<EraseCommands>();
}

bool PdfInkUndoRedoModel::Erase(size_t id) {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kErase)) {
    // Can only erase at top of the stack, and the entry there must be for
    // erasing.
    return false;
  }
  if (!HasIdInDrawCommands(id)) {
    return false;  // Failed invariant 6.
  }
  if (HasIdInEraseCommands(id)) {
    return false;  // Failed invariant 5.
  }

  GetModifiableEraseCommands(commands_stack_.back())->insert(id);
  return true;
}

bool PdfInkUndoRedoModel::FinishErase() {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kErase)) {
    // Can only erase at top of the stack, and the entry there must be for
    // erasing.
    return false;
  }

  auto& commands = commands_stack_.back();
  if (GetEraseCommands(commands)->empty()) {
    commands = absl::monostate();  // Reuse top of stack if empty.
  } else {
    // Otherwise push new item onto the stack.
    ++stack_position_;
    commands_stack_.push_back(absl::monostate());
  }
  return true;
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Undo() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  if (stack_position_ == 0) {
    return absl::monostate();  // Already at bottom of stack.
  }

  // Result is reverse of the recorded commands.
  --stack_position_;
  const auto& commands = commands_stack_[stack_position_];
  switch (GetCommandsType(commands)) {
    case CommandsType::kNone: {
      NOTREACHED_NORETURN();  // Invariant 2.
    }
    case CommandsType::kDraw: {
      EraseCommands result;
      for (size_t id : GetDrawCommands(commands).value()) {
        result->insert(id);
      }
      return result;
    }
    case CommandsType::kErase: {
      DrawCommands result;
      for (size_t id : GetEraseCommands(commands).value()) {
        result->insert(id);
      }
      return result;
    }
  }
}

PdfInkUndoRedoModel::Commands PdfInkUndoRedoModel::Redo() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  if (stack_position_ == commands_stack_.size() - 1) {
    return absl::monostate();  // Already at top of stack.
  }

  // Result is the recorded commands as-is.
  const auto& commands = commands_stack_[stack_position_];
  ++stack_position_;
  switch (GetCommandsType(commands)) {
    case CommandsType::kNone: {
      NOTREACHED_NORETURN();  // Invariant 2.
    }
    case CommandsType::kDraw: {
      return GetDrawCommands(commands);
    }
    case CommandsType::kErase: {
      return GetEraseCommands(commands);
    }
  }
}

// static
PdfInkUndoRedoModel::CommandsType PdfInkUndoRedoModel::GetCommandsType(
    const Commands& commands) {
  if (absl::holds_alternative<absl::monostate>(commands)) {
    return CommandsType::kNone;
  }
  if (absl::holds_alternative<DrawCommands>(commands)) {
    return CommandsType::kDraw;
  }
  CHECK(absl::holds_alternative<EraseCommands>(commands));
  return CommandsType::kErase;
}

// static
const PdfInkUndoRedoModel::DrawCommands& PdfInkUndoRedoModel::GetDrawCommands(
    const Commands& commands) {
  return absl::get<DrawCommands>(commands);
}

// static
const PdfInkUndoRedoModel::EraseCommands& PdfInkUndoRedoModel::GetEraseCommands(
    const Commands& commands) {
  return absl::get<EraseCommands>(commands);
}

template <typename T>
bool PdfInkUndoRedoModel::StartImpl() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  auto& commands = commands_stack_[stack_position_];
  const bool has_commands = GetCommandsType(commands) != CommandsType::kNone;
  if (stack_position_ == commands_stack_.size() - 1) {
    if (has_commands) {
      return false;  // Cannot start when drawing/erasing already started.
    }
  } else {
    CHECK(has_commands);                          // Invariant 2.
    commands_stack_.resize(stack_position_ + 1);  // Discard rest of stack.
  }

  commands = T();  // Set the top of the stack to the appropriate command type.
  return true;
}

bool PdfInkUndoRedoModel::IsAtTopOfStackWithGivenCommandType(
    CommandsType type) const {
  if (stack_position_ != commands_stack_.size() - 1) {
    return false;
  }
  return GetCommandsType(commands_stack_.back()) == type;
}

bool PdfInkUndoRedoModel::HasIdInDrawCommands(size_t id) const {
  for (const auto& commands : commands_stack_) {
    if (GetCommandsType(commands) == CommandsType::kDraw &&
        GetDrawCommands(commands)->contains(id)) {
      return true;
    }
  }
  return false;
}

bool PdfInkUndoRedoModel::HasIdInEraseCommands(size_t id) const {
  for (const auto& commands : commands_stack_) {
    if (GetCommandsType(commands) == CommandsType::kErase &&
        GetEraseCommands(commands)->contains(id)) {
      return true;
    }
  }
  return false;
}

}  // namespace chrome_pdf
