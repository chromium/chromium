// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_undo_redo_model.h"

#include <stddef.h>

#include <numeric>
#include <optional>
#include <set>
#include <variant>

#include "base/cfi_buildflags.h"
#include "base/types/expected.h"
#include "pdf/pdf_ink_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Optional;

namespace chrome_pdf {


namespace {

// InkStrokeId modification operators needed only for tests.
InkStrokeId& operator--(InkStrokeId& id) {
  --id.value();
  return id;
}

InkStrokeId operator+(const InkStrokeId& id, int amount) {
  return InkStrokeId(id.value() + amount);
}

InkStrokeId& operator+=(InkStrokeId& id, int amount) {
  id.value() += amount;
  return id;
}

InkStrokeId& operator-=(InkStrokeId& id, int amount) {
  id.value() -= amount;
  return id;
}

// Shorthand for test setup that is expected to succeed.
void DoAddCommandsCycle(PdfInkUndoRedoModel& undo_redo,
                        const std::set<IdType>& ids) {
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  for (IdType id : ids) {
    ASSERT_TRUE(undo_redo.Add(id));
  }
  ASSERT_TRUE(undo_redo.Finish());
}

TEST(PdfInkUndoRedoModelTest, BadActionDoubleStart) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Start().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousAdd) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.Add(InkStrokeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinish) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.Finish());
}

TEST(PdfInkUndoRedoModelTest, BadActionAddModeledShape) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Add(InkModeledShapeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionAddLoadedTextId) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Add(InkLoadedTextId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionAddLoadedSignatureId) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Add(InkLoadedSignatureId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousRemove) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.Remove(InkModeledShapeId(2)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousAddAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes, ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.Add(InkStrokeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes, ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.Finish());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousRemoveAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes, ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(4)));
  ASSERT_FALSE(undo_redo.Remove(InkModeledShapeId(9)));
}

TEST(PdfInkUndoRedoModelTest, BadActionRemoveUnknownStrokeId) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(3)));
}

TEST(PdfInkUndoRedoModelTest, BadActionRemoveUnknownTextId) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Remove(InkTextId(3)));
}

TEST(PdfInkUndoRedoModelTest, BadActionRemoveUnknownSignatureId) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Remove(InkSignatureId(3)));
}

TEST(PdfInkUndoRedoModelTest, BadActionRemoveTwice) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(0)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(0)));
  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(0)));
  ASSERT_FALSE(undo_redo.Remove(InkModeledShapeId(0)));
}

TEST(PdfInkUndoRedoModelTest, BadActionStartAddRemove) {
  PdfInkUndoRedoModel undo_redo;

  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(0)));
  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(0)));
}

TEST(PdfInkUndoRedoModelTest, Empty) {
  PdfInkUndoRedoModel undo_redo;
  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());
}

TEST(PdfInkUndoRedoModelTest, EmptyAdd) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());
}

TEST(PdfInkUndoRedoModelTest, EmptyRemove) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Finish());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());
}

TEST(PdfInkUndoRedoModelTest, AddEnforcesIncreasingOrder) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo,
                     {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());

  // Cannot add ID that is already on the stack.
  ASSERT_FALSE(undo_redo.Add(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.Add(InkStrokeId(3)));

  // Can add larger IDs.
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(97)));
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(99)));

  // Cannot add IDs that are not strictly increasing.
  ASSERT_FALSE(undo_redo.Add(InkStrokeId(99)));
  ASSERT_FALSE(undo_redo.Add(InkStrokeId(98)));
  ASSERT_FALSE(undo_redo.Add(InkStrokeId(1)));
}

TEST(PdfInkUndoRedoModelTest, AddCanRepeatIdAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo,
                     {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});
  DoAddCommandsCycle(undo_redo,
                     {InkStrokeId(97), InkStrokeId(98), InkStrokeId(99)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(
      commands.removes,
      ElementsAreArray({InkStrokeId(97), InkStrokeId(98), InkStrokeId(99)}));

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(
      commands.removes,
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_THAT(lowest_discard.value(), Optional(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(2)));
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(98)));
}

TEST(PdfInkUndoRedoModelTest, AddUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo,
                     {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(
      commands.removes,
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds, ElementsAreArray({InkStrokeId(1), InkStrokeId(2),
                                               InkStrokeId(3)}));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_TRUE(commands.removes.empty());
}

TEST(PdfInkUndoRedoModelTest, AddAddRemoveUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo,
                     {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(4)));
  ASSERT_TRUE(undo_redo.Finish());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds,
              ElementsAreArray({InkStrokeId(1), InkStrokeId(4)}));

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes, ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(
      commands.removes,
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds, ElementsAreArray({InkStrokeId(1), InkStrokeId(2),
                                               InkStrokeId(3)}));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds, ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes, ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds, ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes,
              ElementsAreArray({InkStrokeId(1), InkStrokeId(4)}));
}

