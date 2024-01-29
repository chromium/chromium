// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_storage/shared_storage_test_url_loader_network_observer.h"

#include <deque>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/origin.h"

namespace network {

namespace {

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

std::vector<std::tuple<mojom::SharedStorageOperationType,
                       std::optional<std::string>,
                       std::optional<std::string>,
                       std::optional<bool>>>
MakeOperationTuples(std::vector<mojom::SharedStorageOperationPtr> operations) {
  std::deque<mojom::SharedStorageOperationPtr> to_process;
  to_process.insert(to_process.end(),
                    std::make_move_iterator(operations.begin()),
                    std::make_move_iterator(operations.end()));
  std::vector<
      std::tuple<mojom::SharedStorageOperationType, std::optional<std::string>,
                 std::optional<std::string>, std::optional<bool>>>
      operation_tuples;
  while (!to_process.empty()) {
    mojom::SharedStorageOperationPtr operation = std::move(to_process.front());
    to_process.pop_front();
    operation_tuples.emplace_back(
        operation->type, std::move(operation->key), std::move(operation->value),
        MojomToAbslOptionalBool(operation->ignore_if_present));
  }
  return operation_tuples;
}

}  // namespace

SharedStorageTestURLLoaderNetworkObserver::
    SharedStorageTestURLLoaderNetworkObserver() = default;
SharedStorageTestURLLoaderNetworkObserver::
    ~SharedStorageTestURLLoaderNetworkObserver() = default;

void SharedStorageTestURLLoaderNetworkObserver::OnSharedStorageHeaderReceived(
    const url::Origin& request_origin,
    std::vector<mojom::SharedStorageOperationPtr> operations,
    OnSharedStorageHeaderReceivedCallback callback) {
  auto operation_tuples = MakeOperationTuples(std::move(operations));
  headers_received_.emplace_back(request_origin, std::move(operation_tuples));
  if (loop_ && loop_->running() &&
      headers_received_.size() >= expected_total_) {
    loop_->Quit();
  }
  if (callback) {
    std::move(callback).Run();
  }
}

void SharedStorageTestURLLoaderNetworkObserver::WaitForHeadersReceived(
    size_t expected_total) {
  DCHECK(!loop_);
  DCHECK(!expected_total_);
  if (headers_received_.size() >= expected_total) {
    return;
  }
  expected_total_ = expected_total;
  loop_ = std::make_unique<base::RunLoop>();
  loop_->Run();
  loop_.reset();
  expected_total_ = 0;
}

}  // namespace network
