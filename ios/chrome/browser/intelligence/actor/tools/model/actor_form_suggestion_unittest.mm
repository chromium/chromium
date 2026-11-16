// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion_favicon_data_source.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/gfx/image/image.h"

// Fake favicon data source capturing load requests for `ActorFormSuggestion`.
@interface FakeActorFormSuggestionFaviconDataSource
    : NSObject <ActorFormSuggestionFaviconDataSource>

// Whether `-actorFormSuggestion:loadFaviconWithCompletion:` should initiate
// loading (`YES` by default).
@property(nonatomic, assign) BOOL shouldInitiateLoad;

// Optional icon delivered synchronously inside
// `-actorFormSuggestion:loadFaviconWithCompletion:`.
@property(nonatomic, strong) UIImage* synchronousIcon;

// Number of times `-actorFormSuggestion:loadFaviconWithCompletion:` was called.
@property(nonatomic, assign, readonly) NSUInteger loadCallCount;

// Latest completion block passed to
// `-actorFormSuggestion:loadFaviconWithCompletion:`.
@property(nonatomic, copy, readonly)
    ActorFormSuggestionFaviconCompletion pendingCompletion;

@end

@implementation FakeActorFormSuggestionFaviconDataSource

- (instancetype)init {
  self = [super init];
  if (self) {
    _shouldInitiateLoad = YES;
  }
  return self;
}

- (BOOL)actorFormSuggestion:(ActorFormSuggestion*)suggestion
    loadFaviconWithCompletion:(ActorFormSuggestionFaviconCompletion)completion {
  _loadCallCount++;
  if (!_shouldInitiateLoad) {
    return NO;
  }
  if (_synchronousIcon) {
    completion(_synchronousIcon);
    return YES;
  }
  _pendingCompletion = [completion copy];
  return YES;
}

@end

namespace {

using ActorFormSuggestionTest = PlatformTest;

// Creates a minimal password `actor_login::Credential` for testing.
actor_login::Credential CreatePasswordCredential() {
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  cred.type = actor_login::CredentialType::kPassword;
  cred.username = u"user@example.com";
  cred.display_origin = u"example.com";
  return cred;
}

// Tests that `icon` returns `formSuggestion.icon` directly for federated
// credentials with a brand icon and skips `faviconDataSource`.
TEST_F(ActorFormSuggestionTest, FederatedBrandIconSkipsFaviconDataSource) {
  UIImage* brand_icon = [[UIImage alloc] init];
  actor_login::Credential cred = CreatePasswordCredential();
  cred.type = actor_login::CredentialType::kFederated;
  actor_login::FederationDetail detail;
  detail.brand_icon = gfx::Image(brand_icon);
  cred.federation_detail = detail;

  FakeActorFormSuggestionFaviconDataSource* data_source =
      [[FakeActorFormSuggestionFaviconDataSource alloc] init];
  ActorFormSuggestion* suggestion =
      [[ActorFormSuggestion alloc] initWithCredential:cred];
  suggestion.faviconDataSource = data_source;

  EXPECT_NE(suggestion.icon, nil);
  EXPECT_EQ(suggestion.icon, suggestion.formSuggestion.icon);

  __block bool handler_called = false;
  [suggestion loadIconWithUpdateHandler:^{
    handler_called = true;
  }];
  EXPECT_FALSE(handler_called);
  EXPECT_EQ(data_source.loadCallCount, 0u);
}

// Tests that `loadIconWithUpdateHandler:` populates `icon` synchronously when
// `faviconDataSource` completes immediately, while keeping `formSuggestion`
// unchanged.
TEST_F(ActorFormSuggestionTest, SynchronousFaviconLoadUpdatesIcon) {
  UIImage* favicon = [[UIImage alloc] init];
  FakeActorFormSuggestionFaviconDataSource* data_source =
      [[FakeActorFormSuggestionFaviconDataSource alloc] init];
  data_source.synchronousIcon = favicon;

  ActorFormSuggestion* suggestion = [[ActorFormSuggestion alloc]
      initWithCredential:CreatePasswordCredential()];
  suggestion.faviconDataSource = data_source;
  EXPECT_EQ(suggestion.icon, nil);

  __block size_t update_count = 0;
  [suggestion loadIconWithUpdateHandler:^{
    update_count++;
  }];

  EXPECT_EQ(update_count, 1u);
  EXPECT_EQ(suggestion.icon, favicon);
  EXPECT_EQ(suggestion.formSuggestion.icon, nil);
  EXPECT_EQ(data_source.loadCallCount, 1u);

  // Subsequent calls once `icon` is loaded are no-ops.
  [suggestion loadIconWithUpdateHandler:^{
    update_count++;
  }];
  EXPECT_EQ(update_count, 1u);
  EXPECT_EQ(data_source.loadCallCount, 1u);
}

// Tests that `loadIconWithUpdateHandler:` replaces the pending update handler
// if called again while in flight without starting a duplicate fetch.
TEST_F(ActorFormSuggestionTest, AsynchronousFaviconLoadNotifiesLatestHandler) {
  FakeActorFormSuggestionFaviconDataSource* data_source =
      [[FakeActorFormSuggestionFaviconDataSource alloc] init];
  ActorFormSuggestion* suggestion = [[ActorFormSuggestion alloc]
      initWithCredential:CreatePasswordCredential()];
  suggestion.faviconDataSource = data_source;

  __block bool first_handler_called = false;
  __block bool second_handler_called = false;
  [suggestion loadIconWithUpdateHandler:^{
    first_handler_called = true;
  }];
  [suggestion loadIconWithUpdateHandler:^{
    second_handler_called = true;
  }];

  EXPECT_EQ(data_source.loadCallCount, 1u);
  ASSERT_TRUE(data_source.pendingCompletion);

  UIImage* favicon = [[UIImage alloc] init];
  data_source.pendingCompletion(favicon);

  EXPECT_FALSE(first_handler_called);
  EXPECT_TRUE(second_handler_called);
  EXPECT_EQ(suggestion.icon, favicon);
}

// Tests that finishing without a favicon image does not invoke the update
// handler and leaves `icon` nil.
TEST_F(ActorFormSuggestionTest, EmptyFaviconCompletionDoesNotInvokeHandler) {
  FakeActorFormSuggestionFaviconDataSource* data_source =
      [[FakeActorFormSuggestionFaviconDataSource alloc] init];
  ActorFormSuggestion* suggestion = [[ActorFormSuggestion alloc]
      initWithCredential:CreatePasswordCredential()];
  suggestion.faviconDataSource = data_source;

  __block bool handler_called = false;
  [suggestion loadIconWithUpdateHandler:^{
    handler_called = true;
  }];
  ASSERT_TRUE(data_source.pendingCompletion);

  data_source.pendingCompletion(nil);
  EXPECT_FALSE(handler_called);
  EXPECT_EQ(suggestion.icon, nil);
}

}  // namespace
