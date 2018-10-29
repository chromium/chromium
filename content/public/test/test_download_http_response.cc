// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_download_http_response.h"

#include <inttypes.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/numerics/ranges.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace content {

namespace {

// Lock object for protecting |g_parameters_map|.
base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;

using ParametersMap = std::map<GURL, TestDownloadHttpResponse::Parameters>;
// Maps url to Parameters so that requests for the same URL will get the same
// parameters.
base::LazyInstance<ParametersMap>::Leaky g_parameters_map =
    LAZY_INSTANCE_INITIALIZER;

const char* kTestDownloadPath = "/download/";

// The size of buffer to send the entity body. The header will always be sent in
// one buffer.
const int64_t kBufferSize = 64 * 1024;

// Xorshift* PRNG from https://en.wikipedia.org/wiki/Xorshift
uint64_t XorShift64StarWithIndex(uint64_t seed, uint64_t index) {
  const uint64_t kMultiplier = UINT64_C(2685821657736338717);
  uint64_t x = seed * kMultiplier + index;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  return x * kMultiplier;
}

// Called to resume the response.
void OnResume(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
              const base::Closure& resume_callback) {
  task_runner->PostTask(FROM_HERE, resume_callback);
}

void OnResponseSentOnServerIOThread(
    const TestDownloadHttpResponse::OnResponseSentCallback& callback,
    std::unique_ptr<TestDownloadHttpResponse::CompletedRequest> request) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, std::move(request)));
}

// The shim response object used by embedded_test_server. After this object is
// deleted, we may continue to send data with cached SendBytesCallback to
// support pause/resume behaviors.
class HttpResponse : public net::test_server::HttpResponse {
 public:
  explicit HttpResponse(base::WeakPtr<TestDownloadHttpResponse> owner)
      : owner_(owner) {}
  ~HttpResponse() override = default;

 private:
  // net::test_server::HttpResponse implementations.
  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override {
    if (owner_)
      owner_->SendResponse(send, done);
  }

  base::WeakPtr<TestDownloadHttpResponse> owner_;
  DISALLOW_COPY_AND_ASSIGN(HttpResponse);
};

}  // namespace

const char TestDownloadHttpResponse::kTestDownloadHostName[] =
    "*.default.example.com";

// static
GURL TestDownloadHttpResponse::GetNextURLForDownload() {
  static int index = 0;
  std::string url_string = base::StringPrintf("http://%d.default.example.com%s",
                                              ++index, kTestDownloadPath);
  return GURL(url_string);
}

TestDownloadHttpResponse::HttpResponseData::HttpResponseData(
    int64_t min_offset,
    int64_t max_offset,
    const std::string& response)
    : min_offset(min_offset), max_offset(max_offset), response(response) {}

// static
TestDownloadHttpResponse::Parameters
TestDownloadHttpResponse::Parameters::WithSingleInterruption(
    const TestDownloadHttpResponse::InjectErrorCallback& inject_error_cb) {
  Parameters parameters;
  parameters.injected_errors.push(parameters.size / 2);
  parameters.inject_error_cb = inject_error_cb;
  return parameters;
}

TestDownloadHttpResponse::Parameters::Parameters()
    : etag("abcd"),
      last_modified("Tue, 15 Nov 1994 12:45:26 GMT"),
      content_type("application/octet-stream"),
      size(102400),
      pattern_generator_seed(1),
      support_byte_ranges(true),
      support_partial_response(true),
      connection_type(
          net::HttpResponseInfo::ConnectionInfo::CONNECTION_INFO_UNKNOWN) {}

// Copy and move constructors / assignment operators are all defaults.
TestDownloadHttpResponse::Parameters::Parameters(const Parameters&) = default;
TestDownloadHttpResponse::Parameters& TestDownloadHttpResponse::Parameters::
operator=(const Parameters&) = default;

