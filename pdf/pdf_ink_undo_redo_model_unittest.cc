// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_undo_redo_model.h"

#include <stddef.h>

#include <numeric>
#include <optional>
#include <set>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;
using testing::Optional;

namespace chrome_pdf {

using DiscardedDrawCommands = PdfInkUndoRedoModel::DiscardedDrawCommands;
using enum PdfInkUndoRedoModel::CommandsType;

namespace {

// Shorthand for test setup that is expected to succeed.
void DoDrawCommandsCycle(PdfInkUndoRedoModel& undo_redo,
                         const std::set<size_t>& ids) {
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  for (size_t id : ids) {
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
  ASSERT_FALSE(undo_redo.Draw(1));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishDraw) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.FinishDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionEraseWhileDrawing) {
  PdfInkUndoRedoModel undo_redo;
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.Draw(1));

  ASSERT_FALSE(undo_redo.StartErase());
  ASSERT_FALSE(undo_redo.Erase(1));
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
  ASSERT_FALSE(undo_redo.Erase(1));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishErase) {
  PdfInkUndoRedoModel undo_redo;
  ASSERT_FALSE(undo_redo.FinishErase());
}

TEST(PdfInkUndoRedoModelTest, BadActionDrawWhileErasing) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {1});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));

  ASSERT_FALSE(undo_redo.StartDraw());
  ASSERT_FALSE(undo_redo.Draw(2));
  ASSERT_FALSE(undo_redo.FinishDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousDrawAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {4});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({4}));

  ASSERT_FALSE(undo_redo.Draw(1));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishDrawAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {4});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({4}));

  ASSERT_FALSE(undo_redo.FinishDraw());
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousEraseAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {4});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({4}));

  ASSERT_FALSE(undo_redo.Erase(4));
}

TEST(PdfInkUndoRedoModelTest, BadActionSpuriousFinishEraseAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {4});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({4}));

  ASSERT_FALSE(undo_redo.FinishErase());
}

TEST(PdfInkUndoRedoModelTest, BadActionEraseUnknownId) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {1});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_FALSE(undo_redo.Erase(3));
}

TEST(PdfInkUndoRedoModelTest, BadActionEraseTwice) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {0});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.Erase(0));
  ASSERT_FALSE(undo_redo.Erase(0));
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
  DoDrawCommandsCycle(undo_redo, {1, 2, 3});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_FALSE(undo_redo.Draw(1));
  ASSERT_FALSE(undo_redo.Draw(3));

  ASSERT_TRUE(undo_redo.Draw(97));
  ASSERT_TRUE(undo_redo.Draw(99));
  ASSERT_TRUE(undo_redo.Draw(98));

  ASSERT_FALSE(undo_redo.Draw(1));
  ASSERT_FALSE(undo_redo.Draw(98));
}

TEST(PdfInkUndoRedoModelTest, DrawCanRepeatIdAfterUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {1, 2, 3});
  DoDrawCommandsCycle(undo_redo, {97, 98, 99});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({97, 98, 99}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({1, 2, 3}));

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands({1, 2, 3, 97, 98, 99})));
  ASSERT_TRUE(undo_redo.Draw(2));
  ASSERT_TRUE(undo_redo.Draw(98));
}

TEST(PdfInkUndoRedoModelTest, DrawUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {1, 2, 3});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({1, 2, 3}));

  commands = undo_redo.Undo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({1, 2, 3}));

  commands = undo_redo.Redo();
  EXPECT_EQ(kNone, PdfInkUndoRedoModel::GetCommandsType(commands));
}

TEST(PdfInkUndoRedoModelTest, DrawDrawEraseUndoRedo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {1, 2, 3});
  DoDrawCommandsCycle(undo_redo, {4});

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
  ASSERT_TRUE(undo_redo.Erase(1));
  ASSERT_TRUE(undo_redo.Erase(4));
  ASSERT_TRUE(undo_redo.FinishErase());

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({1, 4}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({4}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({1, 2, 3}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({1, 2, 3}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({4}));

  commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({4}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({4}));

  commands = undo_redo.Redo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({1, 4}));
}

TEST(PdfInkUndoRedoModelTest, DrawDrawUndoEraseUndo) {
  PdfInkUndoRedoModel undo_redo;
  DoDrawCommandsCycle(undo_redo, {5});
  DoDrawCommandsCycle(undo_redo, {4, 8});

  PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
  ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
              ElementsAreArray({4, 8}));

  std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
  ASSERT_THAT(discards, Optional(ElementsAreArray({4, 8})));
  ASSERT_TRUE(undo_redo.Erase(5));
  ASSERT_TRUE(undo_redo.FinishErase());

  commands = undo_redo.Undo();
  ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
  EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
              ElementsAreArray({5}));
}

TEST(PdfInkUndoRedoModelTest, Stress) {
#if defined(NDEBUG)
  constexpr size_t kCycles = 10000;
#else
  // The larger non-debug value is too slow for "dbg" bots.
  constexpr size_t kCycles = 1000;
#endif

  PdfInkUndoRedoModel undo_redo;
  size_t id = 0;
  for (size_t i = 0; i < kCycles; ++i) {
    DoDrawCommandsCycle(undo_redo, {id, id + 1});
    id += 2;
  }

  ASSERT_EQ(2 * kCycles, id);
  for (size_t i = 0; i < kCycles; ++i) {
    std::optional<DiscardedDrawCommands> discards = undo_redo.StartErase();
    ASSERT_THAT(discards, Optional(DiscardedDrawCommands()));
    ASSERT_TRUE(undo_redo.Erase(--id));
    ASSERT_TRUE(undo_redo.Erase(--id));
    ASSERT_TRUE(undo_redo.FinishErase());
  }

  ASSERT_EQ(0u, id);
  for (size_t i = 0; i < kCycles; ++i) {
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    ASSERT_EQ(kDraw, PdfInkUndoRedoModel::GetCommandsType(commands));
    EXPECT_THAT(PdfInkUndoRedoModel::GetDrawCommands(commands).value(),
                ElementsAreArray({id, id + 1}));
    id += 2;
  }

  ASSERT_EQ(2 * kCycles, id);
  for (size_t i = 0; i < kCycles; ++i) {
    id -= 2;
    PdfInkUndoRedoModel::Commands commands = undo_redo.Undo();
    ASSERT_EQ(kErase, PdfInkUndoRedoModel::GetCommandsType(commands));
    EXPECT_THAT(PdfInkUndoRedoModel::GetEraseCommands(commands).value(),
                ElementsAreArray({id, id + 1}));
  }

  std::vector<size_t> expected_discards(kCycles * 2);
  std::iota(expected_discards.begin(), expected_discards.end(), 0);
  std::optional<DiscardedDrawCommands> discards = undo_redo.StartDraw();
  ASSERT_THAT(discards, Optional(ElementsAreArray(expected_discards)));
  ASSERT_TRUE(undo_redo.Draw(0));
  ASSERT_TRUE(undo_redo.FinishDraw());
}

}  // namespace

}  // namespace chrome_pdf
