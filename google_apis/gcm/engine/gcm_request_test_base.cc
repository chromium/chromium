// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_request_test_base.h"

#include <cmath>

#include "base/strings/string_tokenizer.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

namespace {

// Backoff policy for testing registration request.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  15000,  // 15 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.5,  // 50%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 60 * 5, // 5 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

const network::TestURLLoaderFactory::PendingRequest* PendingForURL(
    network::TestURLLoaderFactory* factory,
    const std::string& url) {
  GURL gurl(url);
  std::vector<network::TestURLLoaderFactory::PendingRequest>* pending =
      factory->pending_requests();
  for (const auto& pending_request : *pending) {
    if (pending_request.request.url == gurl)
      return &pending_request;
  }
  return nullptr;
}

}  // namespace

namespace gcm {

GCMRequestTestBase::GCMRequestTestBase()
    : shared_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      retry_count_(0) {}

GCMRequestTestBase::~GCMRequestTestBase() {
}

const net::BackoffEntry::Policy& GCMRequestTestBase::GetBackoffPolicy() const {
  return kDefaultBackoffPolicy;
}

void GCMRequestTestBase::OnAboutToCompleteFetch() {}

void GCMRequestTestBase::SetResponseForURLAndComplete(
    const std::string& url,
    net::HttpStatusCode status_code,
    const std::string& response_body,
    int net_error_code) {
  if (retry_count_++)
    FastForwardToTriggerNextRetry();

  OnAboutToCompleteFetch();
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(url), network::URLLoaderCompletionStatus(net_error_code),
      network::CreateURLResponseHead(status_code), response_body));
}

const net::HttpRequestHeaders* GCMRequestTestBase::GetExtraHeadersForURL(
    const std::string& url) {
  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      PendingForURL(&test_url_loader_factory_, url);
  return pending_request ? &pending_request->request.headers : nullptr;
}

bool GCMRequestTestBase::GetUploadDataForURL(const std::string& url,
                                             std::string* data_out) {
  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      PendingForURL(&test_url_loader_factory_, url);
  if (!pending_request)
    return false;
  *data_out = network::GetUploadData(pending_request->request);
  return true;
}

void GCMRequestTestBase::VerifyFetcherUploadDataForURL(
    const std::string& url,
    std::map<std::string, std::string>* expected_pairs) {
  std::string upload_data;
  ASSERT_TRUE(GetUploadDataForURL(url, &upload_data));

  // Verify data was formatted properly.
  base::StringTokenizer data_tokenizer(upload_data, "&=");
  while (data_tokenizer.GetNext()) {
    auto iter = expected_pairs->find(data_tokenizer.token());
    ASSERT_TRUE(iter != expected_pairs->end()) << data_tokenizer.token();
    ASSERT_TRUE(data_tokenizer.GetNext()) << data_tokenizer.token();
    ASSERT_EQ(iter->second, data_tokenizer.token());
    // Ensure that none of the keys appears twice.
    expected_pairs->erase(iter);
  }

  ASSERT_EQ(0UL, expected_pairs->size());
}

void GCMRequestTestBase::FastForwardToTriggerNextRetry() {
  // Here we compute the maximum delay time by skipping the jitter fluctuation
  // that only affects in the negative way.
  int next_retry_delay_ms = kDefaultBackoffPolicy.initial_delay_ms;
  next_retry_delay_ms *=
      pow(kDefaultBackoffPolicy.multiply_factor, retry_count_);
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(next_retry_delay_ms));
}

}  // namespace gcm
