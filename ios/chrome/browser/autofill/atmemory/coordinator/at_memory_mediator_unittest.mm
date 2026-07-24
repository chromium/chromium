// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AtMemoryMediatorTest : public PlatformTest {};

// Tests that changing search text updates consumer query.
TEST_F(AtMemoryMediatorTest, HandleSearchTextChange) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject niceMockForProtocol:@protocol(AtMemoryConsumer)];
  mediator.consumer = consumer;

  OCMExpect([consumer setSearchQuery:@"query"]);

  [mediator atMemoryViewController:nil didChangeSearchText:@"query"];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that clearing search text updates consumer query.
TEST_F(AtMemoryMediatorTest, HandleClearSearchText) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject niceMockForProtocol:@protocol(AtMemoryConsumer)];
  mediator.consumer = consumer;

  OCMExpect([consumer setSearchQuery:@""]);

  [mediator atMemoryViewController:nil didChangeSearchText:@""];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that tapping search starts loading.
TEST_F(AtMemoryMediatorTest, HandleTapSearchStartsLoading) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject niceMockForProtocol:@protocol(AtMemoryConsumer)];
  mediator.consumer = consumer;

  OCMExpect([consumer setSearchLoading:YES]);

  [mediator atMemoryViewControllerDidTapSearch:nil];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that tapping search result info sets granular fill items.
TEST_F(AtMemoryMediatorTest, HandleTapSearchResultInfo) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject niceMockForProtocol:@protocol(AtMemoryConsumer)];
  mediator.consumer = consumer;

  OCMExpect([consumer setGranularFillItems:[OCMArg any]]);

  [mediator atMemoryViewController:nil didTapSearchResultInfoForItem:nil];

  EXPECT_OCMOCK_VERIFY(consumer);
}
