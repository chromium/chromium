// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

const char* const kTestDescription = "Test description";

class SpellingMarkerTest : public testing::Test {};

TEST_F(SpellingMarkerTest, MarkerType) {
  DocumentMarker* marker =
      MakeGarbageCollected<SpellingMarker>(0, 1, kTestDescription);
  EXPECT_EQ(DocumentMarker::kSpelling, marker->GetType());
}

TEST_F(SpellingMarkerTest, IsSpellCheckMarker) {
  DocumentMarker* marker =
      MakeGarbageCollected<SpellingMarker>(0, 1, kTestDescription);
  EXPECT_TRUE(IsSpellCheckMarker(*marker));
}

TEST_F(SpellingMarkerTest, ConstructorAndGetters) {
  SpellingMarker* marker =
      MakeGarbageCollected<SpellingMarker>(0, 1, kTestDescription);
  EXPECT_EQ(kTestDescription, marker->Description());
}

}  // namespace blink
