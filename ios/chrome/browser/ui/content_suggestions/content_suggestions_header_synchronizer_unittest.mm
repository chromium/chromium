// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"

#include <memory>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ContentSuggestionsHeaderSynchronizerTest : public PlatformTest {
 public:
  ContentSuggestionsHeaderSynchronizerTest() {
    header_controller_ =
        OCMProtocolMock(@protocol(ContentSuggestionsHeaderControlling));
    collection_controller_ =
        OCMProtocolMock(@protocol(ContentSuggestionsCollectionControlling));
    synchronizer_ = [[ContentSuggestionsHeaderSynchronizer alloc]
        initWithCollectionController:collection_controller_
                    headerController:header_controller_];
  }

  ContentSuggestionsHeaderSynchronizer* Synchronizer() { return synchronizer_; }

  id HeaderController() { return header_controller_; }

  id CollectionController() { return collection_controller_; }
  void SetAsIPhone() {
    device_type_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UIDevice class], @selector(userInterfaceIdiom),
        ^UIUserInterfaceIdiom(id self) {
          return UIUserInterfaceIdiomPhone;
        });
  }

 private:
  ContentSuggestionsHeaderSynchronizer* synchronizer_;
  id header_controller_;
  id collection_controller_;
  std::unique_ptr<ScopedBlockSwizzler> device_type_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> orientation_swizzler_;
};

TEST_F(ContentSuggestionsHeaderSynchronizerTest, shiftUp) {
  // Setup.
  id collectionController = CollectionController();
  OCMExpect([collectionController setScrolledToTop:YES]);

  // Action.
  [Synchronizer() shiftTilesUpWithAnimations:nil completion:nil];

  // Tests.
  EXPECT_OCMOCK_VERIFY(collectionController);
}

TEST_F(ContentSuggestionsHeaderSynchronizerTest, updateFakeOmnibox) {
  // Setup.
  id headerController = HeaderController();
  OCMExpect([[[headerController stub] ignoringNonObjectArgs]
      updateFakeOmniboxForOffset:10
                     screenWidth:0
                  safeAreaInsets:UIEdgeInsetsZero]);
  SetAsIPhone();

  // Action.
  [Synchronizer() updateFakeOmniboxOnCollectionScroll];

  // Tests.
  EXPECT_OCMOCK_VERIFY(headerController);
}
}  // namespace