TestDownloadHttpResponse::Parameters::Parameters(Parameters&& that)
    : etag(std::move(that.etag)),
      last_modified(std::move(that.last_modified)),
      content_type(std::move(that.content_type)),
      size(that.size),
      pattern_generator_seed(that.pattern_generator_seed),
      support_byte_ranges(that.support_byte_ranges),
      support_partial_response(that.support_partial_response),
      connection_type(that.connection_type),
      static_response(std::move(that.static_response)),
      injected_errors(std::move(that.injected_errors)),
      inject_error_cb(that.inject_error_cb),
      on_pause_handler(that.on_pause_handler),
      pause_offset(that.pause_offset) {}

TestDownloadHttpResponse::Parameters& TestDownloadHttpResponse::Parameters::
operator=(Parameters&& that) {
  etag = std::move(that.etag);
  last_modified = std::move(that.last_modified);
  content_type = std::move(that.content_type);
  size = that.size;
  pattern_generator_seed = that.pattern_generator_seed;
  support_byte_ranges = that.support_byte_ranges;
  support_partial_response = that.support_partial_response;
  static_response = std::move(that.static_response);
  injected_errors = std::move(that.injected_errors);
  inject_error_cb = that.inject_error_cb;
  on_pause_handler = that.on_pause_handler;
  pause_offset = that.pause_offset;
  return *this;
}

TestDownloadHttpResponse::Parameters::~Parameters() = default;

void TestDownloadHttpResponse::Parameters::ClearInjectedErrors() {
  base::queue<int64_t> empty_error_list;
  injected_errors.swap(empty_error_list);
  inject_error_cb.Reset();
}

void TestDownloadHttpResponse::Parameters::SetResponseForRangeRequest(
    int64_t min_offset,
    int64_t max_offset,
    const std::string& response) {
  range_request_responses.emplace_back(
      HttpResponseData(min_offset, max_offset, response));
}

TestDownloadHttpResponse::CompletedRequest::CompletedRequest(
    const net::test_server::HttpRequest& request)
    : http_request(request) {}

TestDownloadHttpResponse::CompletedRequest::~CompletedRequest() = default;

// static
void TestDownloadHttpResponse::StartServing(
    const TestDownloadHttpResponse::Parameters& parameters,
    const GURL& url) {
  base::AutoLock lock(*g_lock.Pointer());
  auto iter = g_parameters_map.Get().find(url);
  if (iter != g_parameters_map.Get().end())
    g_parameters_map.Get().erase(iter);
  g_parameters_map.Get().emplace(url, std::move(parameters));
}

// static
void TestDownloadHttpResponse::StartServingStaticResponse(
    const std::string& response,
    const GURL& url) {
  TestDownloadHttpResponse::Parameters parameters;
  parameters.static_response = response;
  StartServing(std::move(parameters), url);
}

std::unique_ptr<net::test_server::HttpResponse>
TestDownloadHttpResponse::CreateResponseForTestServer() {
  return std::make_unique<HttpResponse>(weak_ptr_factory_.GetWeakPtr());
}

// static
std::string TestDownloadHttpResponse::GetPatternBytes(int seed,
                                                      int64_t starting_offset,
                                                      int length) {
  int64_t seed_offset = starting_offset / sizeof(int64_t);
  int64_t first_byte_position = starting_offset % sizeof(int64_t);
  std::string output;
  while (length > 0) {
    uint64_t data = XorShift64StarWithIndex(seed, seed_offset);
    int length_to_copy =
        std::min(length, static_cast<int>(sizeof(data) - first_byte_position));
    char* start_pos = reinterpret_cast<char*>(&data) + first_byte_position;
    std::string string_to_append(start_pos, start_pos + length_to_copy);
    output.append(string_to_append);
    length -= length_to_copy;
    ++seed_offset;
    first_byte_position = 0;
  }
  return output;
}

