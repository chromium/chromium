// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/mime_sniffing_url_loader.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/mime_sniffer.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/mime_sniffing_throttle.h"

namespace blink {

// static
const char MimeSniffingURLLoader::kDefaultMimeType[] = "text/plain";

// static
std::tuple<mojo::PendingRemote<network::mojom::URLLoader>,
           mojo::PendingReceiver<network::mojom::URLLoaderClient>,
           MimeSniffingURLLoader*>
MimeSniffingURLLoader::CreateLoader(
    base::WeakPtr<MimeSniffingThrottle> throttle,
    const GURL& response_url,
    network::mojom::URLResponseHeadPtr response_head,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  mojo::PendingRemote<network::mojom::URLLoader> url_loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client;
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      url_loader_client_receiver =
          url_loader_client.InitWithNewPipeAndPassReceiver();

  auto loader = base::WrapUnique(new MimeSniffingURLLoader(
      std::move(throttle), response_url, std::move(response_head),
      std::move(url_loader_client), std::move(task_runner)));
  MimeSniffingURLLoader* loader_rawptr = loader.get();
  mojo::MakeSelfOwnedReceiver(std::move(loader),
                              url_loader.InitWithNewPipeAndPassReceiver());
  return std::make_tuple(std::move(url_loader),
                         std::move(url_loader_client_receiver), loader_rawptr);
}

MimeSniffingURLLoader::MimeSniffingURLLoader(
    base::WeakPtr<MimeSniffingThrottle> throttle,
    const GURL& response_url,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::PendingRemote<network::mojom::URLLoaderClient>
        destination_url_loader_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : throttle_(throttle),
      destination_url_loader_client_(std::move(destination_url_loader_client)),
      response_url_(response_url),
      response_head_(std::move(response_head)),
      task_runner_(task_runner),
      body_consumer_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             task_runner),
      body_producer_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             std::move(task_runner)) {}

MimeSniffingURLLoader::~MimeSniffingURLLoader() = default;

void MimeSniffingURLLoader::Start(
    mojo::PendingRemote<network::mojom::URLLoader> source_url_loader_remote,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        source_url_client_receiver,
    mojo::ScopedDataPipeConsumerHandle body) {
  source_url_loader_.Bind(std::move(source_url_loader_remote));
  source_url_client_receiver_.Bind(std::move(source_url_client_receiver),
                                   task_runner_);
  if (!body)
    return;

  state_ = State::kSniffing;
  body_consumer_handle_ = std::move(body);
  body_consumer_watcher_.Watch(
      body_consumer_handle_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&MimeSniffingURLLoader::OnBodyReadable,
                          base::Unretained(this)));
  body_consumer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // OnReceiveEarlyHints() shouldn't be called. See the comment in
  // OnReceiveResponse().
  NOTREACHED_IN_MIGRATION();
}

void MimeSniffingURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  // OnReceiveResponse() shouldn't be called because MimeSniffingURLLoader is
  // created by MimeSniffingThrottle::WillProcessResponse(), which is equivalent
  // to OnReceiveResponse().
  NOTREACHED_IN_MIGRATION();
}

void MimeSniffingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  // OnReceiveRedirect() shouldn't be called because MimeSniffingURLLoader is
  // created by MimeSniffingThrottle::WillProcessResponse(), which is equivalent
  // to OnReceiveResponse().
  NOTREACHED_IN_MIGRATION();
}

void MimeSniffingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  destination_url_loader_client_->OnUploadProgress(current_position, total_size,
                                                   std::move(ack_callback));
}

void MimeSniffingURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kMimeSniffingURLLoader);
  destination_url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void MimeSniffingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(!complete_status_.has_value());
  switch (state_) {
    case State::kWaitForBody:
      // An error occured before receiving any data.
      DCHECK_NE(net::OK, status.error_code);
      state_ = State::kCompleted;
      response_head_->mime_type = kDefaultMimeType;
      if (!throttle_) {
        Abort();
        return;
      }
      throttle_->ResumeWithNewResponseHead(
          std::move(response_head_), mojo::ScopedDataPipeConsumerHandle());
      destination_url_loader_client_->OnComplete(status);
      return;
    case State::kSniffing:
    case State::kSending:
      // Defer calling OnComplete() until mime sniffing has finished and all
      // data is sent.
      complete_status_ = status;
      return;
    case State::kCompleted:
      destination_url_loader_client_->OnComplete(status);
      return;
    case State::kAborted:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void MimeSniffingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  // MimeSniffingURLLoader starts handling the request after
  // OnReceivedResponse(). A redirect response is not expected.
  NOTREACHED_IN_MIGRATION();
}

void MimeSniffingURLLoader::SetPriority(net::RequestPriority priority,
                                        int32_t intra_priority_value) {
  if (state_ == State::kAborted)
    return;
  source_url_loader_->SetPriority(priority, intra_priority_value);
}

void MimeSniffingURLLoader::PauseReadingBodyFromNet() {
  if (state_ == State::kAborted)
    return;
  source_url_loader_->PauseReadingBodyFromNet();
}

void MimeSniffingURLLoader::ResumeReadingBodyFromNet() {
  if (state_ == State::kAborted)
    return;
  source_url_loader_->ResumeReadingBodyFromNet();
}

