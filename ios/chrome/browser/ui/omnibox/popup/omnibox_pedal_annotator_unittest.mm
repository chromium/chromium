// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"

#import "components/omnibox/browser/actions/omnibox_action.h"
#import "components/omnibox/browser/actions/omnibox_pedal.h"
#import "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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
  match.actions.push_back(std::move(pedal));

  EXPECT_TRUE([annotator_ pedalForMatch:match] != nil);
}

}  // namespace
