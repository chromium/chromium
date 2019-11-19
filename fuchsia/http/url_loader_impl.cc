// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/http/url_loader_impl.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/task/post_task.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"

namespace oldhttp = ::fuchsia::net::oldhttp;

namespace {
// Capacity, in bytes, for buffers used to move data from client requests or
// server responses.
const size_t kReadCapacity = 1024;

// The number of active requests. Used for testing.
int g_active_requests = 0;

// Converts |buffer| into a URLBody with the body set to a buffer. Returns
// nullptr when an error occurs.
oldhttp::URLBodyPtr CreateURLBodyFromBuffer(net::GrowableIOBuffer* buffer) {
  oldhttp::URLBodyPtr body = oldhttp::URLBody::New();

  // The response buffer size is exactly the offset.
  size_t total_size = buffer->offset();

  body->set_buffer(cr_fuchsia::MemBufferFromString(
      base::StringPiece(buffer->StartOfBuffer(), total_size),
      "cr-http-url-body"));

  return body;
}

int NetErrorToHttpError(int net_error) {
  // TODO(https://crbug.com/875533): Convert the Chromium //net error to their
  // Fuchsia counterpart.
  return net_error;
}

oldhttp::HttpErrorPtr BuildError(int net_error) {
  if (net_error == net::OK) {
    return nullptr;
  }

  oldhttp::HttpErrorPtr error = oldhttp::HttpError::New();
  error->code = NetErrorToHttpError(net_error);
  error->description = net::ErrorToString(net_error);
  return error;
}

std::unique_ptr<net::UploadDataStream> UploadDataStreamFromZxSocket(
    zx::socket stream) {
  // TODO(http://crbug.com/875534): Write a ZxStreamUploadStream class.
  std::unique_ptr<net::ChunkedUploadDataStream> upload_stream =
      std::make_unique<net::ChunkedUploadDataStream>(0);
  char buffer[kReadCapacity];
  size_t size = 0;
  zx_status_t result = ZX_OK;
  while (true) {
    result = stream.read(0, buffer, kReadCapacity, &size);
    if (result != ZX_OK) {
      ZX_DLOG(WARNING, result) << "zx_socket_read";
      return nullptr;
    }
    if (size < kReadCapacity) {
      upload_stream->AppendData(buffer, size, false);
      break;
    }
    upload_stream->AppendData(buffer, size, true);
  }

  return upload_stream;
}

std::unique_ptr<net::UploadDataStream> UploadDataStreamFromMemBuffer(
    fuchsia::mem::Buffer mem_buffer) {
  // TODO(http://crbug.com/875534): Write a ZxMemBufferUploadStream class.
  std::unique_ptr<net::ChunkedUploadDataStream> upload_stream =
      std::make_unique<net::ChunkedUploadDataStream>(0);

  char buffer[kReadCapacity];
  size_t size = mem_buffer.size;
  size_t offset = 0;
  zx_status_t result = ZX_OK;
  while (offset != size) {
    size_t length = std::min(size - offset, kReadCapacity);
    result = mem_buffer.vmo.read(buffer, offset, length);
    if (result != ZX_OK) {
      ZX_DLOG(WARNING, result) << "zx_vmo_read";
      return nullptr;
    }
    upload_stream->AppendData(buffer, length, false);
    offset += length;
  }

  return upload_stream;
}

}  // namespace

URLLoaderImpl::URLLoaderImpl(std::unique_ptr<net::URLRequestContext> context,
                             fidl::InterfaceRequest<oldhttp::URLLoader> request)
    : binding_(this, std::move(request)),
      context_(std::move(context)),
      buffer_(new net::GrowableIOBuffer()),
      write_watch_(FROM_HERE) {
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " URLLoader disconnected.";
    delete this;
  });
  g_active_requests++;
}

URLLoaderImpl::~URLLoaderImpl() {
  g_active_requests--;
}

int URLLoaderImpl::GetNumActiveRequestsForTests() {
  return g_active_requests;
}

void URLLoaderImpl::Start(oldhttp::URLRequest request, Callback callback) {
  if (net_request_) {
    callback(BuildResponse(net::ERR_IO_PENDING));
    return;
  }

  done_callback_ = std::move(callback);
  net_error_ = net::OK;

  // Create the URLRequest and set this object as the delegate.
  net_request_ = context_->CreateRequest(GURL(request.url),
                                         net::RequestPriority::MEDIUM, this);
  net_request_->set_method(request.method);

  // Set extra headers.
  if (request.headers) {
    for (oldhttp::HttpHeader header : *(request.headers)) {
      net_request_->SetExtraRequestHeaderByName(header.name, header.value,
                                                false);
    }
  }
  if (request.cache_mode == oldhttp::CacheMode::BYPASS_CACHE) {
    net_request_->SetExtraRequestHeaderByName("Cache-Control", "nocache",
                                              false);
  }

  std::unique_ptr<net::UploadDataStream> upload_stream;
  // Set body.
  if (request.body) {
    if (request.body->is_stream()) {
      upload_stream =
          UploadDataStreamFromZxSocket(std::move(request.body->stream()));
    } else {
      upload_stream =
          UploadDataStreamFromMemBuffer(std::move(request.body->buffer()));
    }

    if (!upload_stream) {
      std::move(done_callback_)(BuildResponse(net::ERR_ACCESS_DENIED));
      return;
    }
    net_request_->set_upload(std::move(upload_stream));
  }

  auto_follow_redirects_ = request.auto_follow_redirects;
  response_body_mode_ = request.response_body_mode;

  // Start the request.
  net_request_->Start();
}

