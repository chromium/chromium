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
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_shared_storage_header_observer.h"
#include "content/test/fenced_frame_test_utils.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

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
      NOTREACHED();
  }
}

}  // namespace

network::mojom::SharedStorageModifierMethodPtr MojomSetMethod(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present) {
  return network::mojom::SharedStorageModifierMethod::NewSetMethod(
      network::mojom::SharedStorageSetMethod::New(key, value,
                                                  ignore_if_present));
}

network::mojom::SharedStorageModifierMethodPtr MojomAppendMethod(
    const std::u16string& key,
    const std::u16string& value) {
  return network::mojom::SharedStorageModifierMethod::NewAppendMethod(
      network::mojom::SharedStorageAppendMethod::New(key, value));
}

network::mojom::SharedStorageModifierMethodPtr MojomDeleteMethod(
    const std::u16string& key) {
  return network::mojom::SharedStorageModifierMethod::NewDeleteMethod(
      network::mojom::SharedStorageDeleteMethod::New(key));
}

network::mojom::SharedStorageModifierMethodPtr MojomClearMethod() {
  return network::mojom::SharedStorageModifierMethod::NewClearMethod(
      network::mojom::SharedStorageClearMethod::New());
}

SharedStorageRuntimeManager* GetSharedStorageRuntimeManagerForStoragePartition(
    StoragePartition* storage_partition) {
  return static_cast<StoragePartitionImpl*>(storage_partition)
      ->GetSharedStorageRuntimeManager();
}

std::string GetFencedStorageReadDisabledMessage() {
  return kFencedStorageReadDisabledMessage;
}

std::string GetFencedStorageReadWithoutRevokeNetworkMessage() {
  return kFencedStorageReadWithoutRevokeNetworkMessage;
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
  SharedStorageRuntimeManager* manager =
      GetSharedStorageRuntimeManagerForStoragePartition(storage_partition);
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
  SharedStorageRuntimeManager* manager =
      GetSharedStorageRuntimeManagerForStoragePartition(storage_partition);
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

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    const url::Origin& request_origin,
    MethodPtr method,
    OperationResult result)
    : request_origin(request_origin),
      method(std::move(method)),
      result(result) {}

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    const SharedStorageWriteOperationAndResult& other)
    : request_origin(other.request_origin),
      method(other.method.Clone()),
      result(other.result) {}

SharedStorageWriteOperationAndResult&
SharedStorageWriteOperationAndResult::operator=(
    const SharedStorageWriteOperationAndResult& other) {
  if (this != &other) {
    request_origin = other.request_origin;
    method = other.method.Clone();
    result = other.result;
  }
  return *this;
}

SharedStorageWriteOperationAndResult::~SharedStorageWriteOperationAndResult() =
    default;

std::ostream& operator<<(std::ostream& os,
                         const SharedStorageWriteOperationAndResult& op) {
  os << "Request Origin: " << op.request_origin;

  switch (op.method->which()) {
    case network::mojom::SharedStorageModifierMethod::Tag::kSetMethod: {
      network::mojom::SharedStorageSetMethodPtr& set_method =
          op.method->get_set_method();
      os << "; Method: Set(" << set_method->key << "," << set_method->value
         << "," << (set_method->ignore_if_present ? "true" : "false") << ")";
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kAppendMethod: {
      network::mojom::SharedStorageAppendMethodPtr& append_method =
          op.method->get_append_method();
      os << "; Method: Append(" << append_method->key << ","
         << append_method->value << ")";
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kDeleteMethod: {
      network::mojom::SharedStorageDeleteMethodPtr& delete_method =
          op.method->get_delete_method();
      os << "; Method: Delete(" << delete_method->key << ")";
      break;
    }
    case network::mojom::SharedStorageModifierMethod::Tag::kClearMethod: {
      os << "; Method: Clear()";
      break;
    }
  }

  os << "; Result: " << SharedStorageOperationResultToString(op.result);
  return os;
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
