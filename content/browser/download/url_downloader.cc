// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/url_downloader.h"

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_request_handle_interface.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/url_download_request_handle.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/byte_stream_input_stream.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"

namespace content {

// static
std::unique_ptr<UrlDownloader> UrlDownloader::BeginDownload(
    base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
    std::unique_ptr<net::URLRequest> request,
    download::DownloadUrlParameters* params,
    bool is_parallel_request) {
  Referrer referrer(params->referrer(),
                    Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
                        params->referrer_policy()));
  Referrer sanitized_referrer =
      Referrer::SanitizeForRequest(request->url(), referrer);
  Referrer::SetReferrerForRequest(request.get(), sanitized_referrer);

  if (request->url().SchemeIs(url::kBlobScheme))
    return nullptr;

  // From this point forward, the |UrlDownloader| is responsible for
  // |started_callback|.
  std::unique_ptr<UrlDownloader> downloader(
      new UrlDownloader(std::move(request), delegate, is_parallel_request,
                        params->request_origin(), params->download_source()));
  downloader->Start();

  return downloader;
}

UrlDownloader::UrlDownloader(
    std::unique_ptr<net::URLRequest> request,
    base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
    bool is_parallel_request,
    const std::string& request_origin,
    download::DownloadSource download_source)
    : request_(std::move(request)),
      delegate_(delegate),
      core_(request_.get(),
            this,
            is_parallel_request,
            request_origin,
            download_source),
      weak_ptr_factory_(this) {}

UrlDownloader::~UrlDownloader() {
}

void UrlDownloader::Start() {
  DCHECK(!request_->is_pending());

  request_->set_delegate(this);
  request_->Start();
}

void UrlDownloader::OnReceivedRedirect(net::URLRequest* request,
                                       const net::RedirectInfo& redirect_info,
                                       bool* defer_redirect) {
  DVLOG(1) << "OnReceivedRedirect: " << request_->url().spec();
  // We are going to block redirects even if DownloadRequestCore allows it.  No
  // redirects are expected for download requests that are made without a
  // renderer, which are currently exclusively resumption requests. Since there
  // is no security policy being applied here, it's safer to block redirects and
  // revisit if some previously unknown legitimate use case arises for redirects
  // while resuming.
  core_.OnWillAbort(download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE);
  request_->CancelWithError(net::ERR_UNSAFE_REDIRECT);
}

void UrlDownloader::OnResponseStarted(net::URLRequest* request, int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  DVLOG(1) << "OnResponseStarted: " << request_->url().spec();

  if (net_error != net::OK) {
    ResponseCompleted(net_error);
    return;
  }

  if (core_.OnResponseStarted(std::string()))
    StartReading(false);  // Read the first chunk.
  else
    ResponseCompleted(net::OK);
}

void UrlDownloader::StartReading(bool is_continuation) {
  int bytes_read;

  // Make sure we track the buffer in at least one place.  This ensures it gets
  // deleted even in the case the request has already finished its job and
  // doesn't use the buffer.
  scoped_refptr<net::IOBuffer> buf;
  int buf_size;
  if (!core_.OnWillRead(&buf, &buf_size)) {
    int result = request_->CancelWithError(net::ERR_ABORTED);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&UrlDownloader::ResponseCompleted,
                                  weak_ptr_factory_.GetWeakPtr(), result));
    return;
  }

  DCHECK(buf.get());
  DCHECK(buf_size > 0);

  bytes_read = request_->Read(buf.get(), buf_size);

  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (bytes_read == net::ERR_IO_PENDING)
    return;

  if (!is_continuation || bytes_read <= 0) {
    OnReadCompleted(request_.get(), bytes_read);
  } else {
    // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
    // thread in case the URLRequest can provide data synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&UrlDownloader::OnReadCompleted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  request_.get(), bytes_read));
  }
}

void UrlDownloader::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  DVLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
           << " bytes_read = " << bytes_read;

  // bytes_read can be an error.
  if (bytes_read < 0) {
    ResponseCompleted(bytes_read);
    return;
  }

  DCHECK(bytes_read >= 0);

  bool defer = false;
  if (!core_.OnReadCompleted(bytes_read, &defer)) {
    request_->CancelWithError(net::ERR_ABORTED);
    return;
  } else if (defer) {
    return;
  }

  if (bytes_read > 0) {
    StartReading(true);  // Read the next chunk.
  } else {
    // URLRequest reported an EOF. Call ResponseCompleted.
    DCHECK_EQ(0, bytes_read);
    ResponseCompleted(net::OK);
  }
}

void UrlDownloader::ResponseCompleted(int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  DVLOG(1) << "ResponseCompleted: " << request_->url().spec();

  core_.OnResponseCompleted(net::URLRequestStatus::FromError(net_error));
  Destroy();
}

void UrlDownloader::OnStart(
    std::unique_ptr<download::DownloadCreateInfo> create_info,
    std::unique_ptr<ByteStreamReader> stream_reader,
    const download::DownloadUrlParameters::OnStartedCallback& callback) {
  create_info->request_handle.reset(new download::UrlDownloadRequestHandle(
      weak_ptr_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get()));

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &download::UrlDownloadHandler::Delegate::OnUrlDownloadStarted,
          delegate_, std::move(create_info),
          std::make_unique<ByteStreamInputStream>(std::move(stream_reader)),
          nullptr, callback));
}

void UrlDownloader::OnReadyToRead() {
    StartReading(false);  // Read the next chunk (OK to complete synchronously).
}

void UrlDownloader::PauseRequest() {
  core_.PauseRequest();
}

void UrlDownloader::ResumeRequest() {
  core_.ResumeRequest();
}

void UrlDownloader::CancelRequest() {
  Destroy();
}

void UrlDownloader::Destroy() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &download::UrlDownloadHandler::Delegate::OnUrlDownloadStopped,
          delegate_, this));
}

}  // namespace content