void URLLoaderImpl::FollowRedirect(Callback callback) {
  if (!net_request_ || auto_follow_redirects_ ||
      !net_request_->is_redirecting()) {
    callback(BuildResponse(net::ERR_INVALID_HANDLE));
  }

  done_callback_ = std::move(callback);
  net_request_->FollowDeferredRedirect(base::nullopt /* removed_headers */,
                                       base::nullopt /* modified_headers */);
}

void URLLoaderImpl::QueryStatus(QueryStatusCallback callback) {
  oldhttp::URLLoaderStatus status;

  if (!net_request_) {
    status.is_loading = false;
  } else if (net_request_->is_pending() || net_request_->is_redirecting()) {
    status.is_loading = true;
  } else {
    status.is_loading = false;
    status.error = BuildError(net_error_);
  }

  callback(std::move(status));
}

void URLLoaderImpl::OnReceivedRedirect(net::URLRequest* request,
                                       const net::RedirectInfo& redirect_info,
                                       bool* defer_redirect) {
  DCHECK_EQ(net_request_.get(), request);
  // Follow redirect depending on policy.
  *defer_redirect = !auto_follow_redirects_;

  if (!auto_follow_redirects_) {
    oldhttp::URLResponse response = BuildResponse(net::OK);
    response.redirect_method = redirect_info.new_method;
    response.redirect_url = redirect_info.new_url.spec();
    response.redirect_referrer = redirect_info.new_referrer;
    std::move(done_callback_)(std::move(response));
  }
}

void URLLoaderImpl::OnAuthRequired(net::URLRequest* request,
                                   const net::AuthChallengeInfo& auth_info) {
  NOTIMPLEMENTED();
  DCHECK_EQ(net_request_.get(), request);
  request->CancelAuth();
}

void URLLoaderImpl::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NOTIMPLEMENTED();
  DCHECK_EQ(net_request_.get(), request);
  request->ContinueWithCertificate(nullptr, nullptr);
}

void URLLoaderImpl::OnSSLCertificateError(net::URLRequest* request,
                                          int net_error,
                                          const net::SSLInfo& ssl_info,
                                          bool fatal) {
  NOTIMPLEMENTED();
  DCHECK_EQ(net_request_.get(), request);
  request->Cancel();
}

