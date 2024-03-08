// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

struct Options {
  CascadeOrigin origin = CascadeOrigin::kAuthor;
  bool important = false;
  uint16_t tree_order = 0;
  bool is_inline_style = false;
  bool is_try_style = false;
  bool is_try_tactics_style = false;
  uint16_t layer_order = 0;
  uint32_t position = 0;
};

CascadePriority Priority(Options o) {
  return CascadePriority(o.origin, o.important, o.tree_order, o.is_inline_style,
                         o.is_try_style, o.is_try_tactics_style, o.layer_order,
                         o.position);
}

CascadePriority AuthorPriority(uint16_t tree_order, uint32_t position) {
  return Priority({.origin = CascadeOrigin::kAuthor,
                   .tree_order = tree_order,
                   .position = position});
}

CascadePriority ImportantAuthorPriority(uint16_t tree_order,
                                        uint32_t position) {
  return Priority({.origin = CascadeOrigin::kAuthor,
                   .important = true,
                   .tree_order = tree_order,
                   .position = position});
}

}  // namespace

TEST(CascadePriorityTest, EncodeOriginImportance) {
  using Origin = CascadeOrigin;
  EXPECT_EQ(0b00001ull, EncodeOriginImportance(Origin::kUserAgent, false));
  EXPECT_EQ(0b00010ull, EncodeOriginImportance(Origin::kUser, false));
  EXPECT_EQ(0b00100ull, EncodeOriginImportance(Origin::kAuthor, false));
  EXPECT_EQ(0b00101ull, EncodeOriginImportance(Origin::kAnimation, false));
  EXPECT_EQ(0b01011ull, EncodeOriginImportance(Origin::kAuthor, true));
  EXPECT_EQ(0b01101ull, EncodeOriginImportance(Origin::kUser, true));
  EXPECT_EQ(0b01110ull, EncodeOriginImportance(Origin::kUserAgent, true));
  EXPECT_EQ(0b10000ull, EncodeOriginImportance(Origin::kTransition, false));
}

TEST(CascadePriorityTest, OriginOperators) {
  std::vector<CascadePriority> priorities = {
      Priority({.origin = CascadeOrigin::kTransition}),
      Priority({.origin = CascadeOrigin::kAnimation}),
      Priority({.origin = CascadeOrigin::kAuthor}),
      Priority({.origin = CascadeOrigin::kUser}),
      Priority({.origin = CascadeOrigin::kUserAgent}),
      Priority({.origin = CascadeOrigin::kNone})};

  for (size_t i = 0; i < priorities.size(); ++i) {
    for (size_t j = i; j < priorities.size(); ++j) {
      EXPECT_GE(priorities[i], priorities[j]);
      EXPECT_FALSE(priorities[i] < priorities[j]);
    }
  }

  for (size_t i = 0; i < priorities.size(); ++i) {
    for (size_t j = i + 1; j < priorities.size(); ++j) {
      EXPECT_LT(priorities[j], priorities[i]);
      EXPECT_FALSE(priorities[j] >= priorities[i]);
    }
  }

  for (CascadePriority priority : priorities) {
    EXPECT_EQ(priority, priority);
  }

  for (size_t i = 0; i < priorities.size(); ++i) {
    for (size_t j = 0; j < priorities.size(); ++j) {
      if (i == j) {
        continue;
      }
      EXPECT_NE(priorities[i], priorities[j]);
    }
  }
}

TEST(CascadePriorityTest, OriginImportance) {
  std::vector<CascadePriority> priorities = {
      Priority({.origin = CascadeOrigin::kTransition, .important = false}),
      Priority({.origin = CascadeOrigin::kUserAgent, .important = true}),
      Priority({.origin = CascadeOrigin::kUser, .important = true}),
      Priority({.origin = CascadeOrigin::kAuthor, .important = true}),
      Priority({.origin = CascadeOrigin::kAnimation, .important = false}),
      Priority({.origin = CascadeOrigin::kAuthor, .important = false}),
      Priority({.origin = CascadeOrigin::kUser, .important = false}),
      Priority({.origin = CascadeOrigin::kUserAgent, .important = false}),
      Priority({.origin = CascadeOrigin::kNone, .important = false})};

  for (size_t i = 0; i < priorities.size(); ++i) {
    for (size_t j = i; j < priorities.size(); ++j) {
      EXPECT_GE(priorities[i], priorities[j]);
    }
  }
}

