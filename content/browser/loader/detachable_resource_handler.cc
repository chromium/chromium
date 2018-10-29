// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/detachable_resource_handler.h"

#include <utility>

#include "base/logging.h"
#include "base/time/time.h"
#include "content/browser/loader/null_resource_controller.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace {
// This matches the maximum allocation size of AsyncResourceHandler.
const int kReadBufSize = 32 * 1024;
}

namespace content {

// ResourceController that, when invoked, runs the corresponding method on
// ResourceHandler.
class DetachableResourceHandler::Controller : public ResourceController {
 public:
  explicit Controller(DetachableResourceHandler* detachable_handler)
      : detachable_handler_(detachable_handler){};

  ~Controller() override {}

  // ResourceController implementation:
  void Resume() override {
    MarkAsUsed();
    detachable_handler_->ResumeInternal();
  }

  void ResumeForRedirect(const base::Optional<net::HttpRequestHeaders>&
                             modified_request_headers) override {
    MarkAsUsed();
    detachable_handler_->ResumeForRedirect(modified_request_headers);
  }

  void Cancel() override {
    MarkAsUsed();
    detachable_handler_->Cancel();
  }

  void CancelWithError(int error_code) override {
    MarkAsUsed();
    detachable_handler_->CancelWithError(error_code);
  }

 private:
  void MarkAsUsed() {
#if DCHECK_IS_ON()
    DCHECK(!used_);
    used_ = true;
#endif
  }

#if DCHECK_IS_ON()
  bool used_ = false;
#endif

  DetachableResourceHandler* detachable_handler_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

DetachableResourceHandler::DetachableResourceHandler(
    net::URLRequest* request,
    base::TimeDelta cancel_delay,
    std::unique_ptr<ResourceHandler> next_handler)
    : ResourceHandler(request),
      next_handler_(std::move(next_handler)),
      cancel_delay_(cancel_delay),
      parent_read_buffer_(nullptr),
      parent_read_buffer_size_(nullptr),
      is_finished_(false) {
  GetRequestInfo()->set_detachable_handler(this);
}

DetachableResourceHandler::~DetachableResourceHandler() {
  // Cleanup back-pointer stored on the request info.
  GetRequestInfo()->set_detachable_handler(nullptr);
}

void DetachableResourceHandler::SetDelegate(Delegate* delegate) {
  ResourceHandler::SetDelegate(delegate);
  if (next_handler_)
    next_handler_->SetDelegate(delegate);
}

void DetachableResourceHandler::Detach() {
  if (is_detached())
    return;

  if (!is_finished_) {
    // Simulate a cancel on the next handler before destroying it.
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    bool was_resumed;
    // TODO(mmenke): Get rid of NullResourceController and do something more
    // reasonable.
    next_handler_->OnResponseCompleted(
        status, std::make_unique<NullResourceController>(&was_resumed));
    DCHECK(was_resumed);
    // If |next_handler_| were to defer its shutdown in OnResponseCompleted,
    // this would destroy it anyway. Fortunately, AsyncResourceHandler never
    // does this anyway, so DCHECK it. MimeTypeResourceHandler and RVH shutdown
    // already ignore deferred ResourceHandler shutdown, but
    // DetachableResourceHandler and the detach-on-renderer-cancel logic
    // introduces a case where this occurs when the renderer cancels a resource.
  }
  // A OnWillRead / OnReadCompleted pair may still be in progress, but
  // OnWillRead passes back a scoped_refptr, so downstream handler's buffer will
  // survive long enough to complete that read. From there, future reads will
  // drain into |read_buffer_|. (If |next_handler_| is an AsyncResourceHandler,
  // the net::IOBuffer takes a reference to the ResourceBuffer which owns the
  // shared memory.)
  next_handler_.reset();

  // Time the request out if it takes too long.
  detached_timer_.reset(new base::OneShotTimer());
  detached_timer_->Start(FROM_HERE, cancel_delay_, this,
                         &DetachableResourceHandler::OnTimedOut);

  // Resume if necessary. The request may have been deferred, say, waiting on a
  // full buffer in AsyncResourceHandler. Now that it has been detached, resume
  // and drain it.
  if (has_controller()) {
    // The nested ResourceHandler may have logged that it's blocking the
    // request.  Log it as no longer doing so, to avoid a DCHECK on resume.
    request()->LogUnblocked();

    // If in the middle of an OnWillRead call, need to allocate the read buffer
    // before resuming.
    if (parent_read_buffer_) {
      DCHECK(parent_read_buffer_size_);

      scoped_refptr<net::IOBuffer>* parent_read_buffer = parent_read_buffer_;
      int* parent_read_buffer_size = parent_read_buffer_size_;
      parent_read_buffer_ = nullptr;
      parent_read_buffer_size_ = nullptr;

      // Will allocate the buffer and resume the request.
      OnWillRead(parent_read_buffer, parent_read_buffer_size,
                 ReleaseController());
      return;
    }

    Resume();
  }
}

void DetachableResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  if (!next_handler_) {
    controller->Resume();
    return;
  }

  HoldController(std::move(controller));
  next_handler_->OnRequestRedirected(redirect_info, response,
                                     std::make_unique<Controller>(this));
}

void DetachableResourceHandler::OnResponseStarted(
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  if (!next_handler_) {
    controller->Resume();
    return;
  }

  HoldController(std::move(controller));
  next_handler_->OnResponseStarted(response,
                                   std::make_unique<Controller>(this));
}

void DetachableResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  if (!next_handler_) {
    controller->Resume();
    return;
  }

  HoldController(std::move(controller));
  next_handler_->OnWillStart(url, std::make_unique<Controller>(this));
}

void DetachableResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  if (!next_handler_) {
    if (!read_buffer_.get())
      read_buffer_ = base::MakeRefCounted<net::IOBuffer>(kReadBufSize);
    *buf = read_buffer_;
    *buf_size = kReadBufSize;
    controller->Resume();
    return;
  }

  parent_read_buffer_ = buf;
  parent_read_buffer_size_ = buf_size;

  HoldController(std::move(controller));
  next_handler_->OnWillRead(buf, buf_size, std::make_unique<Controller>(this));
}

void DetachableResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  if (!next_handler_) {
    controller->Resume();
    return;
  }

  HoldController(std::move(controller));
  next_handler_->OnReadCompleted(bytes_read,
                                 std::make_unique<Controller>(this));
}

void DetachableResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  // No DCHECK(!is_deferred_) as the request may have been cancelled while
  // deferred.

  if (!next_handler_) {
    controller->Resume();
    return;
  }

  is_finished_ = true;

  HoldController(std::move(controller));
  next_handler_->OnResponseCompleted(status,
                                     std::make_unique<Controller>(this));
}

void DetachableResourceHandler::ResumeInternal() {
  parent_read_buffer_ = nullptr;
  parent_read_buffer_size_ = nullptr;
  Resume();
}

void DetachableResourceHandler::OnTimedOut() {
  // Requests are only timed out after being detached, and shouldn't be deferred
  // once detached.
  DCHECK(!next_handler_);
  DCHECK(!has_controller());

  OutOfBandCancel(net::ERR_ABORTED, true /* tell_renderer */);
}

}  // namespace content
