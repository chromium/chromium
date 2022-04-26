// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class OmniboxPedalAnnotatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    annotator_ = [[OmniboxPedalAnnotator alloc] init];
  }

  OmniboxPedalAnnotator* annotator_;
};

TEST_F(OmniboxPedalAnnotatorTest, CreatesPedal) {
  AutocompleteMatch match;

  scoped_refptr<OmniboxPedal> pedal =
      base::WrapRefCounted(new TestOmniboxPedalClearBrowsingData());
  match.action = pedal;

  EXPECT_TRUE([annotator_ pedalForMatch:match incognito:NO] != nil);
}

}  // namespace