TEST(CascadePriorityTest, IsImportant) {
  using Origin = CascadeOrigin;

  EXPECT_FALSE(Priority({.origin = Origin::kUserAgent}).IsImportant());
  EXPECT_FALSE(Priority({.origin = Origin::kUser}).IsImportant());
  EXPECT_FALSE(Priority({.origin = Origin::kAuthor}).IsImportant());
  EXPECT_FALSE(Priority({.origin = Origin::kAnimation}).IsImportant());
  EXPECT_FALSE(Priority({.origin = Origin::kTransition}).IsImportant());
  EXPECT_FALSE(Priority({.origin = Origin::kAuthor,
                         .important = false,
                         .tree_order = 1024,
                         .layer_order = 2048,
                         .position = 4096})
                   .IsImportant());

  EXPECT_TRUE(Priority({.origin = Origin::kUserAgent, .important = true})
                  .IsImportant());
  EXPECT_TRUE(
      Priority({.origin = Origin::kUser, .important = true}).IsImportant());
  EXPECT_TRUE(
      Priority({.origin = Origin::kAuthor, .important = true}).IsImportant());
  EXPECT_TRUE(Priority({.origin = Origin::kAnimation, .important = true})
                  .IsImportant());
  EXPECT_TRUE(Priority({.origin = Origin::kTransition, .important = true})
                  .IsImportant());
  EXPECT_TRUE(Priority({.origin = Origin::kAuthor,
                        .important = true,
                        .tree_order = 1024,
                        .layer_order = 2048,
                        .position = 4096})
                  .IsImportant());
}

static std::vector<CascadeOrigin> all_origins = {
    CascadeOrigin::kUserAgent, CascadeOrigin::kUser, CascadeOrigin::kAuthor,
    CascadeOrigin::kTransition, CascadeOrigin::kAnimation};

TEST(CascadePriorityTest, GetOrigin) {
  for (CascadeOrigin origin : all_origins) {
    EXPECT_EQ(Priority({.origin = origin, .important = false}).GetOrigin(),
              origin);
  }

  for (CascadeOrigin origin : all_origins) {
    if (origin == CascadeOrigin::kAnimation) {
      continue;
    }
    if (origin == CascadeOrigin::kTransition) {
      continue;
    }
    EXPECT_EQ(Priority({.origin = origin, .important = true}).GetOrigin(),
              origin);
  }
}

TEST(CascadePriorityTest, HasOrigin) {
  for (CascadeOrigin origin : all_origins) {
    if (origin != CascadeOrigin::kNone) {
      EXPECT_TRUE(CascadePriority(origin).HasOrigin());
    } else {
      EXPECT_FALSE(CascadePriority(origin).HasOrigin());
    }
  }
  EXPECT_FALSE(CascadePriority().HasOrigin());
}

TEST(CascadePriorityTest, EncodeTreeOrder) {
  EXPECT_EQ(0ull, EncodeTreeOrder(0, false));
  EXPECT_EQ(1ull, EncodeTreeOrder(1, false));
  EXPECT_EQ(2ull, EncodeTreeOrder(2, false));
  EXPECT_EQ(100ull, EncodeTreeOrder(100, false));
  EXPECT_EQ(0xFFFFull, EncodeTreeOrder(0xFFFF, false));

  EXPECT_EQ(0ull ^ 0xFFFF, EncodeTreeOrder(0, true));
  EXPECT_EQ(1ull ^ 0xFFFF, EncodeTreeOrder(1, true));
  EXPECT_EQ(2ull ^ 0xFFFF, EncodeTreeOrder(2, true));
  EXPECT_EQ(100ull ^ 0xFFFF, EncodeTreeOrder(100, true));
  EXPECT_EQ(0xFFFFull ^ 0xFFFF, EncodeTreeOrder(0xFFFF, true));
}

TEST(CascadePriorityTest, TreeOrder) {
  using Priority = CascadePriority;
  CascadeOrigin origin = CascadeOrigin::kAuthor;
  EXPECT_GE(Priority(origin, false, 1), Priority(origin, false, 0));
  EXPECT_GE(Priority(origin, false, 7), Priority(origin, false, 6));
  EXPECT_GE(Priority(origin, false, 42), Priority(origin, false, 42));
  EXPECT_FALSE(Priority(origin, false, 1) >= Priority(origin, false, 8));
}

TEST(CascadePriorityTest, TreeOrderImportant) {
  using Priority = CascadePriority;
  CascadeOrigin origin = CascadeOrigin::kAuthor;
  EXPECT_GE(Priority(origin, true, 0), Priority(origin, true, 1));
  EXPECT_GE(Priority(origin, true, 6), Priority(origin, true, 7));
  EXPECT_GE(Priority(origin, true, 42), Priority(origin, true, 42));
  EXPECT_FALSE(Priority(origin, true, 8) >= Priority(origin, true, 1));
}