TEST(PdfInkUndoRedoModelTest, AddAddUndoRemoveUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(5)});
  DoAddCommandsCycle(undo_redo, {InkStrokeId(6), InkStrokeId(8)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes,
              ElementsAreArray({InkStrokeId(6), InkStrokeId(8)}));

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_THAT(lowest_discard.value(), Optional(InkStrokeId(6)));
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(5)));
  ASSERT_TRUE(undo_redo.Finish());

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds, ElementsAreArray({InkStrokeId(5)}));
}

TEST(PdfInkUndoRedoModelTest, RemoveShapesUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(0)));
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(1)));
  ASSERT_TRUE(undo_redo.Finish());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds,
              ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1)));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes,
              ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1)));
}

TEST(PdfInkUndoRedoModelTest, AddAddRemoveStrokesAndShapesUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(5)});
  DoAddCommandsCycle(undo_redo, {InkStrokeId(6), InkStrokeId(8)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(0)));
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(6)));
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(1)));
  ASSERT_TRUE(undo_redo.Finish());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds, ElementsAre(InkModeledShapeId(0),
                                         InkModeledShapeId(1), InkStrokeId(6)));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(
      commands.removes,
      ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1), InkStrokeId(6)));
}

TEST(PdfInkUndoRedoModelTest, AddRemoveAddUndoUndoStart) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});

  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Finish());

  DoAddCommandsCycle(undo_redo, {InkStrokeId(2)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_FALSE(commands.removes.empty());

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_FALSE(commands.adds.empty());

  // Discarded commands should be Remove(1), Add(2). The lowest discarded stroke
  // ID is 2.
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  EXPECT_THAT(lowest_discard.value(), Optional(InkStrokeId(2)));
}

TEST(PdfInkUndoRedoModelTest, AddUndoStrokeTextAndSignature) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(0)});
  DoAddCommandsCycle(undo_redo, {InkTextId(1)});
  DoAddCommandsCycle(undo_redo, {InkSignatureId(2)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_FALSE(commands.removes.empty());

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_TRUE(lowest_discard.value().has_value());

  IdType lowest_discard_id = lowest_discard.value().value();
  ASSERT_TRUE(std::holds_alternative<InkSignatureId>(lowest_discard_id));
  ASSERT_EQ(InkSignatureId(2), std::get<InkSignatureId>(lowest_discard_id));
  ASSERT_TRUE(undo_redo.Finish());

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_FALSE(commands.removes.empty());

  lowest_discard = undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_TRUE(lowest_discard.value().has_value());

  lowest_discard_id = lowest_discard.value().value();
  ASSERT_TRUE(std::holds_alternative<InkTextId>(lowest_discard_id));
  ASSERT_EQ(InkTextId(1), std::get<InkTextId>(lowest_discard_id));
  ASSERT_TRUE(undo_redo.Finish());

  commands = undo_redo.Undo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_FALSE(commands.removes.empty());

  lowest_discard = undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_TRUE(lowest_discard.value().has_value());

  lowest_discard_id = lowest_discard.value().value();
  ASSERT_TRUE(std::holds_alternative<InkStrokeId>(lowest_discard_id));
  ASSERT_EQ(InkStrokeId(0), std::get<InkStrokeId>(lowest_discard_id));
}

TEST(PdfInkUndoRedoModelTest, AddRemoveAllTypesUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(5)});
  DoAddCommandsCycle(undo_redo, {InkTextId(6)});
  DoAddCommandsCycle(undo_redo, {InkSignatureId(7)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());

  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(5)));
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(5)));
  ASSERT_TRUE(undo_redo.Remove(InkLoadedTextId(6)));
  ASSERT_TRUE(undo_redo.Remove(InkTextId(6)));
  ASSERT_TRUE(undo_redo.Remove(InkLoadedSignatureId(7)));
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(7)));
  ASSERT_TRUE(undo_redo.Finish());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_TRUE(commands.removes.empty());
  EXPECT_THAT(commands.adds,
              ElementsAre(InkStrokeId(5), InkModeledShapeId(5), InkTextId(6),
                          InkLoadedTextId(6), InkSignatureId(7),
                          InkLoadedSignatureId(7)));

  commands = undo_redo.Redo();
  EXPECT_TRUE(commands.adds.empty());
  EXPECT_THAT(commands.removes,
              ElementsAre(InkStrokeId(5), InkModeledShapeId(5), InkTextId(6),
                          InkLoadedTextId(6), InkSignatureId(7),
                          InkLoadedSignatureId(7)));
}

