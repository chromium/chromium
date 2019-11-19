// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_client.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TestURLLoaderClient::TestURLLoaderClient() = default;
TestURLLoaderClient::~TestURLLoaderClient() = default;

void TestURLLoaderClient::OnReceiveResponse(
    mojom::URLResponseHeadPtr response_head) {
  EXPECT_FALSE(has_received_response_);
  EXPECT_FALSE(has_received_cached_metadata_);
  EXPECT_FALSE(has_received_completion_);
  has_received_response_ = true;
  response_head_ = std::move(response_head);
  if (quit_closure_for_on_receive_response_)
    std::move(quit_closure_for_on_receive_response_).Run();
}

void TestURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr response_head) {
  EXPECT_FALSE(has_received_cached_metadata_);
  EXPECT_FALSE(response_body_.is_valid());
  EXPECT_FALSE(has_received_response_);
  // Use ClearHasReceivedRedirect to accept more redirects.
  EXPECT_FALSE(has_received_redirect_);
  EXPECT_FALSE(has_received_completion_);
  has_received_redirect_ = true;
  redirect_info_ = redirect_info;
  response_head_ = std::move(response_head);
  if (quit_closure_for_on_receive_redirect_)
    std::move(quit_closure_for_on_receive_redirect_).Run();
}

void TestURLLoaderClient::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  EXPECT_FALSE(has_received_cached_metadata_);
  EXPECT_TRUE(has_received_response_);
  EXPECT_FALSE(has_received_completion_);
  has_received_cached_metadata_ = true;
  cached_metadata_ =
      std::string(reinterpret_cast<const char*>(data.data()), data.size());
  if (quit_closure_for_on_receive_cached_metadata_)
    std::move(quit_closure_for_on_receive_cached_metadata_).Run();
}

void TestURLLoaderClient::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  EXPECT_FALSE(has_received_completion_);
  EXPECT_GT(transfer_size_diff, 0);
  body_transfer_size_ += transfer_size_diff;
  if (quit_closure_for_on_transfer_size_updated_)
    std::move(quit_closure_for_on_transfer_size_updated_).Run();
}

void TestURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  EXPECT_TRUE(ack_callback);
  EXPECT_FALSE(has_received_response_);
  EXPECT_FALSE(has_received_completion_);
  EXPECT_LT(0, current_position);
  EXPECT_LE(current_position, total_size);

  has_received_upload_progress_ = true;
  current_upload_position_ = current_position;
  total_upload_size_ = total_size;
  std::move(ack_callback).Run();
}

void TestURLLoaderClient::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  EXPECT_TRUE(has_received_response_);
  EXPECT_FALSE(has_received_completion_);
  response_body_ = std::move(body);
  if (quit_closure_for_on_start_loading_response_body_)
    std::move(quit_closure_for_on_start_loading_response_body_).Run();
}

void TestURLLoaderClient::OnComplete(const URLLoaderCompletionStatus& status) {
  EXPECT_FALSE(has_received_completion_);
  has_received_completion_ = true;
  completion_status_ = status;
  if (quit_closure_for_on_complete_)
    std::move(quit_closure_for_on_complete_).Run();
}

void TestURLLoaderClient::ClearHasReceivedRedirect() {
  has_received_redirect_ = false;
}

mojo::PendingRemote<mojom::URLLoaderClient>
TestURLLoaderClient::CreateRemote() {
  mojo::PendingRemote<mojom::URLLoaderClient> client_remote;
  receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(base::BindOnce(
      &TestURLLoaderClient::OnMojoDisconnect, base::Unretained(this)));
  return client_remote;
}

void TestURLLoaderClient::Unbind() {
  receiver_.reset();
  response_body_.reset();
}

void TestURLLoaderClient::RunUntilResponseReceived() {
  if (has_received_response_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_receive_response_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestURLLoaderClient::RunUntilRedirectReceived() {
  if (has_received_redirect_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_receive_redirect_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestURLLoaderClient::RunUntilCachedMetadataReceived() {
  if (has_received_cached_metadata_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_receive_cached_metadata_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestURLLoaderClient::RunUntilResponseBodyArrived() {
  if (response_body_.is_valid())
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_start_loading_response_body_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestURLLoaderClient::RunUntilComplete() {
  if (has_received_completion_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_complete_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestURLLoaderClient::RunUntilDisconnect() {
  if (has_received_disconnect_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_disconnect_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestURLLoaderClient::RunUntilTransferSizeUpdated() {
  base::RunLoop run_loop;
  quit_closure_for_on_transfer_size_updated_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestURLLoaderClient::OnMojoDisconnect() {
  if (has_received_disconnect_)
    return;
  has_received_disconnect_ = true;
  if (quit_closure_for_disconnect_)
    std::move(quit_closure_for_disconnect_).Run();
}

}  // namespace network
