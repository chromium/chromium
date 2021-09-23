// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_aggregation_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/aggregation_service/public_key_parsing_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TestAggregationServiceImpl::TestAggregationServiceImpl(const base::Clock* clock)
    : clock_(*clock),
      storage_(base::SequenceBound<AggregationServiceStorageSql>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          /*run_in_memory=*/true,
          /*path_to_database=*/base::FilePath(),
          clock)) {
  DCHECK(clock);
}

TestAggregationServiceImpl::~TestAggregationServiceImpl() = default;

const base::SequenceBound<AggregationServiceKeyStorage>&
TestAggregationServiceImpl::GetKeyStorage() {
  return storage_;
}

void TestAggregationServiceImpl::SetPublicKeys(
    const url::Origin& origin,
    const std::string& json_string,
    base::OnceCallback<void(bool)> callback) {
  JSONStringValueDeserializer deserializer(json_string);
  std::string error_message;
  std::unique_ptr<base::Value> value_ptr =
      deserializer.Deserialize(nullptr, &error_message);
  if (!value_ptr) {
    LOG(ERROR) << "Unable to deserialze json string: " << json_string
               << ", error: " << error_message;
    std::move(callback).Run(false);
    return;
  }

  PublicKeyset keyset(aggregation_service::GetPublicKeys(*value_ptr),
                      /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_.AsyncCall(&AggregationServiceKeyStorage::SetPublicKeys)
      .WithArgs(origin, std::move(keyset))
      .Then(base::BindOnce(std::move(callback), true));
}

void TestAggregationServiceImpl::SetURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  sender_->SetURLLoaderFactoryForTesting(url_loader_factory);
}

void TestAggregationServiceImpl::SendReport(
    const GURL& url,
    const base::Value& contents,
    base::OnceCallback<void(bool)> callback) {
  sender_->SendReport(
      url, contents,
      base::BindOnce(
          [&](base::OnceCallback<void(bool)> callback,
              AggregatableReportSender::RequestStatus status) {
            std::move(callback).Run(
                status == AggregatableReportSender::RequestStatus::kOk);
          },
          std::move(callback)));
}

void TestAggregationServiceImpl::GetPublicKeys(
    const url::Origin& origin,
    base::OnceCallback<void(std::vector<PublicKey>)> callback) const {
  storage_.AsyncCall(&AggregationServiceKeyStorage::GetPublicKeys)
      .WithArgs(origin)
      .Then(std::move(callback));
}

}  // namespace content