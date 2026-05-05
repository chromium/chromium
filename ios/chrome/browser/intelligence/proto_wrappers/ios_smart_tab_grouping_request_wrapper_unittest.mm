// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/ios_smart_tab_grouping_request_wrapper.h"

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/ios_smart_tab_grouping.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/paths/paths_internal.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

class IosSmartTabGroupingRequestWrapperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build(temp_dir_.GetPath());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    PersistTabContextBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = PersistTabContextBrowserAgent::FromBrowser(browser_.get());
  }

  std::unique_ptr<web::FakeWebState> CreateFakeWebState(
      const std::string& url,
      const std::string& title) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetCurrentURL(GURL(url));
    web_state->SetTitle(base::UTF8ToUTF16(title));
    web_state->SetContentsMimeType("text/html");

    // Setup fake web frame managers to prevent null pointer dereference in
    // PageContextWrapper.
    for (web::ContentWorld world : {web::ContentWorld::kIsolatedWorld,
                                    web::ContentWorld::kPageContentWorld}) {
      web_state->SetWebFramesManager(
          world, std::make_unique<web::FakeWebFramesManager>());
    }
    return web_state;
  }

  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<PersistTabContextBrowserAgent> agent_;
};

TEST_F(IosSmartTabGroupingRequestWrapperTest, Initialization) {
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:web_state_list_
          persistTabContextBrowserAgent:agent_
                     completionCallback:future.GetCallback()];
  EXPECT_NE(wrapper, nil);
}

TEST_F(IosSmartTabGroupingRequestWrapperTest, EmptyWebStateListFromWebStates) {
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:web_state_list_
          persistTabContextBrowserAgent:agent_
                     completionCallback:future.GetCallback()];

  [wrapper populateRequestFieldsAsyncFromWebStates];

  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      request = future.Take();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->tabs_size(), 0);
}

TEST_F(IosSmartTabGroupingRequestWrapperTest,
       EmptyWebStateListFromPersistence) {
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:web_state_list_
          persistTabContextBrowserAgent:agent_
                     completionCallback:future.GetCallback()];

  [wrapper populateRequestFieldsAsyncFromPersistence];

  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      request = future.Take();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->tabs_size(), 0);
}

TEST_F(IosSmartTabGroupingRequestWrapperTest, PopulatesMetadataFromWebStates) {
  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebState("https://google.com", "Google");
  // Verified that enabling realization works cleanly without crashing due to
  // FakeWebFramesManager inclusion.
  web_state->SetIsRealized(true);
  web_state_list_->InsertWebState(std::move(web_state));

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:web_state_list_
          persistTabContextBrowserAgent:agent_
                     completionCallback:future.GetCallback()];

  [wrapper populateRequestFieldsAsyncFromWebStates];

  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      request = future.Take();
  ASSERT_TRUE(request);
  ASSERT_EQ(request->tabs_size(), 1);
  EXPECT_EQ(request->tabs(0).title(), "Google");
  EXPECT_EQ(request->tabs(0).url(), "https://google.com/");
}

// Tests populateRequestFieldsAsyncFromPersistence but does not inject cached
// proto since we already have detailed unit tests for
// PersistTabContextBrowserAgent retrieving protos. This ensures the agent is
// invoked and callback completes.
TEST_F(IosSmartTabGroupingRequestWrapperTest, PopulatesFromPersistenceBasic) {
  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebState("https://example.com", "Example");
  web_state_list_->InsertWebState(std::move(web_state));

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:web_state_list_
          persistTabContextBrowserAgent:agent_
                     completionCallback:future.GetCallback()];

  [wrapper populateRequestFieldsAsyncFromPersistence];

  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      request = future.Take();
  ASSERT_TRUE(request);
  ASSERT_EQ(request->tabs_size(), 1);
  EXPECT_EQ(request->tabs(0).title(), "Example");
  EXPECT_EQ(request->tabs(0).url(), "https://example.com/");
  // No cached page context expected here.
  EXPECT_FALSE(request->tabs(0).has_page_context());
}

