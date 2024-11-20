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
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

class GURL;

namespace content {

class RenderFrameHost;
class SharedStorageRuntimeManager;
class StoragePartition;
class TestSharedStorageHeaderObserver;

using FencedFrameNavigationTarget = absl::variant<GURL, std::string>;
using OperationResult = storage::SharedStorageManager::OperationResult;
using MethodPtr = network::mojom::SharedStorageModifierMethodPtr;

network::mojom::SharedStorageModifierMethodPtr MojomSetMethod(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present);

network::mojom::SharedStorageModifierMethodPtr MojomAppendMethod(
    const std::u16string& key,
    const std::u16string& value);

network::mojom::SharedStorageModifierMethodPtr MojomDeleteMethod(
    const std::u16string& key);

network::mojom::SharedStorageModifierMethodPtr MojomClearMethod();

SharedStorageRuntimeManager* GetSharedStorageRuntimeManagerForStoragePartition(
    StoragePartition* storage_partition);

std::string GetFencedStorageReadDisabledMessage();

std::string GetFencedStorageReadWithoutRevokeNetworkMessage();

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

// In order to use gmock matchers, it's necessary to wrap `method` into a
// copyable struct. We also bundle them with the `request_origin` and `result`.
struct SharedStorageWriteOperationAndResult {
  SharedStorageWriteOperationAndResult(const url::Origin& request_origin,
                                       MethodPtr method,
                                       OperationResult result);

  SharedStorageWriteOperationAndResult(
      const SharedStorageWriteOperationAndResult& other);
  SharedStorageWriteOperationAndResult& operator=(
      const SharedStorageWriteOperationAndResult& other);

  ~SharedStorageWriteOperationAndResult();

  friend bool operator==(const SharedStorageWriteOperationAndResult& a,
                         const SharedStorageWriteOperationAndResult& b) =
      default;

  url::Origin request_origin;
  MethodPtr method;
  OperationResult result;
};

std::ostream& operator<<(std::ostream& os,
                         const SharedStorageWriteOperationAndResult& op);

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
