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

using enum PdfInkUndoRedoModel::CommandsType;

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
                        const std::set<InkStrokeId>& ids) {
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartAdd();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  for (InkStrokeId id : ids) {
    ASSERT_TRUE(undo_redo.Add(id));
  }
  ASSERT_TRUE(undo_redo.FinishAdd());
}

TEST(PdfInkUndoRedoModelTest, BadActionDoubleStartAdd) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartAdd();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.StartAdd().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousAdd) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.Add(InkStrokeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishAdd) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.FinishAdd());
}

TEST(PdfInkUndoRedoModelTest, BadActionAddModeledShape) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartAdd();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Add(InkModeledShapeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionRemoveWhileAdding) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartAdd();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(1)));

  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.FinishRemove());
}

TEST(PdfInkUndoRedoModelTest, BadActionDoubleStartRemove) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.StartRemove().has_value());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousRemove) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.Remove(InkModeledShapeId(2)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishRemove) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.FinishRemove());
}

TEST(PdfInkUndoRedoModelTest, BadActionAddWhileErasing) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());

  ASSERT_FALSE(undo_redo.Add(InkStrokeId(2)));
  ASSERT_FALSE(undo_redo.FinishAdd());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousAddAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.Add(InkStrokeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishAddAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.FinishAdd());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousRemoveAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(4)));
  ASSERT_FALSE(undo_redo.Remove(InkModeledShapeId(9)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishRemoveAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.FinishRemove());
}

TEST(PdfInkUndoRedoModelTest, BadActionRemoveUnknownId) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(1)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(3)));
}

TEST(PdfInkUndoRedoModelTest, BadActionRemoveTwice) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(0)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(0)));
  ASSERT_FALSE(undo_redo.Remove(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(0)));
  ASSERT_FALSE(undo_redo.Remove(InkModeledShapeId(0)));
}

TEST(PdfInkUndoRedoModelTest, Empty) {
  PdfInkUndoRedoModel undo_redo;
  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, EmptyAdd) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, EmptyRemove) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.FinishRemove());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, AddEnforcesIncreasingOrder) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo,
                     {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartAdd();
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
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
      ElementsAreArray({InkStrokeId(97), InkStrokeId(98), InkStrokeId(99)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartAdd();
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
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetAddCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, AddAddRemoveUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo,
                     {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});
  DoAddCommandsCycle(undo_redo, {InkStrokeId(4)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(4)));
  ASSERT_TRUE(undo_redo.FinishRemove());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetAddCommands(commands).value(),
              ElementsAreArray({InkStrokeId(1), InkStrokeId(4)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetAddCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetAddCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetAddCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(1), InkStrokeId(4)}));
}

TEST(PdfInkUndoRedoModelTest, AddAddUndoRemoveUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(5)});
  DoAddCommandsCycle(undo_redo, {InkStrokeId(6), InkStrokeId(8)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAreArray({InkStrokeId(6), InkStrokeId(8)}));

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_THAT(lowest_discard.value(), Optional(InkStrokeId(6)));
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(5)));
  ASSERT_TRUE(undo_redo.FinishRemove());

  commands = undo_redo.Undo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetAddCommands(commands).value(),
              ElementsAreArray({InkStrokeId(5)}));
}

TEST(PdfInkUndoRedoModelTest, RemoveShapesUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(0)));
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(1)));
  ASSERT_TRUE(undo_redo.FinishRemove());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetAddCommands(commands).value(),
              ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1)));

  commands = undo_redo.Redo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
              ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1)));
}

TEST(PdfInkUndoRedoModelTest, AddAddRemoveStrokesAndShapesUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoAddCommandsCycle(undo_redo, {InkStrokeId(5)});
  DoAddCommandsCycle(undo_redo, {InkStrokeId(6), InkStrokeId(8)});

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartRemove();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_FALSE(lowest_discard.value().has_value());
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(0)));
  ASSERT_TRUE(undo_redo.Remove(InkStrokeId(6)));
  ASSERT_TRUE(undo_redo.Remove(InkModeledShapeId(1)));
  ASSERT_TRUE(undo_redo.FinishRemove());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetAddCommands(commands).value(),
      ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1), InkStrokeId(6)));

  commands = undo_redo.Redo();
  ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
      ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1), InkStrokeId(6)));
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
        undo_redo.StartRemove();
    ASSERT_TRUE(lowest_discard.has_value());
    ASSERT_FALSE(lowest_discard.value().has_value());
    ASSERT_TRUE(undo_redo.Remove(--id));
    ASSERT_TRUE(undo_redo.Remove(--id));
    ASSERT_TRUE(undo_redo.FinishRemove());
  }

  ASSERT_EQ(InkStrokeId(0), id);
  for (size_t i = 0; i < kCycles; ++i) {
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    ASSERT_EQ(kAdd, PdfInkUndoRedoModel::GetCommandsType(commands));
    EXPECT_THAT(PdfInkUndoRedoModel::GetAddCommands(commands).value(),
                ElementsAreArray({id, id + 1}));
    id += 2;
  }

  ASSERT_EQ(InkStrokeId(2 * kCycles), id);
  for (size_t i = 0; i < kCycles; ++i) {
    id -= 2;
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    ASSERT_EQ(kRemove, PdfInkUndoRedoModel::GetCommandsType(commands));
    EXPECT_THAT(PdfInkUndoRedoModel::GetRemoveCommands(commands).value(),
                ElementsAreArray({id, id + 1}));
  }

  base::expected<std::optional<IdType>, std::monostate> lowest_discard =
      undo_redo.StartAdd();
  ASSERT_TRUE(lowest_discard.has_value());
  ASSERT_THAT(lowest_discard.value(), Optional(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.Add(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.FinishAdd());
}

}  // namespace

}  // namespace chrome_pdf
