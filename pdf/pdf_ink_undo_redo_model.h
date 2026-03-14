// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_UNDO_REDO_MODEL_H_
#define PDF_PDF_INK_UNDO_REDO_MODEL_H_

#include <stddef.h>

#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_ink_ids.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// Models add and remove commands. Based on the recorded commands, processes
// undo / redo requests and calculates what commands need to be applied.
class PdfInkUndoRedoModel {
 public:
  enum class CommandsType {
    kNone,
    kAdd,
    kRemove,
  };

  using IdSet = std::set<IdType, IdTypeComparator>;
  using AddCommands = base::StrongAlias<class AddCommandsTag, IdSet>;
  using RemoveCommands = base::StrongAlias<class RemoveCommandsTag, IdSet>;

  using Commands = std::variant<std::monostate, AddCommands, RemoveCommands>;

  PdfInkUndoRedoModel();
  PdfInkUndoRedoModel(const PdfInkUndoRedoModel&) = delete;
  PdfInkUndoRedoModel& operator=(const PdfInkUndoRedoModel&) = delete;
  ~PdfInkUndoRedoModel();

  // For all Add / Remove methods:
  // - The expected usage is: 1 StartOp call, any number of Op(Variant) calls,
  //   1 FinishOp call.
  // - StartOp returns the lowest annotation ID among added elements to discard,
  //   or nullopt if there are no elements to discard on success. Returns
  //   `std::monostate` if any requirements are not met.
  // - Op(Variant) and FinishOp return true on success. Return false if any
  //   requirements are not met.
  // - Must not return `std::monostate` or false in production code. Returning
  //   `std::monostate` or false is only allowed in tests to check failure modes
  //   without resorting to death tests.

  // Starts recording add commands. If the current commands stack position is
  // not at the top of the stack, then this discards all entries from the
  // current position to the top of the stack. Returns the lowest annotation ID
  // among added elements to discard. Since IDs are added in increasing order,
  // all elements with the same ID or larger IDs can be discarded. This does not
  // return `InkModeledShapeId`, because model shapes are pre-existing and
  // cannot be discarded.
  // Must be called before Add().
  // Must not be called while another add/remove has been started.
  [[nodiscard]] base::expected<std::optional<IdType>, std::monostate>
  StartAdd();
  // Records adding an annotation identified by `id`.
  // Must be called between StartAdd() and FinishAdd().
  // Callers must ensure that IDs added are in increasing order.
  // `id` must not be on the commands stack.
  // `id` must not be an `InkModeledShapeId`.
  [[nodiscard]] bool Add(IdType id);
  // Finishes recording add commands and pushes a new element onto the stack.
  // Must be called after StartAdd().
  [[nodiscard]] bool FinishAdd();

  // Starts recording remove commands. If the current commands stack position is
  // not at the top of the stack, then this discards all entries from the
  // current position to the top of the stack. Returns the lowest annotation ID
  // among added elements to discard. Since IDs are added in increasing order,
  // all elements with the same ID or larger IDs can be discarded. This does not
  // return `InkModeledShapeId`, because model shapes are pre-existing and
  // cannot be discarded.
  // Must be called before Remove().
  // Must not be called while another add/remove has been started.
  [[nodiscard]] base::expected<std::optional<IdType>, std::monostate>
  StartRemove();
  // Records erasing an annotation identified by `id`.
  // Must be called between StartRemove() and FinishRemove().
  // `id` must not be in any `RemoveCommands` on the commands stack.
  // If `id` is for a stroke, it must be in a `AddCommands` on the commands
  // stack.
  // If the caller passes in invalid values, `PdfInkUndoRedoModel` will
  // faithfully give them back during undo/redo operations.
  [[nodiscard]] bool Remove(IdType id);
  // Finishes recording remove commands and pushes a new element onto the stack.
  // Must be called after StartRemove().
  [[nodiscard]] bool FinishRemove();

  // Returns the commands that needs to be applied to satisfy the undo / redo
  // request and moves the position in the commands stack without modifying the
  // commands themselves.
  Commands Undo();
  Commands Redo();

  static CommandsType GetCommandsType(const Commands& commands);
  static const AddCommands& GetAddCommands(const Commands& commands);
  static const RemoveCommands& GetRemoveCommands(const Commands& commands);

 private:
  template <typename T>
  base::expected<std::optional<IdType>, std::monostate> StartImpl();

  bool IsAtTopOfStackWithGivenCommandType(CommandsType type) const;
  bool HasIdInAddCommands(IdType id) const;
  bool HasIdInRemoveCommands(IdType id) const;

  // Invariants:
  // (1) Never empty.
  // (2) The last element and only the last element can be `std::monostate`.
  // (3) IDs used in `AddCommands` elements are unique among all `AddCommands`
  //     elements.
  // (4) IDs added to a `AddCommands` must not exist in any `RemoveCommands`.
  // (5) IDs used in `RemoveCommands` elements are unique among all
  //     `RemoveCommands` elements.
  // (6) IDs added to a `RemoveCommands` must exist in some `AddCommands`
  //     element.
  // (7) `AddCommands` only contains `InkStrokeId` elements here. The reason
  //     `AddCommands` can hold `InkModeledShapeId` is to undo an
  //     `InkModeledShapeId` removal, where the caller needs to know they need
  //     to draw the shape.
  std::vector<Commands> commands_stack_ = {std::monostate()};

  // Invariants:
  // (8) Always less than the size of `commands_stack_`.
  size_t stack_position_ = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_UNDO_REDO_MODEL_H_
