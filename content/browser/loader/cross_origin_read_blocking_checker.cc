// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_origin_read_blocking_checker.h"

#include <string_view>

#include "base/functional/callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "url/origin.h"

namespace content {

// The CrossOriginReadBlockingChecker lives on the UI thread, but blobs must be
// read on IO. This class handles all blob access for
// CrossOriginReadBlockingChecker.
class CrossOriginReadBlockingChecker::BlobIOState {
 public:
  BlobIOState(base::WeakPtr<CrossOriginReadBlockingChecker> checker,
              std::unique_ptr<storage::BlobDataHandle> blob_data_handle)
      : checker_(std::move(checker)),
        blob_data_handle_(std::move(blob_data_handle)) {}

  ~BlobIOState() { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

  void StartSniffing() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    blob_reader_ = blob_data_handle_->CreateReader();
    const storage::BlobReader::Status size_status = blob_reader_->CalculateSize(
        base::BindOnce(&BlobIOState::DidCalculateSize, base::Unretained(this)));
    switch (size_status) {
      case storage::BlobReader::Status::NET_ERROR:
        OnNetError();
        return;
      case storage::BlobReader::Status::IO_PENDING:
        return;
      case storage::BlobReader::Status::DONE:
        DidCalculateSize(net::OK);
        return;
    }
  }

 private:
  void DidCalculateSize(int result) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    size_t buf_size = net::kMaxBytesToSniff;
    if (buf_size > blob_reader_->total_size()) {
      buf_size = blob_reader_->total_size();
    }
    buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(buf_size);
    int bytes_read;
    const storage::BlobReader::Status status = blob_reader_->Read(
        buffer_.get(), buf_size, &bytes_read,
        base::BindOnce(&BlobIOState::OnReadComplete, base::Unretained(this)));
    switch (status) {
      case storage::BlobReader::Status::NET_ERROR:
        OnNetError();
        return;
      case storage::BlobReader::Status::IO_PENDING:
        return;
      case storage::BlobReader::Status::DONE:
        OnReadComplete(bytes_read);
        return;
    }
  }

  void OnNetError() {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CrossOriginReadBlockingChecker::OnNetError,
                                  checker_, blob_reader_->net_error()));
  }

  void OnReadComplete(int bytes_read) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CrossOriginReadBlockingChecker::OnReadComplete,
                       checker_, bytes_read, buffer_,
                       blob_reader_->net_error()));
  }

  // |checker_| should only be accessed on the thread the navigation loader is
  // running on.
  base::WeakPtr<CrossOriginReadBlockingChecker> checker_;

  scoped_refptr<net::IOBufferWithSize> buffer_;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;
  std::unique_ptr<storage::BlobReader> blob_reader_;
};

CrossOriginReadBlockingChecker::CrossOriginReadBlockingChecker(
    const network::ResourceRequest& request,
    const network::mojom::URLResponseHead& response,
    const storage::BlobDataHandle& blob_data_handle,
    network::orb::PerFactoryState* orb_state,
    base::OnceCallback<void(Result)> callback)
    : callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());

  orb_analyzer_ = network::orb::ResponseAnalyzer::Create(orb_state);
  auto decision =
      orb_analyzer_->Init(request.url, request.request_initiator, request.mode,
                          request.destination, response);
  switch (decision) {
    case network::orb::ResponseAnalyzer::Decision::kBlock:
      OnBlocked();
      return;

    case network::orb::ResponseAnalyzer::Decision::kAllow:
      OnAllowed();
      return;

    case network::orb::ResponseAnalyzer::Decision::kSniffMore:
      blob_io_state_ = std::make_unique<BlobIOState>(
          weak_factory_.GetWeakPtr(),
          std::make_unique<storage::BlobDataHandle>(blob_data_handle));
      // base::Unretained is safe because |blob_io_state_| will be deleted on
      // the IO thread.
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&BlobIOState::StartSniffing,
                                    base::Unretained(blob_io_state_.get())));
      return;
  }
  NOTREACHED_IN_MIGRATION();  // Unrecognized `decision` value?
}

CrossOriginReadBlockingChecker::~CrossOriginReadBlockingChecker() {
  GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, std::move(blob_io_state_));
}

int CrossOriginReadBlockingChecker::GetNetError() {
  return net_error_;
}

void CrossOriginReadBlockingChecker::OnAllowed() {
  std::move(callback_).Run(Result::kAllowed);
}

void CrossOriginReadBlockingChecker::OnBlocked() {
  std::move(callback_).Run(orb_analyzer_->ShouldReportBlockedResponse()
                               ? Result::kBlocked_ShouldReport
                               : Result::kBlocked_ShouldNotReport);
}

void CrossOriginReadBlockingChecker::OnNetError(int net_error) {
  net_error_ = net_error;
  std::move(callback_).Run(Result::kNetError);
}

void CrossOriginReadBlockingChecker::OnReadComplete(
    int bytes_read,
    scoped_refptr<net::IOBufferWithSize> buffer,
    int net_error) {
  if (bytes_read != buffer->size()) {
    OnNetError(net_error);
    return;
  }

  std::string_view data(buffer->data(), bytes_read);
  network::orb::ResponseAnalyzer::Decision orb_decision =
      orb_analyzer_->Sniff(data);

  // At OnReadComplete we are out of data, so fall back to
  // HandleEndOfSniffableResponseBody if no allow/block `orb_decision` has been
  // reached yet.
  if (orb_decision == network::orb::ResponseAnalyzer::Decision::kSniffMore) {
    orb_decision = orb_analyzer_->HandleEndOfSniffableResponseBody();
    DCHECK_NE(network::orb::ResponseAnalyzer::Decision::kSniffMore,
              orb_decision);
  }

  switch (orb_decision) {
    case network::orb::ResponseAnalyzer::Decision::kBlock:
      OnBlocked();
      return;

    case network::orb::ResponseAnalyzer::Decision::kAllow:
      OnAllowed();
      return;

    case network::orb::ResponseAnalyzer::Decision::kSniffMore:
      // This should be impossible after going through
      // HandleEndOfSniffableResponseBody above.
      NOTREACHED_IN_MIGRATION();
      break;
  }
  // Fall back to blocking after encountering an unexpected or unrecognized
  // `orb_decision` in the `switch` statement above.
  NOTREACHED_IN_MIGRATION();
  OnBlocked();
}

}  // namespace content
