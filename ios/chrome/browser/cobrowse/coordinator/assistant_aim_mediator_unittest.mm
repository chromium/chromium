// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/contextual_tasks/public/features.h"
#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AssistantAIMMediatorTest : public PlatformTest {
 protected:
  AssistantAIMMediatorTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    scoped_feature_list_.InitAndEnableFeature(
        contextual_tasks::kContextualTasks);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(&browser_state_);
    auto manager = std::make_unique<web::FakeWebFramesManager>();
    fake_web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                        std::move(manager));

    mediator_ =
        [[AssistantAIMMediator alloc] initWithWebState:std::move(fake_web_state)
                                               context:nullptr
                                      containerHandler:nil
                                contextualTasksService:nullptr];

    mock_delegate_ = OCMProtocolMock(@protocol(AssistantAIMMediatorDelegate));
    mediator_.delegate = mock_delegate_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::FakeBrowserState browser_state_;
  AssistantAIMMediator* mediator_;
  id mock_delegate_;
};

// Tests that prepareLoadForQueryText correctly aborts if the message is empty.
TEST_F(AssistantAIMMediatorTest, AbortsWhenMessageEmpty) {
  // Empty message (size 0).
  lens::ClientToAimMessage message;

  [[mock_delegate_ reject] assistantAIMMediatorDidLoadQuery:[OCMArg any]];

  [mediator_ prepareLoadWithClientToAimMessage:message];

  [mock_delegate_ verify];
}

// Tests that prepareLoadForQueryText correctly aborts if the WebState is null.
TEST_F(AssistantAIMMediatorTest, AbortsWhenWebStateNull) {
  lens::ClientToAimMessage message;
  // Non-empty message.
  message.mutable_submit_query()->mutable_payload()->set_query_text("test");

  [mediator_ disconnect];

  [[mock_delegate_ reject] assistantAIMMediatorDidLoadQuery:[OCMArg any]];

  [mediator_ prepareLoadWithClientToAimMessage:message];

  [mock_delegate_ verify];
}

// Tests that prepareLoadForQueryText successfully loads the query and notifies
// the delegate when valid parameters are provided.
TEST_F(AssistantAIMMediatorTest, LoadsSuccessfully) {
  lens::ClientToAimMessage message;
  // Non-empty message.
  message.mutable_submit_query()->mutable_payload()->set_query_text("test");

  [[mock_delegate_ expect] assistantAIMMediatorDidLoadQuery:mediator_];

  [mediator_ prepareLoadWithClientToAimMessage:message];

  [mock_delegate_ verify];
}
