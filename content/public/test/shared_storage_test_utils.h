// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_

#include <stddef.h>

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

class GURL;

namespace content {

class RenderFrameHost;
class SharedStorageWorkletHostManager;
class StoragePartition;
class TestSharedStorageHeaderObserver;

using FencedFrameNavigationTarget = absl::variant<GURL, std::string>;
using OperationResult = storage::SharedStorageManager::OperationResult;
using OperationType = network::mojom::SharedStorageOperationType;
using OperationPtr = network::mojom::SharedStorageOperationPtr;

SharedStorageWorkletHostManager*
GetSharedStorageWorkletHostManagerForStoragePartition(
    StoragePartition* storage_partition);

std::string GetSharedStorageDisabledMessage();

std::string GetSharedStorageSelectURLDisabledMessage();

std::string GetSharedStorageAddModuleDisabledMessage();

size_t GetAttachedSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition);

size_t GetKeepAliveSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition);

// TODO(crbug.com/40256120): This function should be removed. Use
// `CreateFencedFrame` in fenced_frame_test_util.h instead.
RenderFrameHost* CreateFencedFrame(RenderFrameHost* root,
                                   const FencedFrameNavigationTarget& target);

[[nodiscard]] network::mojom::OptionalBool AbslToMojomOptionalBool(
    std::optional<bool> opt_bool);

// In order to use gmock matchers, it's necessary to copy the members of
// `OperationPtr` into a copyable struct or tuple. We also bundle them with the
// `request_origin` and `result`.
struct SharedStorageWriteOperationAndResult {
  static SharedStorageWriteOperationAndResult SetOperation(
      const url::Origin& request_origin,
      std::string key,
      std::string value,
      std::optional<bool> ignore_if_present,
      OperationResult result);
  static SharedStorageWriteOperationAndResult AppendOperation(
      const url::Origin& request_origin,
      std::string key,
      std::string value,
      OperationResult result);
  static SharedStorageWriteOperationAndResult DeleteOperation(
      const url::Origin& request_origin,
      std::string key,
      OperationResult result);
  static SharedStorageWriteOperationAndResult ClearOperation(
      const url::Origin& request_origin,
      OperationResult result);
  SharedStorageWriteOperationAndResult(const url::Origin& request_origin,
                                       OperationType operation_type,
                                       std::optional<std::string> key,
                                       std::optional<std::string> value,
                                       std::optional<bool> ignore_if_present,
                                       OperationResult result);
  SharedStorageWriteOperationAndResult(
      const url::Origin& request_origin,
      OperationType operation_type,
      std::optional<std::string> key,
      std::optional<std::string> value,
      network::mojom::OptionalBool ignore_if_present,
      OperationResult result);
  SharedStorageWriteOperationAndResult(const url::Origin& request_origin,
                                       OperationPtr operation,
                                       OperationResult result);
  SharedStorageWriteOperationAndResult(
      const SharedStorageWriteOperationAndResult&);
  ~SharedStorageWriteOperationAndResult();
  url::Origin request_origin;
  OperationType operation_type;
  std::optional<std::string> key;
  std::optional<std::string> value;
  std::optional<bool> ignore_if_present;
  OperationResult result;
};

bool operator==(const SharedStorageWriteOperationAndResult& a,
                const SharedStorageWriteOperationAndResult& b);

PrivateAggregationHost::PipeResult
GetPrivateAggregationHostPipeReportSuccessValue();

PrivateAggregationHost::PipeResult
GetPrivateAggregationHostPipeApiDisabledValue();

base::WeakPtr<TestSharedStorageHeaderObserver>
CreateAndOverrideSharedStorageHeaderObserver(StoragePartition* partition);

base::StringPairs SharedStorageCrossOriginWorkletResponseHeaderReplacement(
    const std::string& access_control_allow_origin_replacement,
    const std::string& shared_storage_cross_origin_allowed_replacement);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
