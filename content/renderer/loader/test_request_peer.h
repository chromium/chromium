// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_TEST_REQUEST_PEER_H_
#define CONTENT_RENDERER_LOADER_TEST_REQUEST_PEER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include "base/time/time.h"
#include "content/public/renderer/request_peer.h"
#include "net/base/load_timing_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace content {

class ResourceDispatcher;

// Listens for request response data and stores it so that it can be compared
// to the reference data.
class TestRequestPeer : public RequestPeer {
 public:
  struct Context;
  TestRequestPeer(ResourceDispatcher* dispatcher, Context* context);
  ~TestRequestPeer() override;

  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr head) override;
  void OnReceivedResponse(network::mojom::URLResponseHeadPtr head) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnReceivedCachedMetadata(mojo_base::BigBuffer data) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;
  scoped_refptr<base::TaskRunner> GetTaskRunner() override;

  struct Context final {
    Context();
    ~Context();

    // True if should follow redirects, false if should cancel them.
    bool follow_redirects = true;
    // True if the request should be deferred on redirects.
    bool defer_on_redirect = false;

    // Number of total redirects seen.
    int seen_redirects = 0;

    bool cancel_on_receive_response = false;
    bool cancel_on_receive_data = false;
    bool received_response = false;

    mojo_base::BigBuffer cached_metadata;
    // Data received. If downloading to file, remains empty.
    std::string data;

    // Mojo's data pipe passed on OnStartLoadingResponseBody.
    mojo::ScopedDataPipeConsumerHandle body_handle;

    // Total encoded data length, regardless of whether downloading to a file or
    // not.
    int total_encoded_data_length = 0;
    bool defer_on_transfer_size_updated = false;

    bool complete = false;
    bool cancelled = false;
    int request_id = -1;

    net::LoadTimingInfo last_load_timing;
    network::URLLoaderCompletionStatus completion_status;
  };

 private:
  ResourceDispatcher* dispatcher_;
  Context* context_;

  DISALLOW_COPY_AND_ASSIGN(TestRequestPeer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_TEST_REQUEST_PEER_H_