TEST(PdfInkUndoRedoModelTest, EditTextUndoRedo) {
  PdfInkUndoRedoModel undo_redo;

  DoAddCommandsCycle(undo_redo, {InkTextId(0)});

  // "Edit" the text by removing the initial text ID and adding a new text ID.
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkTextId(0)));
  ASSERT_TRUE(undo_redo.Add(InkTextId(1)));
  ASSERT_TRUE(undo_redo.Finish());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_THAT(commands.adds, ElementsAre(InkTextId(0)));
  EXPECT_THAT(commands.removes, ElementsAre(InkTextId(1)));

  commands = undo_redo.Redo();
  EXPECT_THAT(commands.adds, ElementsAre(InkTextId(1)));
  EXPECT_THAT(commands.removes, ElementsAre(InkTextId(0)));
}

TEST(PdfInkUndoRedoModelTest, EditSignatureUndoRedo) {
  PdfInkUndoRedoModel undo_redo;

  DoAddCommandsCycle(undo_redo, {InkSignatureId(0)});

  // "Edit" the signature by removing the initial signature ID and adding a new
  // signature ID.
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(0)));
  ASSERT_TRUE(undo_redo.Add(InkSignatureId(1)));
  ASSERT_TRUE(undo_redo.Finish());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_THAT(commands.adds, ElementsAre(InkSignatureId(0)));
  EXPECT_THAT(commands.removes, ElementsAre(InkSignatureId(1)));

  commands = undo_redo.Redo();
  EXPECT_THAT(commands.adds, ElementsAre(InkSignatureId(1)));
  EXPECT_THAT(commands.removes, ElementsAre(InkSignatureId(0)));
}

TEST(PdfInkUndoRedoModelTest, GetUndoTextIdAndRedoInkTextId) {
  PdfInkUndoRedoModel undo_redo;

  // Add T1.
  DoAddCommandsCycle(undo_redo, {InkTextId(1)});
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());

  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
  EXPECT_THAT(undo_redo.GetRedoInkTextId(), Optional(InkTextId(1)));

  undo_redo.Redo();
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());

  // "Edit" T1 to T2.
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkTextId(1)));
  ASSERT_TRUE(undo_redo.Add(InkTextId(2)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_THAT(undo_redo.GetUndoTextId(), Optional(InkTextId(1)));
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());

  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
  EXPECT_THAT(undo_redo.GetRedoInkTextId(), Optional(InkTextId(2)));
}

TEST(PdfInkUndoRedoModelTest, GetUndoRedoTextIdWithLoadedText) {
  PdfInkUndoRedoModel undo_redo;

  // Remove loaded text ID 1.
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkLoadedTextId(1)));
  ASSERT_TRUE(undo_redo.Finish());

  EXPECT_THAT(undo_redo.GetUndoTextId(), Optional(InkLoadedTextId(1)));

  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());

  undo_redo.Redo();
  EXPECT_THAT(undo_redo.GetUndoTextId(), Optional(InkLoadedTextId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoTextIdEmptyStack) {
  PdfInkUndoRedoModel undo_redo;
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoTextIdAtBottomOfStack) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkTextId(1)});
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoTextIdNothingRemoved) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkTextId(1)});
  undo_redo.Undo();
  undo_redo.Redo();
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoTextIdTooManyItemsRemoved) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkTextId(1)});
  DoAddCommandsCycle(undo_redo, {InkTextId(2)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkTextId(1)));
  ASSERT_TRUE(undo_redo.Remove(InkTextId(2)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoTextIdTooManyItemsAdded) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkTextId(10)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkTextId(10)));
  ASSERT_TRUE(undo_redo.Add(InkTextId(11)));
  ASSERT_TRUE(undo_redo.Add(InkTextId(12)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoTextIdNonText) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_FALSE(undo_redo.GetUndoTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkTextIdEmptyStack) {
  PdfInkUndoRedoModel undo_redo;
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkTextIdAtTopOfStack) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkTextId(1)});
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkTextIdNothingAdded) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Finish());
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkTextIdTooManyItemsAdded) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkTextId(1), InkTextId(2)});
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkTextIdTooManyItemsRemoved) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkTextId(1), InkTextId(2)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkTextId(1)));
  ASSERT_TRUE(undo_redo.Remove(InkTextId(2)));
  ASSERT_TRUE(undo_redo.Add(InkTextId(3)));
  ASSERT_TRUE(undo_redo.Finish());
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkTextIdNonText) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkTextId().has_value());
}

