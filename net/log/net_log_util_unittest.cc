// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_util.h"

#include <set>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/net_info_source_list.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction.h"
#include "net/http/mock_http_cache.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
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

  auto context = CreateTestURLRequestContextBuilder()->Build();
  HttpCache* http_cache = context->http_transaction_factory()->GetCache();

  // Get NetInfo when there's no cache backend (It's only created on first use).
  EXPECT_FALSE(http_cache->GetCurrentBackend());
  base::Value::Dict net_info_without_cache(GetNetInfo(context.get()));
  EXPECT_FALSE(http_cache->GetCurrentBackend());
  EXPECT_GT(net_info_without_cache.size(), 0u);

  // Force creation of a cache backend, and get NetInfo again.
  auto [rv, _] = context->http_transaction_factory()->GetCache()->GetBackend(
      TestGetBackendCompletionCallback().callback());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(http_cache->GetCurrentBackend());
  base::Value::Dict net_info_with_cache = GetNetInfo(context.get());
  EXPECT_GT(net_info_with_cache.size(), 0u);

  EXPECT_EQ(net_info_without_cache.size(), net_info_with_cache.size());
}

// Verify that active Field Trials are reflected.
TEST(NetLogUtil, GetNetInfoIncludesFieldTrials) {
  base::test::TaskEnvironment task_environment;

  // Clear all Field Trials.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(
      std::make_unique<base::FeatureList>());

  // Add and activate a new Field Trial.
  base::FieldTrialList::CreateFieldTrial("NewFieldTrial", "Active");
  EXPECT_EQ(base::FieldTrialList::FindFullName("NewFieldTrial"), "Active");

  auto context = CreateTestURLRequestContextBuilder()->Build();
  base::Value net_info(GetNetInfo(context.get()));

  // Verify that the returned information reflects the new trial.
  ASSERT_TRUE(net_info.is_dict());
  base::Value::List* trials =
      net_info.GetDict().FindList("activeFieldTrialGroups");
  ASSERT_NE(nullptr, trials);
  EXPECT_EQ(1u, trials->size());
  EXPECT_TRUE((*trials)[0].is_string());
  EXPECT_EQ("NewFieldTrial:Active", (*trials)[0].GetString());
}

// Demonstrate that disabling a provider causes it to be added to the list of
// disabled DoH providers.
//
// TODO(crbug.com/40218379) Stop using the real DoH provider list.
TEST(NetLogUtil, GetNetInfoIncludesDisabledDohProviders) {
  constexpr std::string_view kArbitraryProvider = "Google";
  base::test::TaskEnvironment task_environment;

  for (bool provider_enabled : {false, true}) {
    // Get the DoH provider entry.
    auto provider_list = net::DohProviderEntry::GetList();
    auto provider_it = base::ranges::find(provider_list, kArbitraryProvider,
                                          &net::DohProviderEntry::provider);
    CHECK(provider_it != provider_list.end());
    const DohProviderEntry& provider_entry = **provider_it;

    // Enable or disable the provider's feature according to `provider_enabled`.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(provider_entry.feature.get(),
                                             provider_enabled);
    EXPECT_EQ(provider_enabled,
              base::FeatureList::IsEnabled(provider_entry.feature.get()));

    // Verify that the provider is present in the list of disabled providers iff
    // we disabled it.
    auto context = CreateTestURLRequestContextBuilder()->Build();
    base::Value net_info(GetNetInfo(context.get()));
    ASSERT_TRUE(net_info.is_dict());
    const base::Value::List* disabled_doh_providers_list =
        net_info.GetDict().FindList(kNetInfoDohProvidersDisabledDueToFeature);
    CHECK(disabled_doh_providers_list);
    EXPECT_EQ(!provider_enabled,
              base::Contains(*disabled_doh_providers_list,
                             base::Value(kArbitraryProvider)));
  }
}

// Make sure CreateNetLogEntriesForActiveObjects works for requests from a
// single URLRequestContext.
TEST(NetLogUtil, CreateNetLogEntriesForActiveObjectsOneContext) {
  base::test::TaskEnvironment task_environment;

  // Using same context for each iteration makes sure deleted requests don't
  // appear in the list, or result in crashes.
  auto context = CreateTestURLRequestContextBuilder()->Build();
  TestDelegate delegate;
  for (size_t num_requests = 0; num_requests < 5; ++num_requests) {
    std::vector<std::unique_ptr<URLRequest>> requests;
    for (size_t i = 0; i < num_requests; ++i) {
      requests.push_back(context->CreateRequest(GURL("about:life"),
                                                DEFAULT_PRIORITY, &delegate,
                                                TRAFFIC_ANNOTATION_FOR_TESTS));
    }
    std::set<URLRequestContext*> contexts;
    contexts.insert(context.get());
    RecordingNetLogObserver net_log_observer;
    CreateNetLogEntriesForActiveObjects(contexts, &net_log_observer);
    auto entry_list = net_log_observer.GetEntries();
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
    std::vector<std::unique_ptr<URLRequestContext>> contexts;
    std::vector<std::unique_ptr<URLRequest>> requests;
    std::set<URLRequestContext*> context_set;
    for (size_t i = 0; i < num_requests; ++i) {
      contexts.push_back(CreateTestURLRequestContextBuilder()->Build());
      context_set.insert(contexts[i].get());
      requests.push_back(
          contexts[i]->CreateRequest(GURL("about:hats"), DEFAULT_PRIORITY,
                                     &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    }
    RecordingNetLogObserver net_log_observer;
    CreateNetLogEntriesForActiveObjects(context_set, &net_log_observer);
    auto entry_list = net_log_observer.GetEntries();
    ASSERT_EQ(num_requests, entry_list.size());

    for (size_t i = 0; i < num_requests; ++i) {
      EXPECT_EQ(entry_list[i].source.id, requests[i]->net_log().source().id);
    }
  }
}

}  // namespace

}  // namespace net
