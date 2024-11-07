// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_undo_redo_model.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace chrome_pdf {

namespace {

PdfInkUndoRedoModel::DrawStrokeCommands& GetModifiableDrawStrokeCommands(
    PdfInkUndoRedoModel::Commands& commands) {
  return absl::get<PdfInkUndoRedoModel::DrawStrokeCommands>(commands);
}

PdfInkUndoRedoModel::EraseStrokeCommands& GetModifiableEraseStrokeCommands(
    PdfInkUndoRedoModel::Commands& commands) {
  return absl::get<PdfInkUndoRedoModel::EraseStrokeCommands>(commands);
}

}  // namespace

PdfInkUndoRedoModel::PdfInkUndoRedoModel() = default;

PdfInkUndoRedoModel::~PdfInkUndoRedoModel() = default;

std::optional<PdfInkUndoRedoModel::DiscardedDrawStrokeCommands>
PdfInkUndoRedoModel::StartDrawStroke() {
  return StartImpl<DrawStrokeCommands>();
}

bool PdfInkUndoRedoModel::DrawStroke(InkStrokeId id) {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kDrawStroke)) {
    // Can only draw at top of the stack, and the entry there must be for
    // drawing.
    return false;
  }
  if (HasIdInDrawStrokeCommands(id)) {
    return false;  // Failed invariant 3.
  }

  // Invariant 4 holds if invariant 6 holds.
  CHECK(!HasIdInEraseStrokeCommands(id));
  GetModifiableDrawStrokeCommands(commands_stack_.back())->insert(id);
  return true;
}

bool PdfInkUndoRedoModel::FinishDrawStroke() {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kDrawStroke)) {
    // Can only draw at top of the stack, and the entry there must be for
    // drawing.
    return false;
  }

  auto& commands = commands_stack_.back();
  if (GetDrawStrokeCommands(commands)->empty()) {
    commands = absl::monostate();  // Reuse top of stack if empty.
  } else {
    // Otherwise push new item onto the stack.
    ++stack_position_;
    commands_stack_.push_back(absl::monostate());
  }
  return true;
}

std::optional<PdfInkUndoRedoModel::DiscardedDrawStrokeCommands>
PdfInkUndoRedoModel::StartEraseStroke() {
  return StartImpl<EraseStrokeCommands>();
}

bool PdfInkUndoRedoModel::EraseStroke(InkStrokeId id) {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kEraseStroke)) {
    // Can only erase at top of the stack, and the entry there must be for
    // erasing.
    return false;
  }
  if (!HasIdInDrawStrokeCommands(id)) {
    return false;  // Failed invariant 6.
  }
  if (HasIdInEraseStrokeCommands(id)) {
    return false;  // Failed invariant 5.
  }

  GetModifiableEraseStrokeCommands(commands_stack_.back())->insert(id);
  return true;
}

bool PdfInkUndoRedoModel::FinishEraseStroke() {
  CHECK(!commands_stack_.empty());

  if (!IsAtTopOfStackWithGivenCommandType(CommandsType::kEraseStroke)) {
    // Can only erase at top of the stack, and the entry there must be for
    // erasing.
    return false;
  }

  auto& commands = commands_stack_.back();
  if (GetEraseStrokeCommands(commands)->empty()) {
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
      NOTREACHED();  // Invariant 2.
    }
    case CommandsType::kDrawStroke: {
      EraseStrokeCommands result;
      for (InkStrokeId id : GetDrawStrokeCommands(commands).value()) {
        result->insert(id);
      }
      return result;
    }
    case CommandsType::kEraseStroke: {
      DrawStrokeCommands result;
      for (InkStrokeId id : GetEraseStrokeCommands(commands).value()) {
        result->insert(id);
      }
      return result;
    }
  }
  NOTREACHED();
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
      NOTREACHED();  // Invariant 2.
    }
    case CommandsType::kDrawStroke: {
      return GetDrawStrokeCommands(commands);
    }
    case CommandsType::kEraseStroke: {
      return GetEraseStrokeCommands(commands);
    }
  }
  NOTREACHED();
}

// static
PdfInkUndoRedoModel::CommandsType PdfInkUndoRedoModel::GetCommandsType(
    const Commands& commands) {
  if (absl::holds_alternative<absl::monostate>(commands)) {
    return CommandsType::kNone;
  }
  if (absl::holds_alternative<DrawStrokeCommands>(commands)) {
    return CommandsType::kDrawStroke;
  }
  CHECK(absl::holds_alternative<EraseStrokeCommands>(commands));
  return CommandsType::kEraseStroke;
}

// static
const PdfInkUndoRedoModel::DrawStrokeCommands&
PdfInkUndoRedoModel::GetDrawStrokeCommands(const Commands& commands) {
  return absl::get<DrawStrokeCommands>(commands);
}

// static
const PdfInkUndoRedoModel::EraseStrokeCommands&
PdfInkUndoRedoModel::GetEraseStrokeCommands(const Commands& commands) {
  return absl::get<EraseStrokeCommands>(commands);
}

template <typename T>
std::optional<PdfInkUndoRedoModel::DiscardedDrawStrokeCommands>
PdfInkUndoRedoModel::StartImpl() {
  CHECK(!commands_stack_.empty());
  CHECK_LT(stack_position_, commands_stack_.size());

  DiscardedDrawStrokeCommands discarded_commands;
  auto& commands = commands_stack_[stack_position_];
  const bool has_commands = GetCommandsType(commands) != CommandsType::kNone;
  if (stack_position_ == commands_stack_.size() - 1) {
    if (has_commands) {
      // Cannot start when drawing/erasing already started.
      return std::nullopt;
    }
  } else {
    CHECK(has_commands);  // Invariant 2.

    // Record the draw commands to discard.
    for (size_t i = stack_position_; i < commands_stack_.size(); ++i) {
      if (GetCommandsType(commands_stack_[i]) == CommandsType::kDrawStroke) {
        for (InkStrokeId id :
             GetDrawStrokeCommands(commands_stack_[i]).value()) {
          bool inserted = discarded_commands.insert(id).second;
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

bool PdfInkUndoRedoModel::HasIdInDrawStrokeCommands(InkStrokeId id) const {
  for (const auto& commands : commands_stack_) {
    if (GetCommandsType(commands) == CommandsType::kDrawStroke &&
        GetDrawStrokeCommands(commands)->contains(id)) {
      return true;
    }
  }
  return false;
}

bool PdfInkUndoRedoModel::HasIdInEraseStrokeCommands(InkStrokeId id) const {
  for (const auto& commands : commands_stack_) {
    if (GetCommandsType(commands) == CommandsType::kEraseStroke &&
        GetEraseStrokeCommands(commands)->contains(id)) {
      return true;
    }
  }
  return false;
}

}  // namespace chrome_pdf