TEST(PdfInkUndoRedoModelTest, GetUndoSignatureIdAndRedoInkSignatureId) {
  PdfInkUndoRedoModel undo_redo;

  // Add S1.
  DoAddCommandsCycle(undo_redo, {InkSignatureId(1)});
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());

  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
  EXPECT_THAT(undo_redo.GetRedoInkSignatureId(), Optional(InkSignatureId(1)));

  undo_redo.Redo();
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());

  // "Edit" S1 to S2.
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(1)));
  ASSERT_TRUE(undo_redo.Add(InkSignatureId(2)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_THAT(undo_redo.GetUndoSignatureId(), Optional(InkSignatureId(1)));
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());

  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
  EXPECT_THAT(undo_redo.GetRedoInkSignatureId(), Optional(InkSignatureId(2)));
}

TEST(PdfInkUndoRedoModelTest, GetUndoRedoSignatureIdWithLoadedSignature) {
  PdfInkUndoRedoModel undo_redo;

  // Remove loaded signature ID 1.
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkLoadedSignatureId(1)));
  ASSERT_TRUE(undo_redo.Finish());

  EXPECT_THAT(undo_redo.GetUndoSignatureId(),
              Optional(InkLoadedSignatureId(1)));

  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());

  undo_redo.Redo();
  EXPECT_THAT(undo_redo.GetUndoSignatureId(),
              Optional(InkLoadedSignatureId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoSignatureIdEmptyStack) {
  PdfInkUndoRedoModel undo_redo;
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoSignatureIdAtBottomOfStack) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkSignatureId(1)});
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoSignatureIdNothingRemoved) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkSignatureId(1)});
  undo_redo.Undo();
  undo_redo.Redo();
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoSignatureIdTooManyItemsRemoved) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkSignatureId(1)});
  DoAddCommandsCycle(undo_redo, {InkSignatureId(2)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(1)));
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(2)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoSignatureIdTooManyItemsAdded) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkSignatureId(10)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(10)));
  ASSERT_TRUE(undo_redo.Add(InkSignatureId(11)));
  ASSERT_TRUE(undo_redo.Add(InkSignatureId(12)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetUndoSignatureIdNonSignature) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Finish());
  EXPECT_FALSE(undo_redo.GetUndoSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkSignatureIdEmptyStack) {
  PdfInkUndoRedoModel undo_redo;
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkSignatureIdAtTopOfStack) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkSignatureId(1)});
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkSignatureIdNothingAdded) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Finish());
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkSignatureIdTooManyItemsAdded) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkSignatureId(1), InkSignatureId(2)});
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkSignatureIdTooManyItemsRemoved) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkSignatureId(1), InkSignatureId(2)});
  ASSERT_TRUE(undo_redo.Start().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(1)));
  ASSERT_TRUE(undo_redo.Remove(InkSignatureId(2)));
  ASSERT_TRUE(undo_redo.Add(InkSignatureId(3)));
  ASSERT_TRUE(undo_redo.Finish());
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadGetRedoInkSignatureIdNonSignature) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});
  undo_redo.Undo();
  EXPECT_FALSE(undo_redo.GetRedoInkSignatureId().has_value());
}

TEST(PdfInkUndoRedoModelTest, Stress) {
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER) ||         \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    BUILDFLAG(CFI_ICALL_CHECK)
  // The larger non-debug value is too slow for "dbg" bots and bots with
  // sanitizers enabled.
  constexpr size_t kCycles = 1000;
#else
  constexpr size_t kCycles = 10000;
#endif

  PdfInkUndoRedoModel undo_redo;
  InkStrokeId id(0);
  for (size_t i = 0; i < kCycles; ++i) {
    DoAddCommandsCycle(undo_redo, {id, id + 1});
    id += 2;
  }

  ASSERT_EQ(InkStrokeId(2 * kCycles), id);
  for (size_t i = 0; i < kCycles; ++i) {
    base::expected<std::optional<IdType>, std::monostate> lowest_discard =
        undo_redo.Start();
    ASSERT_TRUE(lowest_discard.has_value());
    ASSERT_FALSE(lowest_discard.value().has_value());
    ASSERT_TRUE(undo_redo.Remove(--id));
    ASSERT_TRUE(undo_redo.Remove(--id));
    ASSERT_TRUE(undo_redo.Finish());
  }

  ASSERT_EQ(InkStrokeId(0), id);
  for (size_t i = 0; i < kCycles; ++i) {
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    EXPECT_TRUE(commands.removes.empty());
    EXPECT_THAT(commands.adds, ElementsAreArray({id, id + 1}));
    id += 2;
  }

  ASSERT_EQ(InkStrokeId(2 * kCycles), id);
  for (size_t i = 0; i < kCycles; ++i) {
    id -= 2;
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    EXPECT_TRUE(commands.adds.empty());
    EXPECT_THAT(commands.removes, ElementsAreArray({id, id + 1}));
  }

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.Start();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_THAT(lowest_discard.value(), Optional(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.Finish());
}

}  // namespace

}  // namespace chrome_pdf
