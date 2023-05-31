// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_header_observer.h"

#include <deque>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"

namespace content {

SharedStorageHeaderObserver::SharedStorageHeaderObserver(
    StoragePartitionImpl* storage_partition)
    : storage_partition_(storage_partition) {}

SharedStorageHeaderObserver::~SharedStorageHeaderObserver() = default;

// static
bool& SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowedForTesting() {
  return GetBypassIsSharedStorageAllowed();
}

void SharedStorageHeaderObserver::HeaderReceived(
    const url::Origin& request_origin,
    RenderFrameHost* rfh,
    std::vector<OperationPtr> operations,
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!IsSharedStorageAllowed(rfh, request_origin)) {
    // TODO(crbug.com/1434529): Log the following error message to console:
    // "'Shared-Storage-Write: shared storage is disabled."
    std::move(callback).Run();
    return;
  }

  std::deque<network::mojom::SharedStorageOperationPtr> to_process;
  to_process.insert(to_process.end(),
                    std::make_move_iterator(operations.begin()),
                    std::make_move_iterator(operations.end()));

  std::vector<bool> header_results;
  while (!to_process.empty()) {
    network::mojom::SharedStorageOperationPtr operation =
        std::move(to_process.front());
    to_process.pop_front();
    header_results.push_back(Invoke(request_origin, std::move(operation)));
  }

  OnHeaderProcessed(request_origin, header_results);
  std::move(callback).Run();
}

// static
bool& SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowed() {
  static bool should_bypass = false;
  return should_bypass;
}

bool SharedStorageHeaderObserver::Invoke(const url::Origin& request_origin,
                                         OperationPtr operation) {
  switch (operation->type) {
    case OperationType::kSet:
      if (!operation->key.has_value() || !operation->value.has_value()) {
        // TODO(crbug.com/1434529): Log the following error message to console:
        // "Shared-Storage-Write: 'set' missing parameter 'key' or 'value'."
        return false;
      }
      return Set(request_origin, std::move(operation->key.value()),
                 std::move(operation->value.value()),
                 operation->ignore_if_present);
    case OperationType::kAppend:
      if (!operation->key.has_value() || !operation->value.has_value()) {
        // TODO(crbug.com/1434529): Log the following error message to console:
        // "Shared-Storage-Write: 'append' missing parameter 'key' or 'value'."
        return false;
      }
      return Append(request_origin, std::move(operation->key.value()),
                    std::move(operation->value.value()));
    case OperationType::kDelete:
      if (!operation->key.has_value()) {
        // TODO(crbug.com/1434529): Log the following error message to console:
        // "Shared-Storage-Write: 'delete' missing parameter 'key'."
        return false;
      }
      return Delete(request_origin, std::move(operation->key.value()));
    case OperationType::kClear:
      return Clear(request_origin);
    default:
      NOTREACHED();
  }
  return false;
}

bool SharedStorageHeaderObserver::Set(
    const url::Origin& request_origin,
    std::string key,
    std::string value,
    network::mojom::OptionalBool ignore_if_present) {
  std::u16string utf16_key;
  std::u16string utf16_value;
  if (!base::UTF8ToUTF16(key.c_str(), key.size(), &utf16_key) ||
      !base::UTF8ToUTF16(value.c_str(), value.size(), &utf16_value) ||
      !blink::IsValidSharedStorageKeyStringLength(utf16_key.size()) ||
      !blink::IsValidSharedStorageValueStringLength(utf16_value.size())) {
    // TODO(crbug.com/1434529): Log the following error message to console:
    // "Shared-Storage-Write: 'set' has invalid parameter 'key' or 'value'."
    return false;
  }

  storage::SharedStorageDatabase::SetBehavior set_behavior =
      (ignore_if_present == network::mojom::OptionalBool::kTrue)
          ? storage::SharedStorageDatabase::SetBehavior::kIgnoreIfPresent
          : storage::SharedStorageDatabase::SetBehavior::kDefault;

  // TODO(crbug.com/1434529): Consider calling `NotifySharedStorageAccessed()`.
  // Would need to add a new `AccessType::kHeaderSet`.

  GetSharedStorageManager()->Set(
      request_origin, std::move(utf16_key), std::move(utf16_value),
      base::BindOnce(&SharedStorageHeaderObserver::OnOperationFinished,
                     weak_ptr_factory_.GetWeakPtr(), request_origin,
                     network::mojom::SharedStorageOperation::New(
                         OperationType::kSet, std::move(key), std::move(value),
                         ignore_if_present)),
      set_behavior);
  return true;
}

