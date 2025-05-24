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
#include "services/network/shared_storage/shared_storage_test_utils.h"
#include "services/network/test/test_url_loader_network_observer.h"
#include "url/origin.h"

namespace network {

class SharedStorageTestURLLoaderNetworkObserver
    : public TestURLLoaderNetworkObserver {
 public:
  struct HeaderResult {
    HeaderResult(const url::Origin& request_origin,
                 std::vector<SharedStorageMethodWrapper> methods,
                 const std::optional<std::string>& with_lock);

    HeaderResult(const HeaderResult& other) = delete;
    HeaderResult& operator=(const HeaderResult& other) = delete;

    HeaderResult(HeaderResult&& other);
    HeaderResult& operator=(HeaderResult&& other);

    ~HeaderResult();

    url::Origin request_origin;
    std::vector<SharedStorageMethodWrapper> methods;
    std::optional<std::string> with_lock;
  };

  SharedStorageTestURLLoaderNetworkObserver();
  ~SharedStorageTestURLLoaderNetworkObserver() override;

  const std::vector<HeaderResult>& headers_received() const {
    return headers_received_;
  }

  // TestURLLoaderNetworkObserver:
  void OnSharedStorageHeaderReceived(
      const url::Origin& request_origin,
      std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
          methods_with_options,
      const std::optional<std::string>& with_lock,
      OnSharedStorageHeaderReceivedCallback callback) override;

  void WaitForHeadersReceived(size_t expected_total);

 private:
  std::unique_ptr<base::RunLoop> loop_;
  size_t expected_total_ = 0;
  std::vector<HeaderResult> headers_received_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_TEST_URL_LOADER_NETWORK_OBSERVER_H_
