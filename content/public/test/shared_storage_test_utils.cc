// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/shared_storage_test_utils.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_shared_storage_header_observer.h"
#include "content/test/fenced_frame_test_utils.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

std::string SharedStorageOperationTypeToString(OperationType operation_type) {
  switch (operation_type) {
    case OperationType::kSet:
      return "Set";
    case OperationType::kAppend:
      return "Append";
    case OperationType::kDelete:
      return "Delete";
    case OperationType::kClear:
      return "Clear";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "None";
}

std::string SharedStorageOperationResultToString(OperationResult result) {
  switch (result) {
    case OperationResult::kSuccess:
      return "Success";
    case OperationResult::kSet:
      return "Set";
    case OperationResult::kIgnored:
      return "Ignored";
    case OperationResult::kSqlError:
      return "SqlError";
    case OperationResult::kInitFailure:
      return "InitFailure";
    case OperationResult::kNoCapacity:
      return "NoCapacity";
    case OperationResult::kInvalidAppend:
      return "InvalidAppend";
    case OperationResult::kNotFound:
      return "NotFound";
    case OperationResult::kTooManyFound:
      return "TooManyFound";
    case OperationResult::kExpired:
      return "Expired";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "None";
}

std::string OptionalStringToString(const std::optional<std::string>& str) {
  return str.has_value() ? str.value() : "[[NULL]]";
}

std::string OptionalBoolToString(std::optional<bool> opt_bool) {
  return opt_bool.has_value() ? (opt_bool.value() ? "true" : "false")
                              : "[[NULL]]";
}

std::optional<bool> MojomToAbslOptionalBool(
    network::mojom::OptionalBool opt_bool) {
  std::optional<bool> converted;
  if (opt_bool == network::mojom::OptionalBool::kTrue) {
    converted = true;
  } else if (opt_bool == network::mojom::OptionalBool::kFalse) {
    converted = false;
  }
  return converted;
}

}  // namespace

SharedStorageWorkletHostManager*
GetSharedStorageWorkletHostManagerForStoragePartition(
    StoragePartition* storage_partition) {
  return static_cast<StoragePartitionImpl*>(storage_partition)
      ->GetSharedStorageWorkletHostManager();
}

std::string GetSharedStorageDisabledMessage() {
  return kSharedStorageDisabledMessage;
}

std::string GetSharedStorageSelectURLDisabledMessage() {
  return kSharedStorageSelectURLDisabledMessage;
}

std::string GetSharedStorageAddModuleDisabledMessage() {
  return kSharedStorageAddModuleDisabledMessage;
}

size_t GetAttachedSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition) {
  SharedStorageWorkletHostManager* manager =
      GetSharedStorageWorkletHostManagerForStoragePartition(storage_partition);
  DCHECK(manager);

  size_t count = 0;
  for (const auto& [document_service, worklet_hosts] :
       manager->GetAttachedWorkletHostsForTesting()) {
    count += worklet_hosts.size();
  }

  return count;
}

size_t GetKeepAliveSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition) {
  SharedStorageWorkletHostManager* manager =
      GetSharedStorageWorkletHostManagerForStoragePartition(storage_partition);
  DCHECK(manager);
  return manager->GetKeepAliveWorkletHostsForTesting().size();
}

RenderFrameHost* CreateFencedFrame(RenderFrameHost* root,
                                   const FencedFrameNavigationTarget& target) {
  FrameTreeNode* root_node =
      static_cast<RenderFrameHostImpl*>(root)->frame_tree_node();
  size_t initial_child_count = root_node->child_count();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(initial_child_count + 1, root_node->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root_node->child_at(initial_child_count));

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EvalJsResult result = EvalJs(
      root, absl::visit(base::Overloaded{
                            [](const GURL& url) {
                              return JsReplace(
                                  "f.config = new FencedFrameConfig($1);", url);
                            },
                            [](const std::string& config) {
                              return JsReplace("f.config =  window[$1]",
                                               config);
                            },
                        },
                        target));

  observer.Wait();

  EXPECT_TRUE(result.error.empty());

  return fenced_frame_root_node->current_frame_host();
}

network::mojom::OptionalBool AbslToMojomOptionalBool(
    std::optional<bool> opt_bool) {
  return opt_bool.has_value()
             ? (opt_bool.value() ? network::mojom::OptionalBool::kTrue
                                 : network::mojom::OptionalBool::kFalse)
             : network::mojom::OptionalBool::kUnset;
}

// static
SharedStorageWriteOperationAndResult
SharedStorageWriteOperationAndResult::SetOperation(
    const url::Origin& request_origin,
    std::string key,
    std::string value,
    std::optional<bool> ignore_if_present,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult(
      request_origin, OperationType::kSet, std::move(key), std::move(value),
      ignore_if_present, result);
}

// static
SharedStorageWriteOperationAndResult
SharedStorageWriteOperationAndResult::AppendOperation(
    const url::Origin& request_origin,
    std::string key,
    std::string value,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult(
      request_origin, OperationType::kAppend, std::move(key), std::move(value),
      /*ignore_if_present=*/std::nullopt, result);
}

