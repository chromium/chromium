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

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/origin.h"

namespace network {

SharedStorageTestURLLoaderNetworkObserver::HeaderResult::HeaderResult(
    const url::Origin& request_origin,
    std::vector<SharedStorageMethodWrapper> methods,
    const std::optional<std::string>& with_lock)
    : request_origin(request_origin),
      methods(std::move(methods)),
      with_lock(with_lock) {}

SharedStorageTestURLLoaderNetworkObserver::HeaderResult::HeaderResult(
    HeaderResult&& other) = default;

SharedStorageTestURLLoaderNetworkObserver::HeaderResult&
SharedStorageTestURLLoaderNetworkObserver::HeaderResult::operator=(
    HeaderResult&& other) = default;

SharedStorageTestURLLoaderNetworkObserver::HeaderResult::~HeaderResult() =
    default;

SharedStorageTestURLLoaderNetworkObserver::
    SharedStorageTestURLLoaderNetworkObserver() = default;
SharedStorageTestURLLoaderNetworkObserver::
    ~SharedStorageTestURLLoaderNetworkObserver() = default;

void SharedStorageTestURLLoaderNetworkObserver::OnSharedStorageHeaderReceived(
    const url::Origin& request_origin,
    std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
        methods_with_options,
    const std::optional<std::string>& with_lock,
    OnSharedStorageHeaderReceivedCallback callback) {
  std::vector<SharedStorageMethodWrapper> transformed =
      base::ToVector(methods_with_options, [](auto& methods_with_options) {
        return SharedStorageMethodWrapper(std::move(methods_with_options));
      });

  headers_received_.emplace_back(request_origin, std::move(transformed),
                                 with_lock);
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
