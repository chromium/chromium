// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_

#include <stddef.h>

#include <string>
#include <variant>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/origin.h"

class GURL;

namespace content {

class RenderFrameHost;
class SharedStorageRuntimeManager;
class StoragePartition;
class TestSharedStorageHeaderObserver;

using FencedFrameNavigationTarget = std::variant<GURL, std::string>;
using OperationResult = storage::SharedStorageManager::OperationResult;
using MethodWithOptionsPtr =
    network::mojom::SharedStorageModifierMethodWithOptionsPtr;

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomSetMethod(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    std::optional<std::string> with_lock = std::nullopt);

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomAppendMethod(
    const std::u16string& key,
    const std::u16string& value,
    std::optional<std::string> with_lock = std::nullopt);

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomDeleteMethod(
    const std::u16string& key,
    std::optional<std::string> with_lock = std::nullopt);

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomClearMethod(
    std::optional<std::string> with_lock = std::nullopt);

std::vector<MethodWithOptionsPtr> CloneSharedStorageMethods(
    const std::vector<MethodWithOptionsPtr>& methods_with_options);

std::string SerializeSharedStorageMethods(
    const std::vector<MethodWithOptionsPtr>& methods_with_options);

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

// Bundles the request (`request_origin`, `methods_with_options`, and
// `with_lock`) with the result `success`.
struct SharedStorageWriteOperationAndResult {
  SharedStorageWriteOperationAndResult(
      const url::Origin& request_origin,
      std::vector<MethodWithOptionsPtr> methods_with_options,
      const std::optional<std::string>& with_lock,
      bool success);

  SharedStorageWriteOperationAndResult(
      const SharedStorageWriteOperationAndResult& other) = delete;
  SharedStorageWriteOperationAndResult& operator=(
      const SharedStorageWriteOperationAndResult& other) = delete;

  SharedStorageWriteOperationAndResult(
      SharedStorageWriteOperationAndResult&& other);
  SharedStorageWriteOperationAndResult& operator=(
      SharedStorageWriteOperationAndResult&& other);

  ~SharedStorageWriteOperationAndResult();

  friend bool operator==(const SharedStorageWriteOperationAndResult& a,
                         const SharedStorageWriteOperationAndResult& b) =
      default;

  url::Origin request_origin;
  std::vector<MethodWithOptionsPtr> methods_with_options;
  std::optional<std::string> with_lock;
  bool success;
};

std::ostream& operator<<(std::ostream& os,
                         const SharedStorageWriteOperationAndResult& op);

SharedStorageWriteOperationAndResult HeaderOperationSuccess(
    const url::Origin& request_origin,
    std::vector<MethodWithOptionsPtr> methods_with_options);

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
