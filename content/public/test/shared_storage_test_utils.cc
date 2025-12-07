// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/shared_storage_test_utils.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/to_string.h"
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
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace content {

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomSetMethod(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    std::optional<std::string> with_lock) {
  auto method = network::mojom::SharedStorageModifierMethod::NewSetMethod(
      network::mojom::SharedStorageSetMethod::New(key, value,
                                                  ignore_if_present));

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomAppendMethod(
    const std::u16string& key,
    const std::u16string& value,
    std::optional<std::string> with_lock) {
  auto method = network::mojom::SharedStorageModifierMethod::NewAppendMethod(
      network::mojom::SharedStorageAppendMethod::New(key, value));

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomDeleteMethod(
    const std::u16string& key,
    std::optional<std::string> with_lock) {
  auto method = network::mojom::SharedStorageModifierMethod::NewDeleteMethod(
      network::mojom::SharedStorageDeleteMethod::New(key));

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

network::mojom::SharedStorageModifierMethodWithOptionsPtr MojomClearMethod(
    std::optional<std::string> with_lock) {
  auto method = network::mojom::SharedStorageModifierMethod::NewClearMethod(
      network::mojom::SharedStorageClearMethod::New());

  return network::mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
}

std::vector<MethodWithOptionsPtr> CloneSharedStorageMethods(
    const std::vector<MethodWithOptionsPtr>& methods_with_options) {
  std::vector<MethodWithOptionsPtr> cloned_methods_with_options;
  cloned_methods_with_options.reserve(methods_with_options.size());
  for (auto& method_with_options : methods_with_options) {
    cloned_methods_with_options.push_back(method_with_options.Clone());
  }
  return cloned_methods_with_options;
}

std::string SerializeSharedStorageMethods(
    const std::vector<MethodWithOptionsPtr>& methods_with_options) {
  std::stringstream ss;

  ss << "{";

  for (size_t i = 0; i < methods_with_options.size(); ++i) {
    const MethodWithOptionsPtr& method_with_options = methods_with_options[i];

    ss << "{";

    switch (method_with_options->method->which()) {
      case network::mojom::SharedStorageModifierMethod::Tag::kSetMethod: {
        network::mojom::SharedStorageSetMethodPtr& set_method =
            method_with_options->method->get_set_method();
        ss << "Set(" << set_method->key << "," << set_method->value << ","
           << base::ToString(set_method->ignore_if_present) << ")";
        break;
      }
      case network::mojom::SharedStorageModifierMethod::Tag::kAppendMethod: {
        network::mojom::SharedStorageAppendMethodPtr& append_method =
            method_with_options->method->get_append_method();
        ss << "Append(" << append_method->key << "," << append_method->value
           << ")";
        break;
      }
      case network::mojom::SharedStorageModifierMethod::Tag::kDeleteMethod: {
        network::mojom::SharedStorageDeleteMethodPtr& delete_method =
            method_with_options->method->get_delete_method();
        ss << "Delete(" << delete_method->key << ")";
        break;
      }
      case network::mojom::SharedStorageModifierMethod::Tag::kClearMethod: {
        ss << "Clear()";
        break;
      }
    }

    const std::optional<std::string>& with_lock =
        method_with_options->with_lock;
    if (with_lock) {
      ss << "; WithLock: " << with_lock.value();
    }

    ss << "}";

    if (i != methods_with_options.size() - 1) {
      ss << "; ";
    }
  }

  ss << "}";

  return ss.str();
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

  EvalJsResult result =
      EvalJs(root,
             std::visit(absl::Overload{
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

  EXPECT_TRUE(result.is_ok());

  return fenced_frame_root_node->current_frame_host();
}

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    const url::Origin& request_origin,
    std::vector<MethodWithOptionsPtr> methods_with_options,
    const std::optional<std::string>& with_lock,
    bool success)
    : request_origin(request_origin),
      methods_with_options(std::move(methods_with_options)),
      with_lock(with_lock),
      success(success) {}

SharedStorageWriteOperationAndResult::SharedStorageWriteOperationAndResult(
    SharedStorageWriteOperationAndResult&& other) = default;

SharedStorageWriteOperationAndResult&
SharedStorageWriteOperationAndResult::operator=(
    SharedStorageWriteOperationAndResult&& other) = default;

SharedStorageWriteOperationAndResult::~SharedStorageWriteOperationAndResult() =
    default;

std::ostream& operator<<(std::ostream& os,
                         const SharedStorageWriteOperationAndResult& op) {
  os << "Request Origin: " << op.request_origin;
  os << "; Methods: " << SerializeSharedStorageMethods(op.methods_with_options);

  const std::optional<std::string>& with_lock = op.with_lock;
  if (with_lock) {
    os << "; WithLock (batch): " << with_lock.value();
  }

  os << "; Result: " << (op.success ? "Success" : "Failure");

  return os;
}

SharedStorageWriteOperationAndResult HeaderOperationSuccess(
    const url::Origin& request_origin,
    std::vector<MethodWithOptionsPtr> methods_with_options) {
  return SharedStorageWriteOperationAndResult(request_origin,
                                              std::move(methods_with_options),
                                              /*with_lock=*/std::nullopt,
                                              /*success=*/true);
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
