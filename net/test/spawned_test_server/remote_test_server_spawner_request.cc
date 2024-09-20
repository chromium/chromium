// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/remote_test_server_spawner_request.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/port_util.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"

namespace net {

static const int kBufferSize = 2048;

class RemoteTestServerSpawnerRequest::Core : public URLRequest::Delegate {
 public:
  Core();

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  void SendRequest(const GURL& url, const std::string& post_data);

  // Blocks until request is finished. If |response| isn't nullptr then server
  // response is copied to *response. Returns true if the request was completed
  // successfully.
  [[nodiscard]] bool WaitForCompletion(std::string* response);

 private:
  // URLRequest::Delegate methods.
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int num_bytes) override;

  void ReadResponse();
  void OnCommandCompleted(int net_error);

  // Request results.
  int result_code_ = 0;
  std::string data_received_;

  // WaitableEvent to notify when the request is finished.
  base::WaitableEvent event_;

  std::unique_ptr<URLRequestContext> context_;
  std::unique_ptr<URLRequest> request_;

  scoped_refptr<IOBuffer> read_buffer_;

  THREAD_CHECKER(thread_checker_);
};

RemoteTestServerSpawnerRequest::Core::Core()
    : event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      read_buffer_(base::MakeRefCounted<IOBufferWithSize>(kBufferSize)) {
  DETACH_FROM_THREAD(thread_checker_);
}

void RemoteTestServerSpawnerRequest::Core::SendRequest(
    const GURL& url,
    const std::string& post_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Prepare the URLRequest for sending the command.
  DCHECK(!request_.get());
  context_ = CreateTestURLRequestContextBuilder()->Build();
  request_ = context_->CreateRequest(url, DEFAULT_PRIORITY, this,
                                     TRAFFIC_ANNOTATION_FOR_TESTS);

  if (post_data.empty()) {
    request_->set_method("GET");
  } else {
    request_->set_method("POST");
    std::unique_ptr<UploadElementReader> reader(
        UploadOwnedBytesElementReader::CreateWithString(post_data));
    request_->set_upload(
        ElementsUploadDataStream::CreateWithReader(std::move(reader)));
    request_->SetExtraRequestHeaderByName(HttpRequestHeaders::kContentType,
                                          "application/json",
                                          /*overwrite=*/true);
  }

  request_->Start();
}

RemoteTestServerSpawnerRequest::Core::~Core() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool RemoteTestServerSpawnerRequest::Core::WaitForCompletion(
    std::string* response) {
  // Called by RemoteTestServerSpawnerRequest::WaitForCompletion() on the main
  // thread.

  event_.Wait();
  if (response)
    *response = data_received_;
  return result_code_ == OK;
}

void RemoteTestServerSpawnerRequest::Core::OnCommandCompleted(int net_error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(ERR_IO_PENDING, net_error);
  DCHECK(!event_.IsSignaled());

  // If request has failed, return the error code.
  if (net_error != OK) {
    LOG(ERROR) << "request failed, error: " << ErrorToString(net_error);
    result_code_ = net_error;
  } else if (request_->GetResponseCode() != 200) {
    LOG(ERROR) << "Spawner server returned bad status: "
               << request_->response_headers()->GetStatusLine() << ", "
               << data_received_;
    result_code_ = ERR_FAILED;
  }

  if (result_code_ != OK)
    data_received_.clear();

  request_.reset();
  context_.reset();

  event_.Signal();
}

void RemoteTestServerSpawnerRequest::Core::ReadResponse() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  while (true) {
    int result = request_->Read(read_buffer_.get(), kBufferSize);
    if (result == ERR_IO_PENDING)
      return;

    if (result <= 0) {
      OnCommandCompleted(result);
      return;
    }

    data_received_.append(read_buffer_->data(), result);
  }
}

void RemoteTestServerSpawnerRequest::Core::OnResponseStarted(
    URLRequest* request,
    int net_error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(ERR_IO_PENDING, net_error);
  DCHECK_EQ(request, request_.get());

  if (net_error != OK) {
    OnCommandCompleted(net_error);
    return;
  }

  ReadResponse();
}

void RemoteTestServerSpawnerRequest::Core::OnReadCompleted(URLRequest* request,
                                                           int read_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(ERR_IO_PENDING, read_result);
  DCHECK_EQ(request, request_.get());

  if (read_result <= 0) {
    OnCommandCompleted(read_result);
    return;
  }

  data_received_.append(read_buffer_->data(), read_result);

  ReadResponse();
}

RemoteTestServerSpawnerRequest::RemoteTestServerSpawnerRequest(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const GURL& url,
    const std::string& post_data)
    : io_task_runner_(io_task_runner),
      core_(std::make_unique<Core>()),
      allowed_port_(
          std::make_unique<ScopedPortException>(url.EffectiveIntPort())) {
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::SendRequest,
                                base::Unretained(core_.get()), url, post_data));
}

RemoteTestServerSpawnerRequest::~RemoteTestServerSpawnerRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  io_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

bool RemoteTestServerSpawnerRequest::WaitForCompletion(std::string* response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return core_->WaitForCompletion(response);
}

}  // namespace net