TEST(CascadePriorityTest, TreeOrderDifferentOrigin) {
  using Priority = CascadePriority;
  // Tree order does not matter if the origin is different.
  CascadeOrigin author = CascadeOrigin::kAuthor;
  CascadeOrigin transition = CascadeOrigin::kTransition;
  EXPECT_GE(Priority(transition, 1), Priority(author, 42));
  EXPECT_GE(Priority(transition, 1), Priority(author, 1));
}

TEST(CascadePriorityTest, Position) {
  // AuthorPriority(tree_order, position)
  EXPECT_GE(AuthorPriority(0, 0), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(0, 1), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(0, 2), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(0, 0xFFFFFFFF), AuthorPriority(0, 0xFFFFFFFE));
  EXPECT_FALSE(AuthorPriority(0, 2) >= AuthorPriority(0, 3));
}

TEST(CascadePriorityTest, PositionAndTreeOrder) {
  // AuthorPriority(tree_order, position)
  EXPECT_GE(AuthorPriority(1, 0), AuthorPriority(0, 0));
  EXPECT_GE(AuthorPriority(1, 1), AuthorPriority(0, 1));
  EXPECT_GE(AuthorPriority(1, 1), AuthorPriority(0, 3));
  EXPECT_GE(AuthorPriority(1, 2), AuthorPriority(0, 0xFFFFFFFF));
}

TEST(CascadePriorityTest, PositionAndOrigin) {
  // [Important]AuthorPriority(tree_order, position)
  EXPECT_GE(ImportantAuthorPriority(0, 0), AuthorPriority(0, 0));
  EXPECT_GE(ImportantAuthorPriority(0, 1), AuthorPriority(0, 1));
  EXPECT_GE(ImportantAuthorPriority(0, 1), AuthorPriority(0, 3));
  EXPECT_GE(ImportantAuthorPriority(0, 2), AuthorPriority(0, 0xFFFFFFFF));
}

TEST(CascadePriorityTest, Generation) {
  CascadePriority ua(CascadeOrigin::kUserAgent);
  CascadePriority author(CascadeOrigin::kAuthor);

  EXPECT_EQ(author, author);
  EXPECT_GE(CascadePriority(author, 1), author);
  EXPECT_GE(CascadePriority(author, 2), CascadePriority(author, 1));
  EXPECT_EQ(CascadePriority(author, 2), CascadePriority(author, 2));

  EXPECT_LT(ua, author);
  EXPECT_LT(CascadePriority(ua, 1), author);
  EXPECT_LT(CascadePriority(ua, 2), CascadePriority(author, 1));
  EXPECT_LT(CascadePriority(ua, 2), CascadePriority(author, 2));
  EXPECT_LT(CascadePriority(ua, 2), CascadePriority(author, 3));
}

TEST(CascadePriorityTest, GenerationOverwrite) {
  CascadePriority ua(CascadeOrigin::kUserAgent);

  for (int8_t g = 0; g < 16; ++g) {
    ua = CascadePriority(ua, g);
    EXPECT_EQ(g, ua.GetGeneration());
  }

  for (int8_t g = 15; g >= 0; --g) {
    ua = CascadePriority(ua, g);
    EXPECT_EQ(g, ua.GetGeneration());
  }
}

TEST(CascadePriorityTest, PositionEncoding) {
  // Test 0b0, 0b1, 0b11, 0b111, etc.
  uint32_t pos = 0;
  do {
    // AuthorPriority(tree_order, position)
    ASSERT_EQ(pos, AuthorPriority(0, pos).GetPosition());
    pos = (pos << 1) | 1;
  } while (pos != ~static_cast<uint32_t>(0));

  // Test 0b1, 0b10, 0b100, etc
  pos = 1;
  do {
    // AuthorPriority(tree_order, position)
    ASSERT_EQ(pos, AuthorPriority(0, pos).GetPosition());
    pos <<= 1;
  } while (pos != ~static_cast<uint32_t>(1) << 31);
}

TEST(CascadePriorityTest, EncodeLayerOrder) {
  EXPECT_EQ(0ull, EncodeLayerOrder(0, false));
  EXPECT_EQ(1ull, EncodeLayerOrder(1, false));
  EXPECT_EQ(2ull, EncodeLayerOrder(2, false));
  EXPECT_EQ(100ull, EncodeLayerOrder(100, false));
  EXPECT_EQ(0xFFFFull, EncodeLayerOrder(0xFFFF, false));

  EXPECT_EQ(0ull ^ 0xFFFF, EncodeLayerOrder(0, true));
  EXPECT_EQ(1ull ^ 0xFFFF, EncodeLayerOrder(1, true));
  EXPECT_EQ(2ull ^ 0xFFFF, EncodeLayerOrder(2, true));
  EXPECT_EQ(100ull ^ 0xFFFF, EncodeLayerOrder(100, true));
  EXPECT_EQ(0xFFFFull ^ 0xFFFF, EncodeLayerOrder(0xFFFF, true));
}