void URLLoaderImpl::OnResponseStarted(net::URLRequest* request, int net_error) {
  DCHECK_EQ(net_request_.get(), request);
  net_error_ = net_error;

  // Return early if the request failed.
  if (net_error_ != net::OK) {
    std::move(done_callback_)(BuildResponse(net_error_));
    return;
  }

  // In stream mode, call the callback now and write to the socket.
  if (response_body_mode_ == oldhttp::ResponseBodyMode::STREAM ||
      response_body_mode_ == oldhttp::ResponseBodyMode::BUFFER_OR_STREAM) {
    zx::socket read_socket;
    zx_status_t result = zx::socket::create(0, &read_socket, &write_socket_);
    if (result != ZX_OK) {
      ZX_DLOG(WARNING, result) << "zx_socket_create";
      std::move(done_callback_)(BuildResponse(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }
    oldhttp::URLResponse response = BuildResponse(net::OK);
    response.body = oldhttp::URLBody::New();
    response.body->set_stream(std::move(read_socket));
    std::move(done_callback_)(std::move(response));
  }

  // In stream mode, the buffer is used as a temporary buffer to write to the
  // socket. In buffer mode, it is expanded as more of the response is read.
  buffer_->SetCapacity(kReadCapacity);

  ReadNextBuffer();
}

void URLLoaderImpl::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  DCHECK_EQ(net_request_.get(), request);
  if (WriteResponseBytes(bytes_read)) {
    ReadNextBuffer();
  }
}

void URLLoaderImpl::OnZxHandleSignalled(zx_handle_t handle,
                                        zx_signals_t signals) {
  // We should never have to process signals we didn't ask for.
  DCHECK((ZX_CHANNEL_WRITABLE | ZX_CHANNEL_PEER_CLOSED) & signals);
  DCHECK_GT(buffered_bytes_, 0);

  if (signals & ZX_CHANNEL_PEER_CLOSED) {
    return;
  }

  if (WriteResponseBytes(buffered_bytes_))
    ReadNextBuffer();
  buffered_bytes_ = 0;
}

void URLLoaderImpl::ReadNextBuffer() {
  int net_result;
  do {
    net_result = net_request_->Read(buffer_.get(), kReadCapacity);
    if (net_result == net::ERR_IO_PENDING) {
      return;
    }
  } while (WriteResponseBytes(net_result));
}

bool URLLoaderImpl::WriteResponseBytes(int result) {
  if (result < 0) {
    // Signal read error back to the client.
    if (write_socket_) {
      DCHECK(response_body_mode_ == oldhttp::ResponseBodyMode::STREAM ||
             response_body_mode_ ==
                 oldhttp::ResponseBodyMode::BUFFER_OR_STREAM);
      // There is no need to check the return value of this call as there is no
      // way to recover from a failed socket close.
      write_socket_ = zx::socket();
    } else {
      DCHECK_EQ(response_body_mode_, oldhttp::ResponseBodyMode::BUFFER);
      std::move(done_callback_)(BuildResponse(result));
    }
    return false;
  }

  if (result == 0) {
    // Read complete.
    if (write_socket_) {
      DCHECK(response_body_mode_ == oldhttp::ResponseBodyMode::STREAM ||
             response_body_mode_ ==
                 oldhttp::ResponseBodyMode::BUFFER_OR_STREAM);
      // In socket mode, attempt to shut down the socket and close it.
      write_socket_.shutdown(ZX_SOCKET_SHUTDOWN_WRITE);
      write_socket_ = zx::socket();
    } else {
      DCHECK_EQ(response_body_mode_, oldhttp::ResponseBodyMode::BUFFER);
      // In buffer mode, build the response and call the callback.
      oldhttp::URLBodyPtr body = CreateURLBodyFromBuffer(buffer_.get());
      if (body) {
        oldhttp::URLResponse response = BuildResponse(net::OK);
        response.body = std::move(body);
        std::move(done_callback_)(std::move(response));
      } else {
        std::move(done_callback_)(
            BuildResponse(net::ERR_INSUFFICIENT_RESOURCES));
      }
    }
    return false;
  }

  // Write data to the response buffer or socket.
  if (write_socket_) {
    DCHECK(response_body_mode_ == oldhttp::ResponseBodyMode::STREAM ||
           response_body_mode_ == oldhttp::ResponseBodyMode::BUFFER_OR_STREAM);
    // In socket mode, attempt to write to the socket.
    zx_status_t status =
        write_socket_.write(0, buffer_->data(), result, nullptr);
    if (status == ZX_ERR_SHOULD_WAIT) {
      // Wait until the socket is writable again.
      buffered_bytes_ = result;
      base::MessageLoopCurrentForIO::Get()->WatchZxHandle(
          write_socket_.get(), false /* persistent */,
          ZX_SOCKET_WRITABLE | ZX_SOCKET_PEER_CLOSED, &write_watch_, this);
      return false;
    }
    if (status != ZX_OK) {
      // Something went wrong, attempt to shut down the socket and close it.
      ZX_DLOG(WARNING, status) << "zx_socket_write";
      write_socket_ = zx::socket();
      return false;
    }
  } else {
    DCHECK_EQ(response_body_mode_, oldhttp::ResponseBodyMode::BUFFER);
    // In buffer mode, expand the buffer.
    buffer_->SetCapacity(buffer_->capacity() + result);
    buffer_->set_offset(buffer_->offset() + result);
  }

  return true;
}

oldhttp::URLResponse URLLoaderImpl::BuildResponse(int net_error) {
  oldhttp::URLResponse response;

  response.error = BuildError(net_error);
  if (response.error) {
    return response;
  }

  if (net_request_->url().is_valid()) {
    response.url = net_request_->url().spec();
  }
  response.status_code = net_request_->GetResponseCode();

  net::HttpResponseHeaders* response_headers = net_request_->response_headers();
  if (response_headers) {
    response.status_line = response_headers->GetStatusLine();
    std::string mime_type;
    if (response_headers->GetMimeType(&mime_type)) {
      response.mime_type = mime_type;
    }
    std::string charset;
    if (response_headers->GetCharset(&charset)) {
      response.charset = charset;
    }

    size_t iter = 0;
    std::string header_name;
    std::string header_value;
    response.headers.emplace();
    while (response_headers->EnumerateHeaderLines(&iter, &header_name,
                                                  &header_value)) {
      oldhttp::HttpHeader header;
      header.name = header_name;
      header.value = header_value;
      response.headers->push_back(header);
    }
  }

  return response;
}