TestDownloadHttpResponse::TestDownloadHttpResponse(
    const net::test_server::HttpRequest& request,
    const Parameters& parameters,
    const OnResponseSentCallback& on_response_sent_callback)
    : range_(net::HttpByteRange::Bounded(0, parameters.size - 1)),
      response_sent_offset_(0u),
      parameters_(std::move(parameters)),
      request_(request),
      transferred_bytes_(0u),
      on_response_sent_callback_(on_response_sent_callback),
      weak_ptr_factory_(this) {
  DCHECK_GT(parameters.size, 0) << "File size need to be greater than 0.";
  ParseRequestHeader();
}

TestDownloadHttpResponse::~TestDownloadHttpResponse() = default;

void TestDownloadHttpResponse::SendResponse(
    const net::test_server::SendBytesCallback& send,
    const net::test_server::SendCompleteCallback& done) {
  bytes_sender_ = send;
  done_callback_ = done;

  // Throw error before sending headers.
  if (ShouldAbortImmediately()) {
    bytes_sender_.Run(std::string(), GenerateResultClosure());
    return;
  }

  // Call inject error callback to UI thread.
  if (!parameters_.injected_errors.empty() &&
      parameters_.injected_errors.front() <= range_.last_byte_position() &&
      parameters_.injected_errors.front() >= range_.first_byte_position() &&
      !parameters_.inject_error_cb.is_null()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(parameters_.inject_error_cb,
                       range_.first_byte_position(),
                       parameters_.injected_errors.front() -
                           range_.first_byte_position()));
  }

  // Pause before sending headers.
  if (ShouldPauseImmediately()) {
    PauseResponsesAndWaitForResumption();
    return;
  }

  // Start to send the response.
  SendResponseHeaders();
}

void TestDownloadHttpResponse::ParseRequestHeader() {
  // Parse HTTP range header from the request.
  std::vector<net::HttpByteRange> ranges;
  if (request_.headers.find(net::HttpRequestHeaders::kRange) ==
      request_.headers.end()) {
    return;
  }

  if (!net::HttpUtil::ParseRangeHeader(
          request_.headers.at(net::HttpRequestHeaders::kRange), &ranges)) {
    return;
  }

  if (ranges.size() > 1)
    LOG(WARNING) << "Multiple range intervals are not supported.";

  // Adjust the response range according to request range. The first byte offset
  // of the request may be larger than entity body size.
  request_range_ = ranges[0];
  if (parameters_.support_partial_response)
    range_.set_first_byte_position(request_range_.first_byte_position());
  range_.ComputeBounds(parameters_.size);

  response_sent_offset_ = range_.first_byte_position();
}

void TestDownloadHttpResponse::SendResponseHeaders() {
  // Send static response in |parameters_| and close connection.
  if (!parameters_.static_response.empty()) {
    bytes_sender_.Run(parameters_.static_response, GenerateResultClosure());
    return;
  }

  // Send static |range_request_responses| in |parameters_| and close
  // connection.
  std::string response;
  if (GetResponseForRangeRequest(&response)) {
    bytes_sender_.Run(response, GenerateResultClosure());
    return;
  }

  // Send the headers and start to send the body.
  bytes_sender_.Run(GetDefaultResponseHeaders(), SendNextBodyChunkClosure());
}

