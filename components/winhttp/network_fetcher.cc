// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/network_fetcher.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/win/windows_version.h"
#include "components/winhttp/net_util.h"
#include "components/winhttp/proxy_info.h"
#include "components/winhttp/scoped_hinternet.h"
#include "components/winhttp/scoped_winttp_proxy_info.h"
#include "url/url_constants.h"

namespace winhttp {
namespace {

// TODO(crbug.com/40163568) - implement a way to express priority for
// foreground/background network fetches.
constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

void CrackUrl(const GURL& url,
              bool* is_https,
              std::string* host,
              int* port,
              std::string* path_for_request) {
  if (is_https) {
    *is_https = url.SchemeIs(url::kHttpsScheme);
  }
  if (host) {
    *host = url.host();
  }
  if (port) {
    *port = url.EffectiveIntPort();
  }
  if (path_for_request) {
    *path_for_request = url.PathForRequest();
  }
}

}  // namespace

NetworkFetcher::NetworkFetcher(
    scoped_refptr<SharedHInternet> session_handle,
    scoped_refptr<ProxyConfiguration> proxy_configuration)
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      session_handle_(session_handle),
      proxy_configuration_(proxy_configuration) {}

NetworkFetcher::~NetworkFetcher() {
  DVLOG(3) << __func__;
}

void NetworkFetcher::HandleClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `write_data_callback_` maintains an outstanding reference to this object
  // and the reference must be released to avoid leaking the object.
  write_data_callback_.Reset();
  self_ = nullptr;
}

void NetworkFetcher::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_handle_.reset();
}

void NetworkFetcher::CompleteFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file_.IsValid()) {
    std::move(fetch_complete_callback_).Run(response_code_);
    return;
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce([](base::File& file) { file.Close(); }, std::ref(file_)),
      base::BindOnce(&NetworkFetcher::CompleteFetch, this));
}

HRESULT NetworkFetcher::QueryHeaderString(const std::wstring& name,
                                          std::wstring* value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request_handle_.is_valid()) {
    return HRESULT_FROM_WIN32(ERROR_CANCELLED);
  }
  return QueryHeadersString(request_handle_.get(), WINHTTP_QUERY_CUSTOM,
                            name.c_str(), value);
}

HRESULT NetworkFetcher::QueryHeaderInt(const std::wstring& name,
                                       int* value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request_handle_.is_valid()) {
    return HRESULT_FROM_WIN32(ERROR_CANCELLED);
  }
  return QueryHeadersInt(request_handle_.get(), WINHTTP_QUERY_CUSTOM,
                         name.c_str(), value);
}

std::string NetworkFetcher::GetResponseBody() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return post_response_body_;
}

HRESULT NetworkFetcher::GetNetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return net_error_;
}

base::FilePath NetworkFetcher::GetFilePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return file_path_;
}

int64_t NetworkFetcher::GetContentSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_size_;
}

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    FetchStartedCallback fetch_started_callback,
    FetchProgressCallback fetch_progress_callback,
    FetchCompleteCallback fetch_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_ = url;
  fetch_started_callback_ = std::move(fetch_started_callback);
  fetch_progress_callback_ = std::move(fetch_progress_callback);
  fetch_complete_callback_ = std::move(fetch_complete_callback);

  CrackUrl(url_, &is_https_, &host_, &port_, &path_for_request_);

  verb_ = L"POST";
  content_type_ = content_type;
  write_data_callback_ =
      base::BindRepeating(&NetworkFetcher::WriteDataToMemory, this);

  net_error_ = BeginFetch(post_data, post_additional_headers);

  if (FAILED(net_error_)) {
    CompleteFetch();
  }
}

base::OnceClosure NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    FetchStartedCallback fetch_started_callback,
    FetchProgressCallback fetch_progress_callback,
    FetchCompleteCallback fetch_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_ = url;
  file_path_ = file_path;
  fetch_started_callback_ = std::move(fetch_started_callback);
  fetch_progress_callback_ = std::move(fetch_progress_callback);
  fetch_complete_callback_ = std::move(fetch_complete_callback);

  CrackUrl(url, &is_https_, &host_, &port_, &path_for_request_);

  verb_ = L"GET";
  write_data_callback_ =
      base::BindRepeating(&NetworkFetcher::WriteDataToFile, this);

  net_error_ = BeginFetch({}, {});

  if (FAILED(net_error_)) {
    CompleteFetch();
  }

  return base::BindOnce(&NetworkFetcher::Close, this);
}

