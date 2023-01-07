// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/log_source_access_manager.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/feedback_private/feedback_private_api_unittest_base_chromeos.h"

namespace extensions {

namespace {

using api::feedback_private::LOG_SOURCE_MESSAGES;
using api::feedback_private::LOG_SOURCE_UILATEST;
using api::feedback_private::ReadLogSourceResult;
using api::feedback_private::ReadLogSourceParams;

constexpr int kMaxReadersPerSource =
    LogSourceAccessManager::kMaxReadersPerSource;

void DummyCallback(std::unique_ptr<ReadLogSourceResult>) {}

}  // namespace

using LogSourceAccessManagerTest = FeedbackPrivateApiUnittestBase;

TEST_F(LogSourceAccessManagerTest, MaxNumberOfOpenLogSourcesSameExtension) {
  const base::TimeDelta timeout(base::Milliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  LogSourceAccessManager manager(browser_context());

  const std::string extension_id = "extension";

  // Open 10 readers for LOG_SOURCE_MESSAGES from the same extension.
  ReadLogSourceParams messages_params;
  messages_params.incremental = false;
  messages_params.source = LOG_SOURCE_MESSAGES;
  for (size_t i = 0; i < kMaxReadersPerSource; ++i) {
    EXPECT_TRUE(manager.FetchFromSource(messages_params, extension_id,
                                        base::BindOnce(&DummyCallback)))
        << base::StringPrintf("Unable to read from log source with i=%zu", i);
    EXPECT_EQ(i + 1,
              manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  }
  EXPECT_EQ(0U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Open 10 readers for LOG_SOURCE_UILATEST from the same extension.
  ReadLogSourceParams ui_latest_params;
  ui_latest_params.incremental = false;
  ui_latest_params.source = LOG_SOURCE_UILATEST;
  for (size_t i = 0; i < kMaxReadersPerSource; ++i) {
    EXPECT_TRUE(manager.FetchFromSource(ui_latest_params, extension_id,
                                        base::BindOnce(&DummyCallback)))
        << base::StringPrintf("Unable to read from log source with i=%zu", i);
    EXPECT_EQ(i + 1,
              manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));

  // Can't open more readers for LOG_SOURCE_MESSAGES or LOG_SOURCE_UILATEST.
  EXPECT_FALSE(manager.FetchFromSource(messages_params, extension_id,
                                       base::BindOnce(&DummyCallback)));
  EXPECT_FALSE(manager.FetchFromSource(ui_latest_params, extension_id,
                                       base::BindOnce(&DummyCallback)));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Wait for all asynchronous operations to complete.
  base::RunLoop().RunUntilIdle();
}

TEST_F(LogSourceAccessManagerTest,
       MaxNumberOfOpenLogSourcesDifferentExtensions) {
  const base::TimeDelta timeout(base::Milliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  LogSourceAccessManager manager(browser_context());

  int count = 0;

  // Open 10 readers for LOG_SOURCE_MESSAGES from different extensions.
  ReadLogSourceParams messages_params;
  messages_params.incremental = false;
  messages_params.source = LOG_SOURCE_MESSAGES;
  for (size_t i = 0; i < kMaxReadersPerSource; ++i, ++count) {
    EXPECT_TRUE(manager.FetchFromSource(
        messages_params, base::StringPrintf("extension %d", count),
        base::BindOnce(&DummyCallback)))
        << base::StringPrintf(
               "Unable to read from log source with i=%zu and count=%d", i,
               count);
    EXPECT_EQ(i + 1,
              manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  }
  EXPECT_EQ(0U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Open 10 readers for LOG_SOURCE_UILATEST from different extensions.
  ReadLogSourceParams ui_latest_params;
  ui_latest_params.incremental = false;
  ui_latest_params.source = LOG_SOURCE_UILATEST;
  for (size_t i = 0; i < kMaxReadersPerSource; ++i, ++count) {
    EXPECT_TRUE(manager.FetchFromSource(
        ui_latest_params, base::StringPrintf("extension %d", count),
        base::BindOnce(&DummyCallback)))
        << base::StringPrintf(
               "Unable to read from log source with i=%zu and count=%d", i,
               count);
    EXPECT_EQ(i + 1,
              manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));

  // Can't open more readers for LOG_SOURCE_MESSAGES.
  EXPECT_FALSE(manager.FetchFromSource(
      messages_params, base::StringPrintf("extension %d", count),
      base::BindOnce(&DummyCallback)));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Can't open more readers for LOG_SOURCE_UILATEST.
  EXPECT_FALSE(manager.FetchFromSource(
      ui_latest_params, base::StringPrintf("extension %d", count),
      base::BindOnce(&DummyCallback)));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Wait for all asynchronous operations to complete.
  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