std::string TestDownloadHttpResponse::GetDefaultResponseHeaders() {
  std::string headers;
  // Send partial response.
  if (parameters_.support_partial_response && parameters_.support_byte_ranges &&
      request_.headers.find(net::HttpRequestHeaders::kIfRange) !=
          request_.headers.end() &&
      request_.headers.at(net::HttpRequestHeaders::kIfRange) ==
          parameters_.etag &&
      HandleRangeAssumingValidatorMatch(headers)) {
    return headers;
  }

  // Send precondition failed for "If-Match" request header.
  if (parameters_.support_partial_response && parameters_.support_byte_ranges &&
      request_.headers.find(net::HttpRequestHeaders::kIfMatch) !=
          request_.headers.end()) {
    if (request_.headers.at(net::HttpRequestHeaders::kIfMatch) !=
            parameters_.etag ||
        !HandleRangeAssumingValidatorMatch(headers)) {
      // Unlike If-Range, If-Match returns an error if the validators don't
      // match.
      headers =
          "HTTP/1.1 412 Precondition failed\r\n"
          "Content-Length: 0\r\n"
          "\r\n";
    }
    return headers;
  }

  // Send the whole file in entity body if partial response is not supported.
  range_.set_first_byte_position(0u);
  range_.set_last_byte_position(parameters_.size - 1);
  response_sent_offset_ = 0;

  headers.append("HTTP/1.1 200 OK\r\n");
  if (parameters_.support_byte_ranges)
    headers.append("Accept-Ranges: bytes\r\n");
  headers.append(
      base::StringPrintf("Content-Length: %" PRId64 "\r\n", parameters_.size));
  headers.append(GetCommonEntityHeaders());
  return headers;
}

bool TestDownloadHttpResponse::GetResponseForRangeRequest(std::string* output) {
  if (!range_.IsValid())
    return false;

  // Find the response for range request that starts from |requset_offset|.
  // Use default logic to generate the response if nothing can be found.
  int64_t requset_offset = range_.first_byte_position();
  for (const auto& response : parameters_.range_request_responses) {
    if (response.min_offset == -1 && response.max_offset == -1)
      continue;

    if (requset_offset < response.min_offset)
      continue;

    if (response.max_offset == -1 || requset_offset <= response.max_offset) {
      *output = response.response;
      return true;
    }
  }

  return false;
}

bool TestDownloadHttpResponse::HandleRangeAssumingValidatorMatch(
    std::string& response) {
  // The request may have specified a range that's out of bounds.
  if (request_range_.first_byte_position() >= parameters_.size) {
    response = base::StringPrintf(
        "HTTP/1.1 416 Range not satisfiable\r\n"
        "Content-Range: bytes */%" PRId64
        "\r\n"
        "Content-Length: 0\r\n",
        parameters_.size);
    return true;
  }

  response.append("HTTP/1.1 206 Partial content\r\n");
  response.append(base::StringPrintf(
      "Content-Range: bytes %" PRId64 "-%" PRId64 "/%" PRId64 "\r\n",
      range_.first_byte_position(), range_.last_byte_position(),
      parameters_.size));
  response.append(base::StringPrintf(
      "Content-Length: %" PRId64 "\r\n",
      (range_.last_byte_position() - range_.first_byte_position()) + 1));
  response.append(GetCommonEntityHeaders());
  return true;
}

std::string TestDownloadHttpResponse::GetCommonEntityHeaders() {
  std::string headers;
  if (!parameters_.content_type.empty()) {
    headers.append(base::StringPrintf("Content-Type: %s\r\n",
                                      parameters_.content_type.c_str()));
  }

  if (!parameters_.etag.empty()) {
    headers.append(
        base::StringPrintf("ETag: %s\r\n", parameters_.etag.c_str()));
  }

  if (!parameters_.last_modified.empty()) {
    headers.append(base::StringPrintf("Last-Modified: %s\r\n",
                                      parameters_.last_modified.c_str()));
  }
  headers.append("\r\n");
  return headers;
}

std::string TestDownloadHttpResponse::GetResponseChunk(
    const net::HttpByteRange& buffer_range) {
  DCHECK(buffer_range.IsValid());
  DCHECK(buffer_range.HasLastBytePosition());

  int64_t length = buffer_range.last_byte_position() -
                   buffer_range.first_byte_position() + 1;
  return GetPatternBytes(parameters_.pattern_generator_seed,
                         buffer_range.first_byte_position(), length);
}

bool TestDownloadHttpResponse::ShouldAbortImmediately() const {
  return !parameters_.injected_errors.empty() &&
         parameters_.injected_errors.front() == -1 &&
         !parameters_.inject_error_cb.is_null();
}