HRESULT NetworkFetcher::BeginFetch(
    const std::string& data,
    const base::flat_map<std::string, std::string>& additional_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url_.SchemeIsHTTPOrHTTPS()) {
    return E_INVALIDARG;
  }

  connect_handle_ = Connect();
  if (!connect_handle_.get()) {
    return HRESULTFromLastError();
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&NetworkFetcher::GetProxyForUrl, this),
      base::BindOnce(&NetworkFetcher::ContinueFetch, this, data,
                     additional_headers));
  return S_OK;
}

std::optional<ScopedWinHttpProxyInfo> NetworkFetcher::GetProxyForUrl() {
  return proxy_configuration_->GetProxyForUrl(session_handle_->handle(), url_);
}

void NetworkFetcher::ContinueFetch(
    const std::string& data,
    base::flat_map<std::string, std::string> additional_headers,
    std::optional<ScopedWinHttpProxyInfo> winhttp_proxy_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net_error_ = [&] {
    request_handle_ = OpenRequest();
    if (!request_handle_.get()) {
      return HRESULTFromLastError();
    }

    SetProxyForRequest(request_handle_.get(), winhttp_proxy_info);

    const auto winhttp_callback = ::WinHttpSetStatusCallback(
        request_handle_.get(), &NetworkFetcher::WinHttpStatusCallback,
        WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);
    if (winhttp_callback == WINHTTP_INVALID_STATUS_CALLBACK) {
      return HRESULTFromLastError();
    }

    auto hr = SetOption(request_handle_.get(), WINHTTP_OPTION_CONTEXT_VALUE,
                        context());
    if (FAILED(hr)) {
      return hr;
    }

    // The reference is released when the request handle is closed.
    self_ = this;

    // Disables both saving and sending cookies.
    hr = SetOption(request_handle_.get(), WINHTTP_OPTION_DISABLE_FEATURE,
                   WINHTTP_DISABLE_COOKIES);
    if (FAILED(hr)) {
      return hr;
    }

    if (!content_type_.empty()) {
      additional_headers.insert({"Content-Type", content_type_});
    }

    for (const auto& [name, value] : additional_headers) {
      const std::wstring raw_header =
          base::SysUTF8ToWide(base::StrCat({name, ": ", value, "\r\n"}));
      if (!::WinHttpAddRequestHeaders(
              request_handle_.get(), raw_header.c_str(), raw_header.size(),
              WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
        PLOG(ERROR) << "Failed to set the request header: " << raw_header;
      }
    }

    hr = SendRequest(data);
    if (FAILED(hr)) {
      return hr;
    }

    return S_OK;
  }();

  if (FAILED(net_error_)) {
    CompleteFetch();
  }
}

ScopedHInternet NetworkFetcher::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ScopedHInternet(::WinHttpConnect(
      session_handle_->handle(), base::SysUTF8ToWide(host_).c_str(), port_, 0));
}

ScopedHInternet NetworkFetcher::OpenRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t flags = WINHTTP_FLAG_REFRESH;
  if (is_https_) {
    flags |= WINHTTP_FLAG_SECURE;
  }
  return ScopedHInternet(::WinHttpOpenRequest(
      connect_handle_.get(), verb_.data(),
      base::SysUTF8ToWide(path_for_request_).c_str(), nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
}

HRESULT NetworkFetcher::SendRequest(const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(2) << data;

  // Make a copy of the request data to ensure the buffer is available until
  // the request is processed.
  request_data_ = data;

  const uint32_t bytes_to_send =
      base::saturated_cast<uint32_t>(request_data_.size());
  void* request_body = bytes_to_send ? const_cast<char*>(request_data_.c_str())
                                     : WINHTTP_NO_REQUEST_DATA;
  if (!::WinHttpSendRequest(request_handle_.get(),
                            WINHTTP_NO_ADDITIONAL_HEADERS, 0, request_body,
                            bytes_to_send, bytes_to_send, context())) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

void NetworkFetcher::SendRequestComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net_error_ = ReceiveResponse();
  if (FAILED(net_error_)) {
    CompleteFetch();
  }
}

HRESULT NetworkFetcher::ReceiveResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request_handle_.is_valid()) {
    return HRESULT_FROM_WIN32(ERROR_CANCELLED);
  }
  if (!::WinHttpReceiveResponse(request_handle_.get(), nullptr)) {
    return HRESULTFromLastError();
  }
  return S_OK;
}