TEST(CascadePriorityTest, LayerOrder) {
  EXPECT_GE(Priority({.layer_order = 1}), Priority({.layer_order = 0}));
  EXPECT_GE(Priority({.layer_order = 7}), Priority({.layer_order = 6}));
  EXPECT_GE(Priority({.layer_order = 42}), Priority({.layer_order = 42}));
  EXPECT_FALSE(Priority({.layer_order = 1}) >= Priority({.layer_order = 8}));
}

TEST(CascadePriorityTest, LayerOrderImportant) {
  EXPECT_GE(Priority({.important = true, .layer_order = 0}),
            Priority({.important = true, .layer_order = 1}));
  EXPECT_GE(Priority({.important = true, .layer_order = 6}),
            Priority({.important = true, .layer_order = 7}));
  EXPECT_GE(Priority({.important = true, .layer_order = 4}),
            Priority({.important = true, .layer_order = 4}));
  EXPECT_FALSE(Priority({.important = true, .layer_order = 8}) >=
               Priority({.important = true, .layer_order = 1}));
}

TEST(CascadePriorityTest, LayerOrderDifferentOrigin) {
  // Layer order does not matter if the origin is different.
  CascadeOrigin transition = CascadeOrigin::kTransition;
  EXPECT_GE(Priority({.origin = transition, .layer_order = 1}),
            Priority({.layer_order = 42}));
  EXPECT_GE(Priority({.origin = transition, .layer_order = 1}),
            Priority({.layer_order = 1}));
}

TEST(CascadePriorityTest, InlineStyle) {
  CascadeOrigin user = CascadeOrigin::kUser;

  // Non-important inline style priorities
  EXPECT_GE(Priority({.is_inline_style = true}), Priority({.position = 1}));
  EXPECT_GE(Priority({.is_inline_style = true}), Priority({.layer_order = 1}));
  EXPECT_GE(Priority({.tree_order = 1, .is_inline_style = true}),
            Priority({.is_inline_style = false}));
  EXPECT_LT(Priority({.tree_order = 1, .is_inline_style = true}),
            Priority({.tree_order = 2}));
  EXPECT_GE(Priority({.is_inline_style = true}), Priority({.origin = user}));
  EXPECT_LT(Priority({.is_inline_style = true}), Priority({.important = true}));

  // Important inline style priorities
  EXPECT_GE(Priority({.important = true, .is_inline_style = true}),
            Priority({.important = true, .position = 1}));
  EXPECT_GE(Priority({.important = true, .is_inline_style = true}),
            Priority({.important = true, .layer_order = 1}));
  EXPECT_LT(
      Priority({.important = true, .tree_order = 1, .is_inline_style = true}),
      Priority({.important = true}));
  EXPECT_GE(
      Priority({.important = true, .tree_order = 1, .is_inline_style = true}),
      Priority({.important = true, .tree_order = 2}));
  EXPECT_LT(Priority({.important = true, .is_inline_style = true}),
            Priority({.origin = user, .important = true}));
  EXPECT_GE(Priority({.important = true, .is_inline_style = true}),
            Priority({.is_inline_style = false}));
}

TEST(CascadePriorityTest, TryStyle) {
  EXPECT_GE(Priority({.is_try_style = true}), Priority({}));
  EXPECT_GE(Priority({.is_try_style = true}),
            Priority({.is_inline_style = true}));
  EXPECT_GE(Priority({.is_try_style = true}),
            Priority({.layer_order = static_cast<uint16_t>(
                          EncodeLayerOrder(1u, /* important */ false))}));
  EXPECT_GE(Priority({.is_try_style = true}), Priority({.position = 1000}));

  EXPECT_LT(Priority({.is_try_style = true}), Priority({.important = true}));
  EXPECT_LT(Priority({.is_try_style = true}),
            Priority({.origin = CascadeOrigin::kAnimation}));
  EXPECT_LT(Priority({.is_try_style = true}),
            Priority({.origin = CascadeOrigin::kTransition}));

  // Try styles generate a separate layer.
  EXPECT_NE(Priority({.is_try_style = true}).ForLayerComparison(),
            Priority({}).ForLayerComparison());
}