void MimeSniffingURLLoader::OnBodyReadable(MojoResult) {
  if (state_ == State::kSending) {
    // The pipe becoming readable when kSending means all buffered body has
    // already been sent.
    ForwardBodyToClient();
    return;
  }
  DCHECK_EQ(State::kSniffing, state_);

  size_t start_size = buffered_body_.size();
  size_t read_bytes = net::kMaxBytesToSniff;
  buffered_body_.resize(start_size + read_bytes);
  MojoResult result = body_consumer_handle_->ReadData(
      MOJO_READ_DATA_FLAG_NONE,
      base::as_writable_byte_span(buffered_body_)
          .subspan(start_size, read_bytes),
      read_bytes);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Finished the body before mime type is completely decided.
      buffered_body_.resize(start_size);
      CompleteSniffing();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      buffered_body_.resize(start_size);
      body_consumer_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  DCHECK_EQ(MOJO_RESULT_OK, result);
  buffered_body_.resize(start_size + read_bytes);
  std::string new_type;
  bool made_final_decision = net::SniffMimeType(
      std::string_view(buffered_body_.data(), buffered_body_.size()),
      response_url_, response_head_->mime_type,
      net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
  response_head_->mime_type = new_type;
  response_head_->did_mime_sniff = true;
  if (made_final_decision) {
    CompleteSniffing();
    return;
  }
  body_consumer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::OnBodyWritable(MojoResult) {
  DCHECK_EQ(State::kSending, state_);
  if (bytes_remaining_in_buffer_ > 0) {
    SendReceivedBodyToClient();
  } else {
    ForwardBodyToClient();
  }
}

void MimeSniffingURLLoader::CompleteSniffing() {
  DCHECK_EQ(State::kSniffing, state_);
  if (buffered_body_.empty()) {
    // The URLLoader ended before sending any data. There is not enough
    // information to determine the MIME type.
    response_head_->mime_type = kDefaultMimeType;
  }

  state_ = State::kSending;
  bytes_remaining_in_buffer_ = buffered_body_.size();
  if (!throttle_) {
    Abort();
    return;
  }
  mojo::ScopedDataPipeConsumerHandle body_to_send;
  MojoResult result =
      mojo::CreateDataPipe(nullptr, body_producer_handle_, body_to_send);
  if (result != MOJO_RESULT_OK) {
    Abort();
    return;
  }
  throttle_->ResumeWithNewResponseHead(std::move(response_head_),
                                       std::move(body_to_send));
  // Set up the watcher for the producer handle.
  body_producer_watcher_.Watch(
      body_producer_handle_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&MimeSniffingURLLoader::OnBodyWritable,
                          base::Unretained(this)));

  if (bytes_remaining_in_buffer_) {
    SendReceivedBodyToClient();
    return;
  }

  CompleteSending();
}

void MimeSniffingURLLoader::CompleteSending() {
  DCHECK_EQ(State::kSending, state_);
  state_ = State::kCompleted;
  // Call client's OnComplete() if |this|'s OnComplete() has already been
  // called.
  if (complete_status_.has_value())
    destination_url_loader_client_->OnComplete(complete_status_.value());

  body_consumer_watcher_.Cancel();
  body_producer_watcher_.Cancel();
  body_consumer_handle_.reset();
  body_producer_handle_.reset();
}

void MimeSniffingURLLoader::SendReceivedBodyToClient() {
  DCHECK_EQ(State::kSending, state_);
  // Send the buffered data first.
  DCHECK_GT(bytes_remaining_in_buffer_, 0u);
  base::span<const uint8_t> bytes =
      base::as_byte_span(buffered_body_).last(bytes_remaining_in_buffer_);
  size_t actually_sent_bytes = 0;
  MojoResult result = body_producer_handle_->WriteData(
      bytes, MOJO_WRITE_DATA_FLAG_NONE, actually_sent_bytes);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The pipe is closed unexpectedly. |this| should be deleted once
      // URLLoader on the destination is released.
      Abort();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      body_producer_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  bytes_remaining_in_buffer_ -= actually_sent_bytes;
  body_producer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::ForwardBodyToClient() {
  DCHECK_EQ(0u, bytes_remaining_in_buffer_);
  // Send the body from the consumer to the producer.
  base::span<const uint8_t> buffer;
  MojoResult result = body_consumer_handle_->BeginReadData(
      MOJO_BEGIN_READ_DATA_FLAG_NONE, buffer);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      body_consumer_watcher_.ArmOrNotify();
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // All data has been sent.
      CompleteSending();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  size_t actually_written_bytes = 0;
  result = body_producer_handle_->WriteData(buffer, MOJO_WRITE_DATA_FLAG_NONE,
                                            actually_written_bytes);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The pipe is closed unexpectedly. |this| should be deleted once
      // URLLoader on the destination is released.
      Abort();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      body_consumer_handle_->EndReadData(0);
      body_producer_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  body_consumer_handle_->EndReadData(actually_written_bytes);
  body_consumer_watcher_.ArmOrNotify();
}

void MimeSniffingURLLoader::Abort() {
  state_ = State::kAborted;
  body_consumer_watcher_.Cancel();
  body_producer_watcher_.Cancel();
  source_url_loader_.reset();
  source_url_client_receiver_.reset();
  destination_url_loader_client_.reset();
  // |this| should be removed since the owner will destroy |this| or the owner
  // has already been destroyed by some reason.
}

}  // namespace blink