bool TestDownloadHttpResponse::ShouldPauseImmediately() const {
  return parameters_.pause_offset.has_value() &&
         parameters_.pause_offset.value() == -1 && parameters_.on_pause_handler;
}

bool TestDownloadHttpResponse::HandlePause(
    const net::HttpByteRange& buffer_range) {
  if (!parameters_.on_pause_handler || !parameters_.pause_offset.has_value())
    return false;

  int64_t pause_offset = parameters_.pause_offset.value();
  if (pause_offset < request_range_.first_byte_position())
    return false;

  if (pause_offset > buffer_range.last_byte_position() ||
      pause_offset < buffer_range.first_byte_position()) {
    return false;
  }

  // Send the bytes before the pause offset.
  net::HttpByteRange range = buffer_range;
  if (range.last_byte_position() > pause_offset) {
    range.set_last_byte_position(pause_offset - 1);
    response_sent_offset_ = pause_offset;
    base::RepeatingClosure nothing = base::BindRepeating([]() {});
    SendBodyChunkInternal(range, nothing);
  }

  // Pause now. Don't close the connection to wait for resumption.
  PauseResponsesAndWaitForResumption();
  return true;
}

bool TestDownloadHttpResponse::HandleInjectedError(
    const net::HttpByteRange& buffer_range) {
  if (parameters_.injected_errors.empty())
    return false;

  // Clear all errors before first byte of |range|.
  while (!parameters_.injected_errors.empty() &&
         parameters_.injected_errors.front() <
             buffer_range.first_byte_position()) {
    parameters_.injected_errors.pop();
  }

  int64_t error_offset = parameters_.injected_errors.front();
  if (error_offset > buffer_range.last_byte_position())
    return false;

  // Send the bytes before the error offset, then close the connection.
  net::HttpByteRange range = buffer_range;
  if (error_offset > buffer_range.first_byte_position()) {
    range.set_last_byte_position(error_offset - 1);
    DCHECK(range.IsValid());
    response_sent_offset_ = error_offset;
    SendBodyChunkInternal(range, GenerateResultClosure());
  }

  return true;
}

bool TestDownloadHttpResponse::ShouldPause(
    const net::HttpByteRange& buffer_range) const {
  if (!parameters_.on_pause_handler)
    return false;

  return parameters_.pause_offset >= buffer_range.first_byte_position() &&
         parameters_.pause_offset <= buffer_range.last_byte_position();
}

void TestDownloadHttpResponse::PauseResponsesAndWaitForResumption() {
  // Clean up the on_pause_handler so response will not be paused again.
  auto pause_callback = parameters_.on_pause_handler;
  parameters_.on_pause_handler.Reset();

  base::RepeatingClosure continue_closure = SendNextBodyChunkClosure();

  // We may pause before sending the headers.
  if (parameters_.pause_offset == -1) {
    continue_closure = base::BindRepeating(
        &TestDownloadHttpResponse::SendResponseHeaders, base::Unretained(this));
  }

  // Continue to send data after resumption.
  // TODO(xingliu): Unwind thread hopping callbacks here.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          pause_callback,
          base::BindRepeating(&OnResume, base::ThreadTaskRunnerHandle::Get(),
                              continue_closure)));
}

void TestDownloadHttpResponse::SendResponseBodyChunk() {
  // Close the connection when reaching the end.
  if (response_sent_offset_ > range_.last_byte_position()) {
    GenerateResult();
    return;
  }

  int64_t upper_bound = base::ClampToRange(response_sent_offset_ + kBufferSize,
                                           range_.first_byte_position(),
                                           range_.last_byte_position());
  auto buffer_range =
      net::HttpByteRange::Bounded(response_sent_offset_, upper_bound);

  // Handle pause if needed.
  if (HandlePause(buffer_range))
    return;

  // Handle injected error if needed.
  if (HandleInjectedError(buffer_range))
    return;

  // Send the data buffer by buffer without throwing errors.
  response_sent_offset_ = buffer_range.last_byte_position() + 1;
  SendBodyChunkInternal(buffer_range, SendNextBodyChunkClosure());
  return;
}

