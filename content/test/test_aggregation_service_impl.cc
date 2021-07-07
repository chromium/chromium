// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_aggregation_service_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/aggregation_service/public_key_parsing_utils.h"

namespace content {

TestAggregationServiceImpl::TestAggregationServiceImpl()
    : storage_(base::SequenceBound<AggregationServiceStorage>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}))) {}

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

  content::PublicKeysForOrigin keys(
      origin, content::aggregation_service::GetPublicKeys(*value_ptr));
  storage_.AsyncCall(&AggregationServiceKeyStorage::SetPublicKeys)
      .WithArgs(std::move(keys))
      .Then(base::BindOnce(std::move(callback), true));
}

void TestAggregationServiceImpl::GetPublicKeys(
    const url::Origin& origin,
    base::OnceCallback<void(PublicKeysForOrigin)> callback) const {
  storage_.AsyncCall(&AggregationServiceKeyStorage::GetPublicKeys)
      .WithArgs(origin)
      .Then(std::move(callback));
}

}  // namespace content