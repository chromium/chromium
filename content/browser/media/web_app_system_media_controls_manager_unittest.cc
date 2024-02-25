// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/web_app_system_media_controls_manager.h"
#include "content/browser/media/system_media_controls_notifier.h"

#include "base/unguessable_token.h"
#include "components/system_media_controls/mock_system_media_controls.h"
#include "content/browser/media/web_app_system_media_controls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using system_media_controls::SystemMediaControls;

class WebAppSystemMediaControlsManagerTest : public testing::Test {
 public:
  WebAppSystemMediaControlsManagerTest() = default;

  WebAppSystemMediaControlsManagerTest(
      const WebAppSystemMediaControlsManagerTest&) = delete;
  WebAppSystemMediaControlsManagerTest& operator=(
      const WebAppSystemMediaControlsManagerTest&) = delete;

  ~WebAppSystemMediaControlsManagerTest() override = default;

  void SetUp() override {
    manager_ = std::make_unique<WebAppSystemMediaControlsManager>();
    manager_->SkipMojoConnectionForTesting();
    manager_->Init();

    ASSERT_NE(manager_, nullptr);
  }

  // While we would love to use manager_->OnFocusGained() to unittest
  // WebAppSystemMediaControlsManager, it depends on too many external things
  // e.g. getting HWNDs, resolving WebContents' from RequestIds etc. So just
  // manually insert data into the manager and test retrieval/query functions.
  void AddClient(base::UnguessableToken request_id) {
    ASSERT_NE(manager_, nullptr);
    ASSERT_TRUE(manager_->controls_map_.find(request_id) ==
                manager_->controls_map_.end());
    auto empty_system_media_controls_ptr =
        std::make_unique<WebAppSystemMediaControls>();
    manager_->controls_map_.emplace(request_id,
                                    std::move(empty_system_media_controls_ptr));
  }

  // This variant of AddClient returns an empty |WebAppSystemMediaControls| for
  // verification that manager_ is doing it's job.
  WebAppSystemMediaControls* AddClientWithWebAppSystemMediaControls(
      base::UnguessableToken request_id) {
    std::unique_ptr<WebAppSystemMediaControls>
        empty_webapp_system_media_controls =
            std::make_unique<WebAppSystemMediaControls>();

    // Cache the unique_ptr value for test verification purposes.
    WebAppSystemMediaControls* raw_ptr =
        empty_webapp_system_media_controls.get();

    manager_->controls_map_.emplace(
        request_id, std::move(empty_webapp_system_media_controls));

    return raw_ptr;
  }

  // This variant of AddClient returns the MockSystemMediaControls and
  // WebAppSystemMediaControls for verification purposes.
  void AddClientWithMockSystemMediaControls(
      base::UnguessableToken request_id,
      WebAppSystemMediaControls** out_webapp_system_media_controls,
      SystemMediaControls** out_system_media_controls) {
    std::unique_ptr<WebAppSystemMediaControls>
        empty_webapp_system_media_controls =
            std::make_unique<WebAppSystemMediaControls>();
    CHECK(empty_webapp_system_media_controls);

    // Cache the unique_ptr value for test verification purposes.
    WebAppSystemMediaControls* raw_webapp_system_media_controls_ptr =
        empty_webapp_system_media_controls.get();

    std::unique_ptr<SystemMediaControls> mock_system_media_controls =
        std::make_unique<
            system_media_controls::testing::MockSystemMediaControls>();

    CHECK(mock_system_media_controls);

    // Cache the unique_ptr value for test verification purposes.
    SystemMediaControls* raw_system_media_controls_ptr =
        mock_system_media_controls.get();

    // Insert the mock_smc into the WebAppSystemMediaControls.
    empty_webapp_system_media_controls->system_media_controls_ =
        std::move(mock_system_media_controls);

    manager_->controls_map_.emplace(
        request_id, std::move(empty_webapp_system_media_controls));

    // Return the cached raw pointers to the calee.
    *out_webapp_system_media_controls = raw_webapp_system_media_controls_ptr;
    *out_system_media_controls = raw_system_media_controls_ptr;
  }