void NetworkFetcher::HeadersAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request_handle_.is_valid()) {
    CompleteFetch();
    return;
  }

  std::wstring request_headers;
  QueryHeadersString(
      request_handle_.get(),
      WINHTTP_QUERY_RAW_HEADERS_CRLF | WINHTTP_QUERY_FLAG_REQUEST_HEADERS,
      WINHTTP_HEADER_NAME_BY_INDEX, &request_headers);
  VLOG(3) << "request headers:" << std::endl << request_headers;
  std::wstring response_headers;
  QueryHeadersString(request_handle_.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF,
                     WINHTTP_HEADER_NAME_BY_INDEX, &response_headers);
  VLOG(3) << "response headers:" << std::endl << response_headers;

  net_error_ = QueryHeadersInt(request_handle_.get(), WINHTTP_QUERY_STATUS_CODE,
                               WINHTTP_HEADER_NAME_BY_INDEX, &response_code_);
  if (FAILED(net_error_)) {
    CompleteFetch();
    return;
  }

  int content_length = 0;
  net_error_ =
      QueryHeadersInt(request_handle_.get(), WINHTTP_QUERY_CONTENT_LENGTH,
                      WINHTTP_HEADER_NAME_BY_INDEX, &content_length);
  std::move(fetch_started_callback_)
      .Run(response_code_, SUCCEEDED(net_error_) ? content_length : -1);

  // Start reading the body of response.
  net_error_ = ReadData();
  if (FAILED(net_error_)) {
    CompleteFetch();
  }
}

HRESULT NetworkFetcher::ReadData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request_handle_.is_valid()) {
    return HRESULT_FROM_WIN32(ERROR_CANCELLED);
  }

  // Use a fixed buffer size, larger than the internal WinHTTP buffer size (8K),
  // according to the documentation for WinHttpReadData.
  constexpr size_t kNumBytesToRead = 0x4000;  // 16KiB.
  read_buffer_.resize(kNumBytesToRead);

  if (!::WinHttpReadData(request_handle_.get(), &read_buffer_.front(),
                         read_buffer_.size(), nullptr)) {
    return HRESULTFromLastError();
  }

  DVLOG(3) << "reading data...";
  return S_OK;
}

void NetworkFetcher::ReadDataComplete(size_t num_bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  read_buffer_.resize(num_bytes_read);
  if (write_data_callback_) {
    write_data_callback_.Run();
  }
}

void NetworkFetcher::RequestError(DWORD error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net_error_ = HRESULT_FROM_WIN32(error);
  CompleteFetch();
}

void NetworkFetcher::WriteDataToFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(3) << __func__;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&NetworkFetcher::WriteDataToFileBlocking, this),
      base::BindOnce(&NetworkFetcher::WriteDataToFileComplete, this));
}

// Returns true if EOF is reached.
bool NetworkFetcher::WriteDataToFileBlocking() {
  VLOG(3) << __func__;

  if (read_buffer_.empty()) {
    file_.Close();
    net_error_ = S_OK;
    return true;
  }

  if (!file_.IsValid()) {
    file_.Initialize(file_path_,
                     base::File::Flags::FLAG_CREATE_ALWAYS |
                         base::File::Flags::FLAG_WRITE |
                         base::File::Flags::FLAG_WIN_SEQUENTIAL_SCAN);
    if (!file_.IsValid()) {
      net_error_ = HRESULTFromLastError();
      return false;
    }
  }

  if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(read_buffer_))) {
    net_error_ = HRESULTFromLastError();
    file_.Close();
    base::DeleteFile(file_path_);
    return false;
  }

  content_size_ += read_buffer_.size();
  return false;
}

void NetworkFetcher::WriteDataToFileComplete(bool is_eof) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(3) << __func__;

  fetch_progress_callback_.Run(base::saturated_cast<int64_t>(content_size_));

  if (is_eof || FAILED(net_error_)) {
    CompleteFetch();
    return;
  }

  net_error_ = ReadData();
  if (FAILED(net_error_)) {
    CompleteFetch();
  }
}