bool SharedStorageHeaderObserver::Append(const url::Origin& request_origin,

                                         std::string key,
                                         std::string value) {
  std::u16string utf16_key;
  std::u16string utf16_value;
  if (!base::UTF8ToUTF16(key.c_str(), key.size(), &utf16_key) ||
      !base::UTF8ToUTF16(value.c_str(), value.size(), &utf16_value) ||
      !blink::IsValidSharedStorageKeyStringLength(utf16_key.size()) ||
      !blink::IsValidSharedStorageValueStringLength(utf16_value.size())) {
    // TODO(crbug.com/1434529): Log the following error message to console:
    // "Shared-Storage-Write: 'append' has invalid parameter 'key' or 'value'."
    return false;
  }

  // TODO(crbug.com/1434529): Consider calling `NotifySharedStorageAccessed()`.
  // Would need to add a new `AccessType::kHeaderAppend`.

  GetSharedStorageManager()->Append(
      request_origin, std::move(utf16_key), std::move(utf16_value),
      base::BindOnce(
          &SharedStorageHeaderObserver::OnOperationFinished,
          weak_ptr_factory_.GetWeakPtr(), request_origin,
          network::mojom::SharedStorageOperation::New(
              OperationType::kAppend, std::move(key), std::move(value),
              /*ignore_if_present=*/network::mojom::OptionalBool::kUnset)));
  return true;
}

bool SharedStorageHeaderObserver::Delete(const url::Origin& request_origin,
                                         std::string key) {
  std::u16string utf16_key;
  if (!base::UTF8ToUTF16(key.c_str(), key.size(), &utf16_key) ||
      !blink::IsValidSharedStorageKeyStringLength(utf16_key.size())) {
    // TODO(crbug.com/1434529): Log the following error message to console:
    // "Shared-Storage-Write: 'delete' has invalid parameter 'key'."
    return false;
  }

  // TODO(crbug.com/1434529): Consider calling `NotifySharedStorageAccessed()`.
  // Would need to add a new `AccessType::kHeaderDelete`.

  GetSharedStorageManager()->Delete(
      request_origin, std::move(utf16_key),
      base::BindOnce(
          &SharedStorageHeaderObserver::OnOperationFinished,
          weak_ptr_factory_.GetWeakPtr(), request_origin,
          network::mojom::SharedStorageOperation::New(
              OperationType::kDelete, std::move(key), /*value=*/absl::nullopt,
              /*ignore_if_present=*/network::mojom::OptionalBool::kUnset)));
  return true;
}

bool SharedStorageHeaderObserver::Clear(const url::Origin& request_origin) {
  // TODO(crbug.com/1434529): Consider calling `NotifySharedStorageAccessed()`.
  // Would need to add a new `AccessType::kHeaderClear`.

  GetSharedStorageManager()->Clear(
      request_origin,
      base::BindOnce(
          &SharedStorageHeaderObserver::OnOperationFinished,
          weak_ptr_factory_.GetWeakPtr(), request_origin,
          network::mojom::SharedStorageOperation::New(
              OperationType::kClear,
              /*key=*/absl::nullopt, /*value=*/absl::nullopt,
              /*ignore_if_present=*/network::mojom::OptionalBool::kUnset)));
  return true;
}

storage::SharedStorageManager*
SharedStorageHeaderObserver::GetSharedStorageManager() {
  DCHECK(storage_partition_);
  storage::SharedStorageManager* shared_storage_manager =
      storage_partition_->GetSharedStorageManager();

  // This `SharedStorageHeaderObserver` is created only if
  // `kSharedStorageAPI` is enabled, in which case the `shared_storage_manager`
  // must be valid.
  DCHECK(shared_storage_manager);
  return shared_storage_manager;
}

bool SharedStorageHeaderObserver::IsSharedStorageAllowed(
    RenderFrameHost* rfh,
    const url::Origin& request_origin) {
  if (GetBypassIsSharedStorageAllowed()) {
    return true;
  }

  DCHECK(storage_partition_);
  DCHECK(storage_partition_->browser_context());

  // TODO(crbug.com/1434529): 1.Add a metric to track what how often `rfh` is
  // nullptr here.
  // 2. Cache `top_frame_origin` for the corresponding `rfh` in
  // `NavigationOrDocumentHandle` and pass in from `StoragePartitionImpl` in
  // order to avoid needing to use an opaque origin in the case where `rfh` has
  // been reset.
  url::Origin top_frame_origin =
      rfh ? rfh->GetOutermostMainFrame()->GetLastCommittedOrigin()
          : url::Origin();
  return GetContentClient()->browser()->IsSharedStorageAllowed(
      storage_partition_->browser_context(), rfh, top_frame_origin,
      request_origin);
}

}  // namespace content