void TestDownloadHttpResponse::SendBodyChunkInternal(
    const net::HttpByteRange& buffer_range,
    const base::RepeatingClosure& next) {
  std::string response_chunk = GetResponseChunk(buffer_range);
  transferred_bytes_ += static_cast<int64_t>(response_chunk.size());
  bytes_sender_.Run(response_chunk, next);
}

net::test_server::SendCompleteCallback
TestDownloadHttpResponse::SendNextBodyChunkClosure() {
  return base::BindRepeating(&TestDownloadHttpResponse::SendResponseBodyChunk,
                             base::Unretained(this));
}

void TestDownloadHttpResponse::GenerateResult() {
  auto completed_request = std::make_unique<CompletedRequest>(request_);
  // Transferred bytes in [range_.first_byte_position(), response_sent_offset_).
  completed_request->transferred_byte_count = transferred_bytes_;
  OnResponseSentOnServerIOThread(on_response_sent_callback_,
                                 std::move(completed_request));

  // Close the HTTP connection.
  done_callback_.Run();
}

net::test_server::SendCompleteCallback
TestDownloadHttpResponse::GenerateResultClosure() {
  return base::BindRepeating(&TestDownloadHttpResponse::GenerateResult,
                             base::Unretained(this));
}

std::unique_ptr<net::test_server::HttpResponse>
TestDownloadResponseHandler::HandleTestDownloadRequest(
    const TestDownloadHttpResponse::OnResponseSentCallback& callback,
    const net::test_server::HttpRequest& request) {
  server_task_runner_ = base::MessageLoopCurrent::Get()->task_runner();

  if (request.headers.find(net::HttpRequestHeaders::kHost) ==
      request.headers.end()) {
    return nullptr;
  }

  base::AutoLock lock(*g_lock.Pointer());
  GURL url(base::StringPrintf(
      "http://%s%s", request.headers.at(net::HttpRequestHeaders::kHost).c_str(),
      request.relative_url.c_str()));
  auto iter = g_parameters_map.Get().find(url);
  if (iter != g_parameters_map.Get().end()) {
    auto test_response = std::make_unique<TestDownloadHttpResponse>(
        request, std::move(iter->second), callback);
    auto response = test_response->CreateResponseForTestServer();
    responses_.emplace_back(std::move(test_response));
    return response;
  }
  return nullptr;
}

TestDownloadResponseHandler::TestDownloadResponseHandler() = default;

TestDownloadResponseHandler::~TestDownloadResponseHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& response : responses_)
    server_task_runner_->DeleteSoon(FROM_HERE, response.release());
}

void TestDownloadResponseHandler::RegisterToTestServer(
    net::test_server::EmbeddedTestServer* server) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!server->Started())
      << "Register request handler before starting the server";
  server->RegisterRequestHandler(base::Bind(
      &content::TestDownloadResponseHandler::HandleTestDownloadRequest,
      base::Unretained(this),
      base::Bind(&content::TestDownloadResponseHandler::OnRequestCompleted,
                 base::Unretained(this))));
}

void TestDownloadResponseHandler::OnRequestCompleted(
    std::unique_ptr<TestDownloadHttpResponse::CompletedRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  completed_requests_.push_back(std::move(request));

  if (run_loop_ && run_loop_->running() &&
      completed_requests().size() >= request_count_) {
    run_loop_->Quit();
  }
}

void TestDownloadResponseHandler::WaitUntilCompletion(size_t request_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_count_ = request_count;

  if ((run_loop_ && run_loop_->running()) ||
      completed_requests().size() >= request_count_) {
    return;
  }

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
}

}  // namespace content
