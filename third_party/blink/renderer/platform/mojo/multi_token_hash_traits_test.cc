// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/multi_token_hash_traits.h"

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

TEST(MultiTokenHashTraitsTest, FrameTokenHashMap) {
  HashMap<FrameToken, base::FilePath> map;
  FrameToken local_token{LocalFrameToken()};
  FrameToken remote_token{RemoteFrameToken()};

  EXPECT_TRUE(map.empty());
  map.insert(local_token, base::FilePath(FILE_PATH_LITERAL("local.html")));
  map.insert(remote_token, base::FilePath(FILE_PATH_LITERAL("remote.html")));
  EXPECT_EQ(map.size(), 2u);

  EXPECT_TRUE(map.Contains(local_token));
  EXPECT_TRUE(map.Contains(remote_token));
  EXPECT_EQ(map.at(local_token),
            base::FilePath(FILE_PATH_LITERAL("local.html")));
  EXPECT_EQ(map.at(remote_token),
            base::FilePath(FILE_PATH_LITERAL("remote.html")));

  EXPECT_EQ(map.Take(local_token),
            base::FilePath(FILE_PATH_LITERAL("local.html")));
  EXPECT_EQ(map.size(), 1u);
  EXPECT_FALSE(map.Contains(local_token));
  EXPECT_TRUE(map.Contains(remote_token));
}

TEST(MultiTokenHashTraitsTest, SameUnderlyingValueDifferentVariant) {
  HashMap<FrameToken, String> map;
  base::UnguessableToken raw = base::UnguessableToken::Create();
  FrameToken local_token{LocalFrameToken(raw)};
  FrameToken remote_token{RemoteFrameToken(raw)};

  map.insert(local_token, "local");
  map.insert(remote_token, "remote");
  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map.at(local_token), "local");
  EXPECT_EQ(map.at(remote_token), "remote");
}

TEST(MultiTokenHashTraitsTest, FrameTokenHashSetRehash) {
  HashSet<FrameToken> set;
  Vector<FrameToken> tokens;
  for (int i = 0; i < 100; ++i) {
    tokens.push_back(i % 2 == 0 ? FrameToken(LocalFrameToken())
                                : FrameToken(RemoteFrameToken()));
    set.insert(tokens.back());
  }
  for (int i = 0; i < 100; i += 2) {
    set.erase(tokens[i]);
  }
  EXPECT_EQ(set.size(), 50u);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(set.Contains(tokens[i]), i % 2 == 1);
  }
}

TEST(MultiTokenHashTraitsTest, SentinelValues) {
  using Traits = HashTraits<FrameToken>;
  FrameToken token{LocalFrameToken()};

  EXPECT_NE(Traits::EmptyValue(), Traits::DeletedValue());
  EXPECT_NE(Traits::EmptyValue(), token);
  EXPECT_NE(Traits::DeletedValue(), token);
  EXPECT_FALSE((HashMap<FrameToken, String>::IsValidKey(Traits::EmptyValue())));
  EXPECT_FALSE(
      (HashMap<FrameToken, String>::IsValidKey(Traits::DeletedValue())));
  EXPECT_TRUE((HashMap<FrameToken, String>::IsValidKey(token)));
}

TEST(MultiTokenHashTraitsTest, FrameTokenAsHashMapValue) {
  HashMap<int, FrameToken> map;
  FrameToken local_token{LocalFrameToken()};
  FrameToken remote_token{RemoteFrameToken()};

  map.insert(1, local_token);
  map.insert(2, remote_token);
  HashMap<int, FrameToken> copy(map);
  EXPECT_EQ(copy.at(1), local_token);
  EXPECT_EQ(copy.at(2), remote_token);
}

}  // namespace

}  // namespace blink
