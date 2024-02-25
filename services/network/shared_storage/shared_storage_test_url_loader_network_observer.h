// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_TEST_URL_LOADER_NETWORK_OBSERVER_H_
#define SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_TEST_URL_LOADER_NETWORK_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/test/test_url_loader_network_observer.h"
#include "url/origin.h"

namespace network {

class SharedStorageTestURLLoaderNetworkObserver
    : public TestURLLoaderNetworkObserver {
 public:
  SharedStorageTestURLLoaderNetworkObserver();
  ~SharedStorageTestURLLoaderNetworkObserver() override;

  const std::vector<
      std::pair<url::Origin,
                std::vector<std::tuple<mojom::SharedStorageOperationType,
                                       std::optional<std::string>,
                                       std::optional<std::string>,
                                       std::optional<bool>>>>>&
  headers_received() const {
    return headers_received_;
  }

  // TestURLLoaderNetworkObserver:
  void OnSharedStorageHeaderReceived(
      const url::Origin& request_origin,
      std::vector<mojom::SharedStorageOperationPtr> operations,
      OnSharedStorageHeaderReceivedCallback callback) override;

  void WaitForHeadersReceived(size_t expected_total);

 private:
  std::unique_ptr<base::RunLoop> loop_;
  size_t expected_total_ = 0;
  std::vector<
      std::pair<url::Origin,
                std::vector<std::tuple<mojom::SharedStorageOperationType,
                                       std::optional<std::string>,
                                       std::optional<std::string>,
                                       std::optional<bool>>>>>
      headers_received_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_TEST_URL_LOADER_NETWORK_OBSERVER_H_
