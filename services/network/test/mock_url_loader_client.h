// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_MOCK_URL_LOADER_CLIENT_H_
#define SERVICES_NETWORK_TEST_MOCK_URL_LOADER_CLIENT_H_

#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace network {

class MockURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  MockURLLoaderClient();
  MockURLLoaderClient(const MockURLLoaderClient&) = delete;
  MockURLLoaderClient& operator=(const MockURLLoaderClient&) = delete;
  ~MockURLLoaderClient() override;

  MOCK_METHOD(void,
              OnReceiveEarlyHints,
              (network::mojom::EarlyHintsPtr early_hints),
              (override));
  MOCK_METHOD(void,
              OnReceiveResponse,
              (network::mojom::URLResponseHeadPtr head,
               mojo::ScopedDataPipeConsumerHandle body,
               std::optional<mojo_base::BigBuffer> cached_metadata),
              (override));
  MOCK_METHOD(void,
              OnReceiveRedirect,
              (const net::RedirectInfo& redirect_info,
               network::mojom::URLResponseHeadPtr head),
              (override));
  MOCK_METHOD(void,
              OnUploadProgress,
              (int64_t current_position,
               int64_t total_size,
               OnUploadProgressCallback ack_callback),
              (override));
  MOCK_METHOD(void,
              OnTransferSizeUpdated,
              (int32_t transfer_size_diff),
              (override));
  MOCK_METHOD(void,
              OnComplete,
              (const network::URLLoaderCompletionStatus& status),
              (override));
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_MOCK_URL_LOADER_CLIENT_H_
