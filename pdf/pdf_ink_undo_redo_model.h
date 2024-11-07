// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_UNDO_REDO_MODEL_H_
#define PDF_PDF_INK_UNDO_REDO_MODEL_H_

#include <stddef.h>

#include <optional>
#include <set>
#include <vector>

#include "base/types/strong_alias.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_ink_ids.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// Models commands as seen in the CommandsType enum below. Based on the recorded
// commands, processes undo / redo requests and calculates what commands need to
// be applied.
class PdfInkUndoRedoModel {
 public:
  enum class CommandsType {
    kNone,
    kDrawStroke,
    kEraseStroke,
  };

  // Set of IDs for the enum CommandsType values above.
  using DrawStrokeCommands =
      base::StrongAlias<class DrawStrokeCommandsTag, std::set<InkStrokeId>>;
  using EraseStrokeCommands =
      base::StrongAlias<class EraseStrokeCommandsTag, std::set<InkStrokeId>>;

  using Commands =
      absl::variant<absl::monostate, DrawStrokeCommands, EraseStrokeCommands>;

  // Set of IDs used for drawing to discard.
  using DiscardedDrawStrokeCommands = std::set<InkStrokeId>;

  PdfInkUndoRedoModel();
  PdfInkUndoRedoModel(const PdfInkUndoRedoModel&) = delete;
  PdfInkUndoRedoModel& operator=(const PdfInkUndoRedoModel&) = delete;
  ~PdfInkUndoRedoModel();

  // For all Draw / Erase methods:
  // - The expected usage is: 1 StartOp call, any number of Op calls, 1 FinishOp
  //   call.
  // - StartOp returns a non-null, but possible empty value on success. Returns
  //   nullopt if any requirements are not met.
  // - Op and FinishOp return true on success. Return false if any requirements
  //   are not met.
  // - Must not return false in production code. Returning false is only allowed
  //   in tests to check failure modes without resorting to death tests.

  // Starts recording draw commands. If the current commands stack position is
  // not at the top of the stack, then this discards all entries from the
  // current position to the top of the stack. The caller can discard its
  // entries with IDs that match the returned values.
  // Must be called before DrawStroke().
  // Must not be called while another draw/erase has been started.
  [[nodiscard]] std::optional<DiscardedDrawStrokeCommands> StartDrawStroke();
  // Records drawing a stroke identified by `id`.
  // Must be called between StartDrawStroke() and FinishDrawStroke().
  // `id` must not be on the commands stack.
  [[nodiscard]] bool DrawStroke(InkStrokeId id);
  // Finishes recording draw commands and pushes a new element onto the stack.
  // Must be called after StartDrawStroke().
  [[nodiscard]] bool FinishDrawStroke();

  // Starts recording erase commands. If the current commands stack position is
  // not at the top of the stack, then this discards all entries from the
  // current position to the top of the stack. The caller can discard its
  // entries with IDs that match the returned values.
  // Must be called before EraseStroke().
  // Must not be called while another draw/erase has been started.
  [[nodiscard]] std::optional<DiscardedDrawStrokeCommands> StartEraseStroke();
  // Records erasing a stroke identified by `id`.
  // Must be called between StartEraseStroke() and FinishEraseStroke().
  // `id` must be in a `DrawStrokeCommands` on the commands stack.
  // `id` must not be in any `EraseStrokeCommands` on the commands stack.
  [[nodiscard]] bool EraseStroke(InkStrokeId id);
  // Finishes recording erase commands and pushes a new element onto the stack.
  // Must be called after StartEraseStroke().
  [[nodiscard]] bool FinishEraseStroke();

  // Returns the commands that needs to be applied to satisfy the undo / redo
  // request and moves the position in the commands stack without modifying the
  // commands themselves.
  Commands Undo();
  Commands Redo();

  static CommandsType GetCommandsType(const Commands& commands);
  static const DrawStrokeCommands& GetDrawStrokeCommands(
      const Commands& commands);
  static const EraseStrokeCommands& GetEraseStrokeCommands(
      const Commands& commands);

 private:
  template <typename T>
  std::optional<DiscardedDrawStrokeCommands> StartImpl();

  bool IsAtTopOfStackWithGivenCommandType(CommandsType type) const;
  bool HasIdInDrawStrokeCommands(InkStrokeId id) const;
  bool HasIdInEraseStrokeCommands(InkStrokeId id) const;

  // Invariants:
  // (1) Never empty.
  // (2) The last element and only the last element can be `absl::monostate`.
  // (3) IDs used in `DrawStrokeCommands` elements are unique among all
  //     `DrawStrokeCommands` elements.
  // (4) IDs added to a `DrawStrokeCommands` must not exist in any
  //     `EraseStrokeCommands`.
  // (5) IDs used in `EraseStrokeCommands` elements are unique among all
  //     `EraseStrokeCommands` elements.
  // (6) IDs added to a `EraseStrokeCommands` must exist in some
  //     `DrawStrokeCommands` element.
  std::vector<Commands> commands_stack_ = {absl::monostate()};

  // Invariants:
  // (7) Always less than the size of `commands_stack_`.
  size_t stack_position_ = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_UNDO_REDO_MODEL_H_