// static
SharedStorageWriteOperationAndResult
SharedStorageWriteOperationAndResult::DeleteOperation(
    const url::Origin& request_origin,
    std::string key,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult(
      request_origin, OperationType::kDelete, std::move(key),
      /*value=*/std::nullopt,
      /*ignore_if_present=*/std::nullopt, result);
}

// static
SharedStorageWriteOperationAndResult
SharedStorageWriteOperationAndResult::ClearOperation(
    const url::Origin& request_origin,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult(
      request_origin, OperationType::kClear,
      /*key=*/std::nullopt,
      /*value=*/std::nullopt,
      /*ignore_if_present=*/std::nullopt, result);
}

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    const url::Origin& request_origin,
    OperationType operation_type,
    std::optional<std::string> key,
    std::optional<std::string> value,
    std::optional<bool> ignore_if_present,
    OperationResult result)
    : request_origin(request_origin),
      operation_type(operation_type),
      key(std::move(key)),
      value(std::move(value)),
      ignore_if_present(ignore_if_present),
      result(result) {}

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    const url::Origin& request_origin,
    OperationType operation_type,
    std::optional<std::string> key,
    std::optional<std::string> value,
    network::mojom::OptionalBool ignore_if_present,
    OperationResult result)
    : SharedStorageWriteOperationAndResult(
          request_origin,
          operation_type,
          std::move(key),
          std::move(value),
          MojomToAbslOptionalBool(ignore_if_present),
          result) {}

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    const url::Origin& request_origin,
    OperationPtr operation,
    OperationResult result)
    : SharedStorageWriteOperationAndResult(request_origin,
                                           operation->type,
                                           std::move(operation->key),
                                           std::move(operation->value),
                                           operation->ignore_if_present,
                                           result) {}

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    const SharedStorageWriteOperationAndResult&) = default;

SharedStorageWriteOperationAndResult::~SharedStorageWriteOperationAndResult() =
    default;

bool operator==(const SharedStorageWriteOperationAndResult& a,
                const SharedStorageWriteOperationAndResult& b) {
  if (a.request_origin != b.request_origin) {
    LOG(ERROR) << "request_origin mismatch: " << a.request_origin.Serialize()
               << " <-> " << b.request_origin.Serialize();
  }
  if (a.operation_type != b.operation_type) {
    LOG(ERROR) << "operation_type mismatch: "
               << SharedStorageOperationTypeToString(a.operation_type)
               << " <-> "
               << SharedStorageOperationTypeToString(b.operation_type);
  }
  if (a.key != b.key) {
    LOG(ERROR) << "key mismatch: " << OptionalStringToString(a.key) << " <-> "
               << OptionalStringToString(b.key);
  }
  if (a.value != b.value) {
    LOG(ERROR) << "value mismatch: " << OptionalStringToString(a.value)
               << " <-> " << OptionalStringToString(b.value);
  }
  if (a.ignore_if_present != b.ignore_if_present) {
    LOG(ERROR) << "ignore_if_present mismatch: "
               << OptionalBoolToString(a.ignore_if_present) << " <-> "
               << OptionalBoolToString(b.ignore_if_present);
  }
  if (a.result != b.result) {
    LOG(ERROR) << "result mismatch: "
               << SharedStorageOperationResultToString(a.result) << " <-> "
               << SharedStorageOperationResultToString(b.result);
  }

  return a.request_origin == b.request_origin &&
         a.operation_type == b.operation_type && a.key == b.key &&
         a.value == b.value && a.ignore_if_present == b.ignore_if_present &&
         a.result == b.result;
}

PrivateAggregationHost::PipeResult
GetPrivateAggregationHostPipeReportSuccessValue() {
  return PrivateAggregationHost::PipeResult::kReportSuccess;
}

PrivateAggregationHost::PipeResult
GetPrivateAggregationHostPipeApiDisabledValue() {
  return PrivateAggregationHost::PipeResult::kApiDisabledInSettings;
}

base::WeakPtr<TestSharedStorageHeaderObserver>
CreateAndOverrideSharedStorageHeaderObserver(StoragePartition* partition) {
  auto observer = std::make_unique<TestSharedStorageHeaderObserver>(partition);
  auto observer_ptr = observer->GetMutableWeakPtr();
  static_cast<StoragePartitionImpl*>(partition)
      ->OverrideSharedStorageHeaderObserverForTesting(std::move(observer));
  return observer_ptr;
}

base::StringPairs SharedStorageCrossOriginWorkletResponseHeaderReplacement(
    const std::string& access_control_allow_origin_replacement,
    const std::string& shared_storage_cross_origin_allowed_replacement) {
  base::StringPairs header_replacement;
  header_replacement.emplace_back("{{ACCESS_CONTROL_ALLOW_ORIGIN_HEADER}}",
                                  access_control_allow_origin_replacement);
  header_replacement.emplace_back(
      "{{SHARED_STORAGE_CROSS_ORIGIN_WORKLET_ALLOWED_HEADER}}",
      shared_storage_cross_origin_allowed_replacement);

  return header_replacement;
}

}  // namespace content
