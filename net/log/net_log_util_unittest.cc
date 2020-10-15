// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_util.h"

#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Make sure GetNetConstants doesn't crash.
TEST(NetLogUtil, GetNetConstants) {
  base::Value constants(GetNetConstants());
}

// Make sure GetNetInfo doesn't crash when called on contexts with and without
// caches, and they have the same number of elements.
TEST(NetLogUtil, GetNetInfo) {
  base::test::TaskEnvironment task_environment;

  TestURLRequestContext context;
  HttpCache* http_cache = context.http_transaction_factory()->GetCache();

  // Get NetInfo when there's no cache backend (It's only created on first use).
  EXPECT_FALSE(http_cache->GetCurrentBackend());
  base::Value net_info_without_cache(GetNetInfo(&context));
  EXPECT_FALSE(http_cache->GetCurrentBackend());
  EXPECT_GT(net_info_without_cache.DictSize(), 0u);

  // Force creation of a cache backend, and get NetInfo again.
  disk_cache::Backend* backend = nullptr;
  EXPECT_EQ(OK, context.http_transaction_factory()->GetCache()->GetBackend(
                    &backend, TestCompletionCallback().callback()));
  EXPECT_TRUE(http_cache->GetCurrentBackend());
  base::Value net_info_with_cache = GetNetInfo(&context);
  EXPECT_GT(net_info_with_cache.DictSize(), 0u);

  EXPECT_EQ(net_info_without_cache.DictSize(), net_info_with_cache.DictSize());
}

// Verify that active Field Trials are reflected.
TEST(NetLogUtil, GetNetInfoIncludesFieldTrials) {
  base::test::TaskEnvironment task_environment;

  // Clear all Field Trials.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(
      std::make_unique<base::FeatureList>());

  // Add and activate a new Field Trial.
  base::FieldTrial* field_trial = base::FieldTrialList::FactoryGetFieldTrial(
      "NewFieldTrial", 100, "Default", base::FieldTrial::ONE_TIME_RANDOMIZED,
      nullptr);
  field_trial->AppendGroup("Active", 100);
  EXPECT_EQ(field_trial->group_name(), "Active");

  TestURLRequestContext context;
  base::Value net_info(GetNetInfo(&context));

  // Verify that the returned information reflects the new trial.
  ASSERT_TRUE(net_info.is_dict());
  base::Value* trials = net_info.FindListPath("activeFieldTrialGroups");
  ASSERT_NE(nullptr, trials);
  const auto& trial_list = trials->GetList();
  EXPECT_EQ(1u, trial_list.size());
  std::string result;
  EXPECT_TRUE(trial_list[0].GetAsString(&result));
  EXPECT_EQ("NewFieldTrial:Active", result);
}

// Make sure CreateNetLogEntriesForActiveObjects works for requests from a
// single URLRequestContext.
TEST(NetLogUtil, CreateNetLogEntriesForActiveObjectsOneContext) {
  base::test::TaskEnvironment task_environment;

  // Using same context for each iteration makes sure deleted requests don't
  // appear in the list, or result in crashes.
  TestURLRequestContext context(true);
  TestNetLog net_log;
  context.set_net_log(&net_log);
  context.Init();
  TestDelegate delegate;
  for (size_t num_requests = 0; num_requests < 5; ++num_requests) {
    std::vector<std::unique_ptr<URLRequest>> requests;
    for (size_t i = 0; i < num_requests; ++i) {
      requests.push_back(context.CreateRequest(GURL("about:life"),
                                               DEFAULT_PRIORITY, &delegate,
                                               TRAFFIC_ANNOTATION_FOR_TESTS));
    }
    std::set<URLRequestContext*> contexts;
    contexts.insert(&context);
    RecordingTestNetLog test_net_log;
    CreateNetLogEntriesForActiveObjects(contexts, test_net_log.GetObserver());
    auto entry_list = test_net_log.GetEntries();
    ASSERT_EQ(num_requests, entry_list.size());

    for (size_t i = 0; i < num_requests; ++i) {
      EXPECT_EQ(entry_list[i].source.id, requests[i]->net_log().source().id);
    }
  }
}

// Make sure CreateNetLogEntriesForActiveObjects works with multiple
// URLRequestContexts.
TEST(NetLogUtil, CreateNetLogEntriesForActiveObjectsMultipleContexts) {
  base::test::TaskEnvironment task_environment;

  TestDelegate delegate;
  for (size_t num_requests = 0; num_requests < 5; ++num_requests) {
    TestNetLog net_log;
    std::vector<std::unique_ptr<TestURLRequestContext>> contexts;
    std::vector<std::unique_ptr<URLRequest>> requests;
    std::set<URLRequestContext*> context_set;
    for (size_t i = 0; i < num_requests; ++i) {
      contexts.push_back(std::make_unique<TestURLRequestContext>(true));
      contexts[i]->set_net_log(&net_log);
      contexts[i]->Init();
      context_set.insert(contexts[i].get());
      requests.push_back(
          contexts[i]->CreateRequest(GURL("about:hats"), DEFAULT_PRIORITY,
                                     &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    }
    RecordingTestNetLog test_net_log;
    CreateNetLogEntriesForActiveObjects(context_set,
                                        test_net_log.GetObserver());
    auto entry_list = test_net_log.GetEntries();
    ASSERT_EQ(num_requests, entry_list.size());

    for (size_t i = 0; i < num_requests; ++i) {
      EXPECT_EQ(entry_list[i].source.id, requests[i]->net_log().source().id);
    }
  }
}

}  // namespace

}  // namespace net