TEST(CascadePriorityTest, TryTacticsStyle) {
  // Should be stronger than try-style.
  EXPECT_GE(Priority({.is_try_tactics_style = true}),
            Priority({.is_try_style = true}));

  // Should be stronger than inline styles.
  EXPECT_GE(Priority({.is_try_tactics_style = true}),
            Priority({.is_inline_style = true}));

  // Should be stronger than author cascade layers.
  EXPECT_GE(Priority({.is_try_tactics_style = true}),
            Priority({.layer_order = 1000}));

  // Should be weaker than important in the same origin
  EXPECT_LT(Priority({.is_try_tactics_style = true}),
            Priority({.important = true}));

  // Should be weaker than a stronger origin.
  EXPECT_LT(Priority({.is_try_tactics_style = true}),
            Priority({.origin = CascadeOrigin::kTransition}));

  // Try-tactics styles generate a separate layer.
  EXPECT_NE(Priority({.is_try_tactics_style = true}).ForLayerComparison(),
            Priority({}).ForLayerComparison());
  // Also a separate layer vs. the try styles.
  EXPECT_NE(Priority({.is_try_tactics_style = true}).ForLayerComparison(),
            Priority({.is_try_style = true}).ForLayerComparison());
}

TEST(CascadePriorityTest, ForLayerComparison) {
  CascadeOrigin user = CascadeOrigin::kUser;

  EXPECT_EQ(Priority({.layer_order = 1, .position = 2}).ForLayerComparison(),
            Priority({.layer_order = 1, .position = 8}).ForLayerComparison());
  EXPECT_EQ(
      Priority(
          {.important = true, .tree_order = 1, .layer_order = 1, .position = 4})
          .ForLayerComparison(),
      Priority(
          {.important = true, .tree_order = 1, .layer_order = 1, .position = 8})
          .ForLayerComparison());
  EXPECT_EQ(Priority({.important = true,
                      .tree_order = 1,
                      .layer_order = 1,
                      .position = 16})
                .ForLayerComparison(),
            Priority({.tree_order = 1, .layer_order = 1, .position = 32})
                .ForLayerComparison());
  EXPECT_EQ(Priority({.important = true,
                      .tree_order = 1,
                      .is_inline_style = true,
                      .position = 16})
                .ForLayerComparison(),
            Priority({.tree_order = 1, .is_inline_style = true, .position = 32})
                .ForLayerComparison());

  EXPECT_LT(Priority({.origin = user, .position = 1}).ForLayerComparison(),
            Priority({.origin = user, .layer_order = 1}).ForLayerComparison());
  EXPECT_LT(
      Priority({.origin = user, .position = 1}).ForLayerComparison(),
      Priority({.origin = user, .is_inline_style = true}).ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .position = 1}).ForLayerComparison(),
            Priority({.origin = user, .tree_order = 1}).ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .position = 1}).ForLayerComparison(),
            Priority({}).ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .important = true, .position = 1})
                .ForLayerComparison(),
            Priority({.origin = user, .layer_order = 1}).ForLayerComparison());
  EXPECT_LT(
      Priority({.origin = user, .important = true, .position = 1})
          .ForLayerComparison(),
      Priority({.origin = user, .is_inline_style = true}).ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .important = true, .position = 1})
                .ForLayerComparison(),
            Priority({.origin = user, .tree_order = 1}).ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .important = true, .position = 1})
                .ForLayerComparison(),
            Priority({}).ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .position = 1}).ForLayerComparison(),
            Priority({.origin = user, .important = true, .layer_order = 1})
                .ForLayerComparison());
  EXPECT_LT(
      Priority({.origin = user, .position = 1}).ForLayerComparison(),
      Priority({.origin = user, .important = true, .is_inline_style = true})
          .ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .position = 1}).ForLayerComparison(),
            Priority({.origin = user, .important = true, .tree_order = 1})
                .ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .position = 1}).ForLayerComparison(),
            Priority({.important = true}).ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .important = true, .position = 1})
                .ForLayerComparison(),
            Priority({.origin = user, .important = true, .layer_order = 1})
                .ForLayerComparison());
  EXPECT_LT(
      Priority({.origin = user, .important = true, .position = 1})
          .ForLayerComparison(),
      Priority({.origin = user, .important = true, .is_inline_style = true})
          .ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .important = true, .position = 1})
                .ForLayerComparison(),
            Priority({.origin = user, .important = true, .tree_order = 1})
                .ForLayerComparison());
  EXPECT_LT(Priority({.origin = user, .important = true, .position = 1})
                .ForLayerComparison(),
            Priority({.important = true}).ForLayerComparison());
}

}  // namespace blink
