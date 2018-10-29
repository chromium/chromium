// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/loader/resource_handler.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace download {
struct DownloadCreateInfo;
}  // namespace download

namespace net {
class HttpResponseHeaders;
class URLRequest;
class URLRequestContextGetter;
class URLRequestStatus;
}  // namespace net

namespace content {
class ByteStreamReader;
class ByteStreamWriter;

// This class encapsulates the core logic for reading data from a URLRequest and
// writing it into a ByteStream. It's common to both DownloadResourceHandler and
// UrlDownloader.
//
// Created, lives on and dies on the IO thread.
class CONTENT_EXPORT DownloadRequestCore
    : public base::SupportsWeakPtr<DownloadRequestCore> {
 public:
  class Delegate {
   public:
    virtual void OnReadyToRead() = 0;
    virtual void OnStart(
        std::unique_ptr<download::DownloadCreateInfo> download_create_info,
        std::unique_ptr<ByteStreamReader> stream_reader,
        const download::DownloadUrlParameters::OnStartedCallback& callback) = 0;
  };

  // All parameters are required. |request| and |delegate| must outlive
  // DownloadRequestCore. The request is not the main request if
  // |is_parallel_request| is true.
  DownloadRequestCore(net::URLRequest* request,
                      Delegate* delegate,
                      bool is_parallel_request,
                      const std::string& request_origin,
                      download::DownloadSource download_source);
  ~DownloadRequestCore();

  // Should be called when the URLRequest::Delegate receives OnResponseStarted.
  // Invokes Delegate::OnStart() with download start parameters. The
  // |override_mime_type| is used as the MIME type for the download when
  // constructing a download::DownloadCreateInfo object.
  bool OnResponseStarted(const std::string& override_mime_type);

  // Should be called to handle a redirect. The caller should only allow the
  // redirect to be followed if the return value is true.
  bool OnRequestRedirected();

  // Starts a read cycle. Creates an IOBuffer which can be passed into
  // URLRequest::Read(). Call OnReadCompleted() when the Read operation
  // completes.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size);

  // Used to notify DownloadRequestCore that the caller is about to abort the
  // outer request. |reason| will be used as the final interrupt reason when
  // OnResponseCompleted() is called.
  void OnWillAbort(download::DownloadInterruptReason reason);

  // Should be called when the Read() operation completes. |defer| will be set
  // to true if reading is to be suspended. In the latter case, once more data
  // can be read, invokes the |on_ready_to_read_callback|.
  bool OnReadCompleted(int bytes_read, bool* defer);

  // Called to signal that the response is complete.
  //
  // It is expected that once this method is invoked, the DownloadRequestCore
  // object will be destroyed in short order without invoking any other methods
  // other than the destructor.
  void OnResponseCompleted(const net::URLRequestStatus& status);

  // Called if the request should suspend reading. A subsequent
  // OnReadCompleted() will result in |defer| being set to true.
  //
  // Each PauseRequest() must be balanced with a call to ResumeRequest().
  void PauseRequest();

  // Balances a call to PauseRequest(). If no more pauses are outstanding and
  // the reader end of the ByteStream is ready to receive more data,
  // DownloadRequestCore will invoke the |on_ready_to_read_callback| to signal
  // to the caller that the read cycles should commence.
  void ResumeRequest();

  std::string DebugString() const;

  static std::unique_ptr<net::URLRequest> CreateRequestOnIOThread(
      bool is_new_download,
      download::DownloadUrlParameters* params,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  // Size of the buffer used between the DownloadRequestCore and the
  // downstream receiver of its output.
  static const int kDownloadByteStreamSize;

 protected:
  net::URLRequest* request() const { return request_; }

 private:
  std::unique_ptr<download::DownloadCreateInfo> CreateDownloadCreateInfo(
      download::DownloadInterruptReason result);

  Delegate* delegate_;
  net::URLRequest* request_;

  // "Passthrough" fields. These are only kept here so that they can be used to
  // populate the download::DownloadCreateInfo when the time comes.
  std::unique_ptr<download::DownloadSaveInfo> save_info_;
  bool is_new_download_;
  std::string guid_;
  bool fetch_error_body_;
  download::DownloadUrlParameters::RequestHeadersType request_headers_;
  bool transient_;
  download::DownloadUrlParameters::OnStartedCallback on_started_callback_;

  // Data flow
  scoped_refptr<net::IOBuffer> read_buffer_;    // From URLRequest.
  std::unique_ptr<ByteStreamWriter> stream_writer_;  // To rest of system.

  // Used to keep the system from sleeping while a download is ongoing. If the
  // system enters power saving mode while a URLRequest is alive, it can cause
  // URLRequest to fail and the associated download will be interrupted.
  device::mojom::WakeLockPtr wake_lock_;

  int64_t bytes_read_;

  int pause_count_;
  bool was_deferred_;
  bool is_partial_request_;
  bool started_;

  // When DownloadRequestCore initiates an abort (by blocking a redirect, for
  // example) it expects to eventually receive a OnResponseCompleted() with a
  // status indicating that the request was aborted. When this happens, the
  // interrupt reason in |abort_reason_| will be used instead of USER_CANCELED
  // which is vague.
  download::DownloadInterruptReason abort_reason_;

  // For downloads originating from custom tabs, this records the origin
  // of the custom tab.
  std::string request_origin_;

  // Source of the download, used in metrics.
  download::DownloadSource download_source_;

  // Each successful OnWillRead will yield a buffer of this size.
  static const int kReadBufSize = 32768;   // bytes

  DISALLOW_COPY_AND_ASSIGN(DownloadRequestCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_CORE_H_
