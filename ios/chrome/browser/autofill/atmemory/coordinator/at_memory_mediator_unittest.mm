// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using autofill::AtMemoryViewState;

class AtMemoryMediatorTest : public PlatformTest {
 protected:
  void TearDown() override {
    [AtMemoryMediator setRecentFills:nil];
    PlatformTest::TearDown();
  }
};

// Tests that setting the consumer on the mediator immediately pushes the
// kEmpty content state to it.
TEST_F(AtMemoryMediatorTest, SetsInitialEmptyStateOnConsumer) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject mockForProtocol:@protocol(AtMemoryConsumer)];

  OCMExpect([consumer setGranularFillItems:OCMOCK_ANY]);
  OCMExpect([consumer setViewState:AtMemoryViewState::kEmpty]);

  mediator.consumer = consumer;

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that setting the consumer on the mediator immediately pushes the
// kPreviouslyFilled content state to it when configured with previously
// filled data.
TEST_F(AtMemoryMediatorTest, SetsInitialPreviouslyFilledStateOnConsumer) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  AtMemorySearchItem* item = [[AtMemorySearchItem alloc] init];
  NSArray* fakeData = @[ item ];
  [AtMemoryMediator setRecentFills:fakeData];
  id consumer = [OCMockObject mockForProtocol:@protocol(AtMemoryConsumer)];

  OCMExpect([consumer setGranularFillItems:OCMOCK_ANY]);
  OCMExpect([consumer setRecentFills:fakeData]);
  OCMExpect([consumer setViewState:AtMemoryViewState::kRecentFills]);

  mediator.consumer = consumer;

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that changing search text triggers kSearch state on consumer.
TEST_F(AtMemoryMediatorTest, HandleSearchTextChange) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject niceMockForProtocol:@protocol(AtMemoryConsumer)];
  mediator.consumer = consumer;

  OCMExpect([consumer setSearchQuery:@"query"]);
  OCMExpect([consumer setViewState:AtMemoryViewState::kSearch]);

  [mediator atMemoryViewController:nil didChangeSearchText:@"query"];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that clearing search text returns to empty state.
TEST_F(AtMemoryMediatorTest, HandleClearSearchText) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject niceMockForProtocol:@protocol(AtMemoryConsumer)];
  mediator.consumer = consumer;

  OCMExpect([consumer setViewState:AtMemoryViewState::kEmpty]);

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

// Tests that tapping search result info transitions to kGranularFill state.
TEST_F(AtMemoryMediatorTest, HandleTapSearchResultInfo) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject niceMockForProtocol:@protocol(AtMemoryConsumer)];
  mediator.consumer = consumer;

  OCMExpect([consumer setViewState:AtMemoryViewState::kGranularFill]);

  [mediator atMemoryViewControllerDidTapSearchResultInfo:nil];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that selecting content triggers injection and dismiss.
TEST_F(AtMemoryMediatorTest, HandleSelectContent) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id mockInjector =
      [OCMockObject mockForProtocol:@protocol(ManualFillContentInjector)];
  mediator.contentInjector = mockInjector;

  id mockViewController =
      [OCMockObject mockForClass:[AtMemoryViewController class]];
  id mockHandler = [OCMockObject mockForProtocol:@protocol(AtMemoryCommands)];
  OCMStub([mockViewController atMemoryHandler]).andReturn(mockHandler);

  OCMExpect([mockInjector userDidPickContent:@"test_content"
                               passwordField:NO
                               requiresHTTPS:NO]);
  OCMExpect([mockHandler dismissAtMemory]);

  [mediator atMemoryViewController:mockViewController
                  didSelectContent:@"test_content"];

  EXPECT_OCMOCK_VERIFY(mockInjector);
  EXPECT_OCMOCK_VERIFY(mockHandler);
}

}  // namespace
