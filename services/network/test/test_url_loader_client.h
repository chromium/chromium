// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_URL_LOADER_CLIENT_H_
#define SERVICES_NETWORK_TEST_TEST_URL_LOADER_CLIENT_H_

#include <stdint.h>
#include <vector>

#include "base/functional/callback.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

// TestURLLoaderClient records URLLoaderClient function calls.
//
// Example usage:
//   TestURLLoaderClient client;
//   factory_->CreateLoaderAndStart(..., client.CreateRemote(), ...);
//   client.RunUntilComplete();
//   EXPECT_EQ(net::OK, client.completion_status().error_code);
//   ...
class TestURLLoaderClient final : public mojom::URLLoaderClient {
 public:
  TestURLLoaderClient();

  TestURLLoaderClient(const TestURLLoaderClient&) = delete;
  TestURLLoaderClient& operator=(const TestURLLoaderClient&) = delete;

  ~TestURLLoaderClient() override;

  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         mojom::URLResponseHeadPtr response_head) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

  bool has_received_early_hints() const { return has_received_early_hints_; }
  bool has_received_response() const { return has_received_response_; }
  bool has_received_redirect() const { return has_received_redirect_; }
  bool has_received_upload_progress() const {
    return has_received_upload_progress_;
  }
  bool has_received_completion() const { return has_received_completion_; }
  bool has_received_disconnect() const { return has_received_disconnect_; }
  const mojom::URLResponseHeadPtr& response_head() const {
    return response_head_;
  }
  const std::optional<net::SSLInfo>& ssl_info() const {
    DCHECK(response_head_);
    return response_head_->ssl_info;
  }
  const net::RedirectInfo& redirect_info() const { return redirect_info_; }
  const std::optional<std::string>& cached_metadata() const {
    return cached_metadata_;
  }
  mojo::DataPipeConsumerHandle response_body() { return response_body_.get(); }
  mojo::ScopedDataPipeConsumerHandle response_body_release() {
    return std::move(response_body_);
  }
  const URLLoaderCompletionStatus& completion_status() const {
    return completion_status_;
  }
  int64_t body_transfer_size() const { return body_transfer_size_; }
  int64_t current_upload_position() const { return current_upload_position_; }
  int64_t total_upload_size() const { return total_upload_size_; }
  const std::vector<network::mojom::EarlyHintsPtr>& early_hints() const {
    return early_hints_;
  }

  void reset_has_received_upload_progress() {
    has_received_upload_progress_ = false;
  }

  void ClearHasReceivedRedirect();
  // Creates an PendingRemote, binds it to |*this| and returns it.
  mojo::PendingRemote<mojom::URLLoaderClient> CreateRemote();

  void Unbind();

  void RunUntilResponseReceived();
  void RunUntilRedirectReceived();
  void RunUntilResponseBodyArrived();
  void RunUntilComplete();
  void RunUntilDisconnect();
  void RunUntilTransferSizeUpdated();

 private:
  void OnMojoDisconnect();

  mojo::Receiver<mojom::URLLoaderClient> receiver_{this};
  mojom::URLResponseHeadPtr response_head_;
  net::RedirectInfo redirect_info_;
  std::optional<std::string> cached_metadata_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  URLLoaderCompletionStatus completion_status_;
  bool has_received_early_hints_ = false;
  bool has_received_response_ = false;
  bool has_received_redirect_ = false;
  bool has_received_upload_progress_ = false;
  bool has_received_completion_ = false;
  bool has_received_disconnect_ = false;

  base::OnceClosure quit_closure_for_on_receive_response_;
  base::OnceClosure quit_closure_for_on_receive_redirect_;
  base::OnceClosure quit_closure_for_on_start_loading_response_body_;
  base::OnceClosure quit_closure_for_on_complete_;
  base::OnceClosure quit_closure_for_disconnect_;
  base::OnceClosure quit_closure_for_on_transfer_size_updated_;

  int64_t body_transfer_size_ = 0;
  int64_t current_upload_position_ = 0;
  int64_t total_upload_size_ = 0;

  std::vector<network::mojom::EarlyHintsPtr> early_hints_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_URL_LOADER_CLIENT_H_
