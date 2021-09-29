// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_manager.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

namespace aggregation_service {

struct TestHpkeKey {
  // Public-private key pair.
  EVP_HPKE_KEY full_hpke_key;

  // Contains a copy of the public key of `full_hpke_key`.
  PublicKey public_key;
};

testing::AssertionResult PublicKeysEqual(const std::vector<PublicKey>& expected,
                                         const std::vector<PublicKey>& actual);
testing::AssertionResult SharedInfoEqual(
    const AggregatableReportSharedInfo& expected,
    const AggregatableReportSharedInfo& actual);

AggregatableReportRequest CreateExampleRequest();

// Generates a public-private key pair for HPKE and also constructs a PublicKey
// object for use in assembler methods.
TestHpkeKey GenerateKey(std::string key_id = "example_id");

}  // namespace aggregation_service

// The strings "ABCD1234" and "EFGH5678", respectively, Base64-decoded to bytes.
const std::vector<uint8_t> kABCD1234AsBytes = {0, 16, 131, 215, 109, 248};
const std::vector<uint8_t> kEFGH5678AsBytes = {16, 81, 135, 231, 174, 252};

class TestAggregatableReportManager : public AggregatableReportManager {
 public:
  explicit TestAggregatableReportManager(const base::Clock* clock);
  TestAggregatableReportManager(const TestAggregatableReportManager& other) =
      delete;
  TestAggregatableReportManager& operator=(
      const TestAggregatableReportManager& other) = delete;
  ~TestAggregatableReportManager() override;

  // AggregatableReportManager:
  const base::SequenceBound<content::AggregationServiceKeyStorage>&
  GetKeyStorage() override;

 private:
  base::SequenceBound<content::AggregationServiceKeyStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TEST_UTILS_H_
