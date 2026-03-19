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
#include "pdf/buildflags.h"
#include "pdf/pdf_ink_ids.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// Models add and remove commands. Based on the recorded commands, processes
// undo / redo requests and calculates what commands need to be applied.
class PdfInkUndoRedoModel {
 public:
  // TODO(crbug.com/408976048): Only used for testing. Remove once no longer
  // used.
  enum class CommandsType {
    kNone,
    kAdd,
    kRemove,
  };

  using IdSet = std::set<IdType, IdTypeComparator>;

  // A set of add and remove commands. IDs should be unique in the two sets.
  struct Commands {
    Commands();
    Commands(const Commands&);
    Commands& operator=(const Commands&);
    ~Commands();

    IdSet adds;
    IdSet removes;
  };

  PdfInkUndoRedoModel();
  PdfInkUndoRedoModel(const PdfInkUndoRedoModel&) = delete;
  PdfInkUndoRedoModel& operator=(const PdfInkUndoRedoModel&) = delete;
  ~PdfInkUndoRedoModel();

  // For `Start()`, `Add()`, `Remove()`, and `Finish()`:
  // - The expected usage is: 1 `Start()` call, any number of `Add()` or
  //   `Remove()` calls, and 1 `Finish()` call.
  // - `Start()` returns the lowest annotation ID among added elements to
  //   discard, or nullopt if there are no elements to discard on success.
  //   Returns `std::monostate` if any requirements are not met.
  // - `Add()`, `Remove()`, and `Finish()` return true on success. Return false
  //   if any requirements are not met.
  // - Must not return `std::monostate` or false in production code. Returning
  //   `std::monostate` or false is only allowed in tests to check failure modes
  //   without resorting to death tests.

  // Starts recording commands. If the current commands stack position is not at
  // the top of the stack, then this discards all entries from the current
  // position to the top of the stack. Returns the lowest annotation ID among
  // added elements to discard. Since IDs are added in increasing order, all
  // elements with the same ID or larger IDs can be discarded. This will never
  // return an `InkModeledShapeId` which is preexisting and cannot be discarded.
  // Must be called before Add() or Remove().
  // Must not be called while an operation has already started.
  [[nodiscard]] base::expected<std::optional<IdType>, std::monostate> Start();

  // Records adding an annotation identified by `id`.
  // Must be called between Start() and Finish().
  // Callers must ensure that IDs added are in increasing order.
  // `id` must not be on the commands stack.
  // `id` must not be an `InkModeledShapeId`.
  [[nodiscard]] bool Add(IdType id);

  // Records erasing an annotation identified by `id`.
  // Must be called between Start() and Finish().
  // `id` must not be in any `Commands::removes` on the commands stack.
  // If `id` is for a stroke or text, it must be in a `Commands::adds` on the
  // commands stack.
  // If the caller passes in invalid values, `PdfInkUndoRedoModel` will
  // faithfully give them back during undo/redo operations.
  [[nodiscard]] bool Remove(IdType id);

  // Finishes recording commands and pushes a new element onto the stack. Must
  // be called after Start().
  [[nodiscard]] bool Finish();

  // Returns the commands that needs to be applied to satisfy the undo / redo
  // request and moves the position in the commands stack without modifying the
  // commands themselves.
  Commands Undo();
  Commands Redo();

 private:
  bool HasIdInAddCommands(IdType id) const;
  bool HasIdInRemoveCommands(IdType id) const;

  // Invariants:
  // (1) Never empty.
  // (2) The last element and only the last element can be empty.
  // (3) IDs used in `Commands::adds` are unique among all `Commands::adds`
  //     elements.
  // (4) IDs in `Commands::adds` must not exist in any `Commands::removes`.
  // (5) IDs used in `Commands::removes` are unique among all
  //     `Commands::removes` elements.
  // (6) IDs added to a `Commands::removes` must exist in some `Commands::adds`
  //     element.
  // (7) `Commands::adds` only contains `InkStrokeId` and `InkTextId` elements
  //     here. The reason `Commands::adds` can hold `InkModeledShapeId` is to
  //     undo an `InkModeledShapeId` removal, where the caller needs to know
  //     they need to draw the shape or text annotation.
  std::vector<Commands> commands_stack_ = {Commands()};

  // Invariants:
  // (8) Always less than the size of `commands_stack_`.
  size_t stack_position_ = 0;

  // Whether a recording session is currently in progress.
  bool has_started_ = false;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_UNDO_REDO_MODEL_H_
