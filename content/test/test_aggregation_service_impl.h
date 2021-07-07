// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_
#define CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregatable_report_manager.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/test/test_aggregation_service.h"
#include "url/origin.h"

namespace content {

// Implementation class of a test aggregation service.
class TestAggregationServiceImpl : public AggregatableReportManager,
                                   public TestAggregationService {
 public:
  TestAggregationServiceImpl();
  TestAggregationServiceImpl(const TestAggregationServiceImpl& other) = delete;
  TestAggregationServiceImpl& operator=(
      const TestAggregationServiceImpl& other) = delete;
  ~TestAggregationServiceImpl() override;

  // AggregatableReportManager:
  const base::SequenceBound<AggregationServiceKeyStorage>& GetKeyStorage()
      override;

  // TestAggregationService:
  void SetPublicKeys(const url::Origin& origin,
                     const std::string& json_string,
                     base::OnceCallback<void(bool)> callback) override;

  void GetPublicKeys(
      const url::Origin& origin,
      base::OnceCallback<void(PublicKeysForOrigin)> callback) const;

 private:
  base::SequenceBound<AggregationServiceKeyStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_AGGREGATION_SERVICE_IMPL_H_