// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_undo_redo_model.h"

#include <stddef.h>

#include <numeric>
#include <optional>
#include <set>

#include "pdf/pdf_ink_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Optional;

namespace chrome_pdf {

using DiscardedDrawCommands = PdfInkUndoRedoModel::DiscardedDrawCommands;
using enum PdfInkUndoRedoModel::CommandsType;

// InkStrokeId modification operators needed only for tests.  Must not be in
// the anonymous namespace to work with std::iota.
InkStrokeId& operator++(InkStrokeId& id) {
  ++id.value();
  return id;
}

InkStrokeId operator++(InkStrokeId& id, int) {
  InkStrokeId existing = id;
  ++id;
  return existing;
}

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

namespace {

// Shorthand for test setup that is expected to succeed.
void DoDrawCommandsCycle(PdfInkUndoRedoModel& undo_redo,
                         const std::set<InkStrokeId>& ids) {
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  for (InkStrokeId id : ids) {
    ASSERT_TRUE(undo_redo.Draw(id));
  }
  ASSERT_TRUE(undo_redo.FinishDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionDoubleStartDraw) {
  PdfInkUndoRedoModel undo_redo;
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_FALSE(undo_redo.StartDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousDraw) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.Draw(InkStrokeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishDraw) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.FinishDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionEraseWhileDrawing) {
  PdfInkUndoRedoModel undo_redo;
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.Draw(InkStrokeId(1)));

  ASSERT_FALSE(undo_redo.StartErase());
  ASSERT_FALSE(undo_redo.EraseStroke(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.FinishErase());
}

TEST(PdfInkUndoRedoModelTest, BadActionDoubleStartErase) {
  PdfInkUndoRedoModel undo_redo;
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_FALSE(undo_redo.StartErase());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousErase) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.EraseStroke(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.EraseShape(InkModeledShapeId(2)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishErase) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.FinishErase());
}

TEST(PdfInkUndoRedoModelTest, BadActionDrawWhileErasing) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(1)});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));

  ASSERT_FALSE(undo_redo.StartDraw());
  ASSERT_FALSE(undo_redo.Draw(InkStrokeId(2)));
  ASSERT_FALSE(undo_redo.FinishDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousDrawAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.Draw(InkStrokeId(1)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishDrawAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.FinishDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousEraseAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.EraseStroke(InkStrokeId(4)));
  ASSERT_FALSE(undo_redo.EraseShape(InkModeledShapeId(9)));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishEraseAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(4)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  ASSERT_FALSE(undo_redo.FinishErase());
}

TEST(PdfInkUndoRedoModelTest, BadActionEraseUnknownId) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(1)});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_FALSE(undo_redo.EraseStroke(InkStrokeId(3)));
}

TEST(PdfInkUndoRedoModelTest, BadActionEraseTwice) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(0)});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.EraseStroke(InkStrokeId(0)));
  ASSERT_FALSE(undo_redo.EraseStroke(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.EraseShape(InkModeledShapeId(0)));
  ASSERT_FALSE(undo_redo.EraseShape(InkModeledShapeId(0)));
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

TEST(PdfInkUndoRedoModelTest, EmptyDraw) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, EmptyErase) {
  PdfInkUndoRedoModel undo_redo;
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.FinishErase());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, DrawCannotRepeatId) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo,
                      {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_FALSE(undo_redo.Draw(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.Draw(InkStrokeId(3)));

  ASSERT_TRUE(undo_redo.Draw(InkStrokeId(97)));
  ASSERT_TRUE(undo_redo.Draw(InkStrokeId(99)));
  ASSERT_TRUE(undo_redo.Draw(InkStrokeId(98)));

  ASSERT_FALSE(undo_redo.Draw(InkStrokeId(1)));
  ASSERT_FALSE(undo_redo.Draw(InkStrokeId(98)));
}

TEST(PdfInkUndoRedoModelTest, DrawCanRepeatIdAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo,
                      {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});
  DoDrawCommandsCycle(undo_redo,
                      {InkStrokeId(97), InkStrokeId(98), InkStrokeId(99)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
      ElementsAreArray({InkStrokeId(97), InkStrokeId(98), InkStrokeId(99)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards,
              Optional(DiscardedDrawCommands(
                  {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3),
                   InkStrokeId(97), InkStrokeId(98), InkStrokeId(99)})));
  ASSERT_TRUE(undo_redo.Draw(InkStrokeId(2)));
  ASSERT_TRUE(undo_redo.Draw(InkStrokeId(98)));
}

TEST(PdfInkUndoRedoModelTest, DrawUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo,
                      {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, DrawDrawEraseUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo,
                      {InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)});
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(4)});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.EraseStroke(InkStrokeId(1)));
  ASSERT_TRUE(undo_redo.EraseStroke(InkStrokeId(4)));
  ASSERT_TRUE(undo_redo.FinishErase());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({InkStrokeId(1), InkStrokeId(4)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
      ElementsAreArray({InkStrokeId(1), InkStrokeId(2), InkStrokeId(3)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4)}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(1), InkStrokeId(4)}));
}