void NetworkFetcher::WriteDataToMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (read_buffer_.empty()) {
    VLOG(2) << post_response_body_;
    net_error_ = S_OK;
    CompleteFetch();
    return;
  }

  post_response_body_.append(read_buffer_.begin(), read_buffer_.end());
  content_size_ += read_buffer_.size();
  fetch_progress_callback_.Run(base::saturated_cast<int64_t>(content_size_));

  net_error_ = ReadData();
  if (FAILED(net_error_)) {
    CompleteFetch();
  }
}

void __stdcall NetworkFetcher::WinHttpStatusCallback(HINTERNET handle,
                                                     DWORD_PTR context,
                                                     DWORD status,
                                                     void* info,
                                                     DWORD info_len) {
  CHECK(handle);
  CHECK(context);

  std::string_view status_string;
  std::wstring info_string;
  switch (status) {
    case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
      status_string = "handle created";
      break;
    case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
      status_string = "handle closing";
      break;
    case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
      status_string = "resolving";
      CHECK(info);
      info_string = static_cast<const wchar_t*>(info);  // host.
      VLOG(1) << "hostname: " << info_string;
      break;
    case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
      status_string = "resolved";
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
      status_string = "connecting";
      CHECK(info);
      info_string = static_cast<const wchar_t*>(info);  // IP.
      VLOG(1) << "ip: " << info_string;
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
      status_string = "connected";
      break;
    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
      status_string = "sending";
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
      status_string = "sent";
      break;
    case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
      status_string = "receiving response";
      break;
    case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
      status_string = "response received";
      break;
    case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
      status_string = "connection closing";
      break;
    case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
      status_string = "connection closed";
      break;
    case WINHTTP_CALLBACK_STATUS_REDIRECT:
      // |info| may contain invalid URL data and not safe to reference always.
      status_string = "redirect";
      break;
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
      status_string = "data available";
      CHECK(info);
      CHECK_EQ(info_len, sizeof(uint32_t));
      info_string = base::NumberToWString(*static_cast<uint32_t*>(info));
      break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      status_string = "headers available";
      break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
      status_string = "read complete";
      info_string = base::NumberToWString(info_len);
      break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
      status_string = "send request complete";
      break;
    case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
      status_string = "write complete";
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
      status_string = "request error";
      break;
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
      status_string = "https failure";
      CHECK(info);
      CHECK_EQ(info_len, sizeof(uint32_t));
      info_string = base::ASCIIToWide(
          base::StringPrintf("%#x", *static_cast<uint32_t*>(info)));
      break;
    default:
      status_string = "unknown callback";
      break;
  }

  std::string msg;
  if (!status_string.empty()) {
    base::StringAppendF(&msg, "status=%s", status_string.data());
  } else {
    base::StringAppendF(&msg, "status=%#lx", status);
  }
  if (!info_string.empty()) {
    base::StringAppendF(&msg, ", info=%s",
                        base::SysWideToUTF8(info_string).c_str());
  }
  VLOG(3) << "WinHttp status callback:" << " handle=" << handle << ", " << msg;

  NetworkFetcher* network_fetcher = reinterpret_cast<NetworkFetcher*>(context);
  base::OnceClosure callback;
  switch (status) {
    case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
      callback =
          base::BindOnce(&NetworkFetcher::HandleClosing, network_fetcher);
      break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
      callback =
          base::BindOnce(&NetworkFetcher::SendRequestComplete, network_fetcher);
      break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      callback =
          base::BindOnce(&NetworkFetcher::HeadersAvailable, network_fetcher);
      break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
      CHECK_EQ(info, &network_fetcher->read_buffer_.front());
      callback = base::BindOnce(&NetworkFetcher::ReadDataComplete,
                                network_fetcher, size_t{info_len});
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
      CHECK(info);
      callback = base::BindOnce(
          &NetworkFetcher::RequestError, network_fetcher,
          static_cast<const WINHTTP_ASYNC_RESULT*>(info)->dwError);
      break;
  }
  if (callback) {
    network_fetcher->main_task_runner_->PostTask(FROM_HERE,
                                                 std::move(callback));
  }
}

}  // namespace winhttp