TEST_F(IosSmartTabGroupingRequestWrapperTest,
       PopulatesMultipleWebStatesFromWebStates) {
  std::unique_ptr<web::FakeWebState> web_state1 =
      CreateFakeWebState("https://google.com", "Google");
  web_state1->SetIsRealized(false);
  web_state_list_->InsertWebState(std::move(web_state1));

  std::unique_ptr<web::FakeWebState> web_state2 =
      CreateFakeWebState("https://example.com", "Example");
  web_state2->SetIsRealized(false);
  web_state_list_->InsertWebState(std::move(web_state2));

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:web_state_list_
          persistTabContextBrowserAgent:agent_
                     completionCallback:future.GetCallback()];

  [wrapper populateRequestFieldsAsyncFromWebStates];

  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      request = future.Take();
  ASSERT_TRUE(request);
  ASSERT_EQ(request->tabs_size(), 2);
  EXPECT_EQ(request->tabs(0).url(), "https://google.com/");
  EXPECT_EQ(request->tabs(1).url(), "https://example.com/");
}

TEST_F(IosSmartTabGroupingRequestWrapperTest,
       PopulatesMultipleWebStatesFromPersistence) {
  web_state_list_->InsertWebState(
      CreateFakeWebState("https://google.com", "Google"));
  web_state_list_->InsertWebState(
      CreateFakeWebState("https://example.com", "Example"));

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:web_state_list_
          persistTabContextBrowserAgent:agent_
                     completionCallback:future.GetCallback()];

  [wrapper populateRequestFieldsAsyncFromPersistence];

  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      request = future.Take();
  ASSERT_TRUE(request);
  ASSERT_EQ(request->tabs_size(), 2);
  EXPECT_EQ(request->tabs(0).url(), "https://google.com/");
  EXPECT_EQ(request->tabs(1).url(), "https://example.com/");
}

TEST_F(IosSmartTabGroupingRequestWrapperTest,
       PopulatesFromPersistenceWithCachedData) {
  // Ensure FileSystem persistence mode is specifically forced regardless of
  // default environment configurations.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeaturesAndParameters(
      {{kPersistTabContext, {{kPersistTabContextStorageParam, "0"}}}}, {});

  // Re-instantiate a clean local browser and agent which picks up the override.
  std::unique_ptr<TestBrowser> test_browser =
      std::make_unique<TestBrowser>(profile_.get());
  PersistTabContextBrowserAgent::CreateForBrowser(test_browser.get());
  PersistTabContextBrowserAgent* test_agent =
      PersistTabContextBrowserAgent::FromBrowser(test_browser.get());
  WebStateList* local_web_state_list = test_browser->GetWebStateList();

  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebState("https://example.com", "Example");
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  local_web_state_list->InsertWebState(std::move(web_state));

  // Create a cached PageContext proto.
  optimization_guide::proto::PageContext context;
  context.set_title("Cached Title");
  context.set_url("https://cached.com/");
  context.set_inner_text("Cached Data");
  std::string serialized_context;
  ASSERT_TRUE(context.SerializeToString(&serialized_context));

  // Write the serialized proto to the user's filesystem cache directory so that
  // the PersistTabContextBrowserAgent successfully finds it.
  base::FilePath cache_dir;
  ios::GetUserCacheDirectory(profile_->GetStatePath(), &cache_dir);
  base::FilePath contexts_dir = cache_dir.Append("persisted_tab_contexts");
  ASSERT_TRUE(base::CreateDirectory(contexts_dir));
  base::FilePath file_path = contexts_dir.Append(base::StrCat(
      {"page_context_", base::NumberToString(web_state_id.identifier()),
       ".proto"}));
  ASSERT_TRUE(base::WriteFile(file_path, serialized_context));

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>>
      future;
  IosSmartTabGroupingRequestWrapper* wrapper =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:local_web_state_list
          persistTabContextBrowserAgent:test_agent
                     completionCallback:future.GetCallback()];

  [wrapper populateRequestFieldsAsyncFromPersistence];

  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      request = future.Take();
  ASSERT_TRUE(request);
  ASSERT_EQ(request->tabs_size(), 1);
  EXPECT_EQ(request->tabs(0).title(), "Example");
  // Confirm that the cached context was successfully mapped back onto the
  // proto!
  EXPECT_TRUE(request->tabs(0).has_page_context());
  EXPECT_EQ(request->tabs(0).page_context().inner_text(), "Cached Data");
}

}  // namespace