TEST(PdfInkUndoRedoModelTest, DrawDrawUndoEraseUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(5)});
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(4), InkStrokeId(8)});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({InkStrokeId(4), InkStrokeId(8)}));

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards,
              Optional(ElementsAreArray({InkStrokeId(4), InkStrokeId(8)})));
  ASSERT_TRUE(undo_redo.EraseStroke(InkStrokeId(5)));
  ASSERT_TRUE(undo_redo.FinishErase());

  commands = undo_redo.Undo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({InkStrokeId(5)}));
}

TEST(PdfInkUndoRedoModelTest, EraseShapesUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.EraseShape(InkModeledShapeId(0)));
  ASSERT_TRUE(undo_redo.EraseShape(InkModeledShapeId(1)));
  ASSERT_TRUE(undo_redo.FinishErase());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1)));

  commands = undo_redo.Redo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAre(InkModeledShapeId(0), InkModeledShapeId(1)));
}

TEST(PdfInkUndoRedoModelTest, DrawDrawEraseStrokesAndShapesUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(5)});
  DoDrawCommandsCycle(undo_redo, {InkStrokeId(4), InkStrokeId(8)});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.EraseShape(InkModeledShapeId(0)));
  ASSERT_TRUE(undo_redo.EraseStroke(InkStrokeId(4)));
  ASSERT_TRUE(undo_redo.EraseShape(InkModeledShapeId(1)));
  ASSERT_TRUE(undo_redo.FinishErase());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
      ElementsAre(InkStrokeId(4), InkModeledShapeId(0), InkModeledShapeId(1)));

  commands = undo_redo.Redo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(
      PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
      ElementsAre(InkStrokeId(4), InkModeledShapeId(0), InkModeledShapeId(1)));
}

TEST(PdfInkUndoRedoModelTest, Stress) {
#if defined(NDEBUG)
  constexpr size_t kCycles = 10000;
#else
  // The larger non-debug value is too slow for "dbg" bots.
  constexpr size_t kCycles = 1000;
#endif

  PdfInkUndoRedoModel undo_redo;
  InkStrokeId id(0);
  for (size_t i = 0; i < kCycles; ++i) {
    DoDrawCommandsCycle(undo_redo, {id, id + 1});
    id += 2;
  }

  ASSERT_EQ(InkStrokeId(2 * kCycles), id);
  for (size_t i = 0; i < kCycles; ++i) {
    std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
    ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
    ASSERT_TRUE(undo_redo.EraseStroke(--id));
    ASSERT_TRUE(undo_redo.EraseStroke(--id));
    ASSERT_TRUE(undo_redo.FinishErase());
  }

  ASSERT_EQ(InkStrokeId(0), id);
  for (size_t i = 0; i < kCycles; ++i) {
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
    EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
                ElementsAreArray({id, id + 1}));
    id += 2;
  }

  ASSERT_EQ(InkStrokeId(2 * kCycles), id);
  for (size_t i = 0; i < kCycles; ++i) {
    id -= 2;
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
    EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
                ElementsAreArray({id, id + 1}));
  }

  std::vector<InkStrokeId> expected_discards(kCycles * 2);
  std::iota(expected_discards.begin(), expected_discards.end(), InkStrokeId(0));
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(ElementsAreArray(expected_discards)));
  ASSERT_TRUE(undo_redo.Draw(InkStrokeId(0)));
  ASSERT_TRUE(undo_redo.FinishDraw());
}

}  // namespace

}  // namespace chrome_pdf
