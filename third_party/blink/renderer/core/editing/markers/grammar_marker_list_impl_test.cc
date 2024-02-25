// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/spell_check_marker_list_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker_list_impl.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// Functionality implemented in SpellCheckMarkerListImpl is tested in
// spelling_marker_list_impl_test.cc.

class GrammarMarkerListImplTest : public testing::Test {
 protected:
  GrammarMarkerListImplTest()
      : marker_list_(MakeGarbageCollected<GrammarMarkerListImpl>()) {}

  Persistent<GrammarMarkerListImpl> marker_list_;
};

// Test cases for functionality implemented by GrammarMarkerListImpl.

TEST_F(GrammarMarkerListImplTest, MarkerType) {
  EXPECT_EQ(DocumentMarker::kGrammar, marker_list_->MarkerType());
}

}  // namespace