  // Simulates a request ID being released.
  void RemoveClient(base::UnguessableToken request_id) {
    ASSERT_NE(manager_, nullptr);

    manager_->OnRequestIdReleased(request_id);
  }

  size_t ClientCount() { return manager_->controls_map_.size(); }

  std::unique_ptr<WebAppSystemMediaControlsManager> manager_;
};

// Testing adding one and removing one client works.
TEST_F(WebAppSystemMediaControlsManagerTest, BasicControlsRetrieval) {
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  AddClient(token1);
  ASSERT_TRUE(ClientCount() == 1);
  ASSERT_TRUE(manager_->IsActive());
  ASSERT_TRUE(manager_->GetAllControls().size() == 1);
  RemoveClient(token1);
  ASSERT_TRUE(ClientCount() == 0);
  ASSERT_FALSE(manager_->IsActive());
}

// Testing removing non-sensical request_id has no effect.
TEST_F(WebAppSystemMediaControlsManagerTest, ControlsRetrievalNonExistant) {
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  ASSERT_TRUE(ClientCount() == 0);
  ASSERT_FALSE(manager_->IsActive());
  RemoveClient(token1);
  ASSERT_TRUE(ClientCount() == 0);
  ASSERT_FALSE(manager_->IsActive());
}

// Test retrieval of WebAppSystemMediaControls via request ID works.
TEST_F(WebAppSystemMediaControlsManagerTest,
       WebAppSystemMediaControlsRetrieval) {
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  WebAppSystemMediaControls* controls =
      AddClientWithWebAppSystemMediaControls(token1);
  ASSERT_EQ(manager_->GetControlsForRequestId(token1), controls);
}

// Test retrieval of WebAppSystemMediaControls via passing SMC works.
TEST_F(WebAppSystemMediaControlsManagerTest, RetrievalViaSystemMediaControls) {
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  WebAppSystemMediaControls* webapp_system_media_controls;
  SystemMediaControls* system_media_controls;

  // Add an entry but keep the pointers to web_app_smc and smc.
  AddClientWithMockSystemMediaControls(token1, &webapp_system_media_controls,
                                       &system_media_controls);

  // Retrieve the web_app_smc using smc as the key.
  ASSERT_EQ(manager_->GetWebAppSystemMediaControlsForSystemMediaControls(
                system_media_controls),
            webapp_system_media_controls);
}

// Testing adding one and removing one client works.
TEST_F(WebAppSystemMediaControlsManagerTest, AddMany) {
  // Insert 1,2,3,4,5.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  base::UnguessableToken token3 = base::UnguessableToken::Create();
  base::UnguessableToken token4 = base::UnguessableToken::Create();
  base::UnguessableToken token5 = base::UnguessableToken::Create();
  AddClient(token1);
  AddClient(token2);
  AddClient(token3);
  AddClient(token4);
  AddClient(token5);
  ASSERT_EQ(ClientCount(), (size_t)5);
  ASSERT_TRUE(manager_->IsActive());
  ASSERT_EQ(manager_->GetAllControls().size(), (size_t)5);

  // Remove 3.
  // Currently: 1,2,4,5 inserted.
  RemoveClient(token3);
  ASSERT_EQ(ClientCount(), (size_t)4);
  ASSERT_TRUE(manager_->IsActive());

  // Insert 6.
  // 1,2,4,5,6 inserted.
  base::UnguessableToken token6 = base::UnguessableToken::Create();
  AddClient(token6);
  ASSERT_EQ(ClientCount(), (size_t)5);
  // 2 should still be there
  ASSERT_TRUE(manager_->GetControlsForRequestId(token2));
  // 3 is gone
  ASSERT_EQ(manager_->GetControlsForRequestId(token3), nullptr);

  // Remove 1,2,4,5,6
  RemoveClient(token1);
  RemoveClient(token2);
  RemoveClient(token4);
  RemoveClient(token5);
  RemoveClient(token6);
  ASSERT_EQ(ClientCount(), (size_t)0);
  ASSERT_FALSE(manager_->IsActive());
}

}  // namespace content
