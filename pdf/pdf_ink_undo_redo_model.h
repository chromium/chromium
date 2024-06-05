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
#include "third_party/abseil-cpp/absl/types/variant.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// Models draw and erase commands. Based on the recorded commands,
// processes undo / redo requests and calculates what commands need to be
// applied.
class PdfInkUndoRedoModel {
 public:
  enum class CommandsType {
    kNone,
    kDraw,
    kErase,
  };

  // Set of IDs to draw/erase.
  using DrawCommands =
      base::StrongAlias<class DrawCommandsTag, std::set<size_t>>;
  using EraseCommands =
      base::StrongAlias<class EraseCommandsTag, std::set<size_t>>;

  using Commands = absl::variant<absl::monostate, DrawCommands, EraseCommands>;

  // Set of IDs used for drawing to discard.
  using DiscardedDrawCommands = std::set<size_t>;

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
  // Must be called before Draw().
  // Must not be called while another draw/erase has been started.
  [[nodiscard]] std::optional<DiscardedDrawCommands> StartDraw();
  // Records drawing a stroke identified by `id`.
  // Must be called between StartDraw() and FinishDraw().
  // `id` must not be on the commands stack.
  [[nodiscard]] bool Draw(size_t id);
  // Finishes recording draw commands and pushes a new element onto the stack.
  // Must be called after StartDraw().
  [[nodiscard]] bool FinishDraw();

  // Starts recording erase commands. If the current commands stack position is
  // not at the top of the stack, then this discards all entries from the
  // current position to the top of the stack. The caller can discard its
  // entries with IDs that match the returned values.
  // Must be called before Erase().
  // Must not be called while another draw/erase has been started.
  [[nodiscard]] std::optional<DiscardedDrawCommands> StartErase();
  // Records erasing a stroke identified by `id`.
  // Must be called between StartErase() and FinishErase().
  // `id` must be in a `DrawCommands` on the commands stack.
  // `id` must not be in any `EraseCommands` on the commands stack.
  [[nodiscard]] bool Erase(size_t id);
  // Finishes recording erase commands and pushes a new element onto the stack.
  // Must be called after StartErase().
  [[nodiscard]] bool FinishErase();

  // Returns the commands that needs to be applied to satisfy the undo / redo
  // request and moves the position in the commands stack without modifying the
  // commands themselves.
  Commands Undo();
  Commands Redo();

  static CommandsType GetCommandsType(const Commands& commands);
  static const DrawCommands& GetDrawCommands(const Commands& commands);
  static const EraseCommands& GetEraseCommands(const Commands& commands);

 private:
  template <typename T>
  std::optional<DiscardedDrawCommands> StartImpl();

  bool IsAtTopOfStackWithGivenCommandType(CommandsType type) const;
  bool HasIdInDrawCommands(size_t id) const;
  bool HasIdInEraseCommands(size_t id) const;

  // Invariants:
  // (1) Never empty.
  // (2) The last element and only the last element can be `absl::monostate`.
  // (3) IDs used in `DrawCommands` elements are unique among all `DrawCommands`
  //     elements.
  // (4) IDs added to a `DrawCommands` must not exist in any `EraseCommands`.
  // (5) IDs used in `EraseCommands` elements are unique among all
  //     `EraseCommands` elements.
  // (6) IDs added to a `EraseCommands` must exist in some `DrawCommands`
  //     element.
  std::vector<Commands> commands_stack_ = {absl::monostate()};

  // Invariants:
  // (7) Always less than the size of `commands_stack_`.
  size_t stack_position_ = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_UNDO_REDO_MODEL_H_
