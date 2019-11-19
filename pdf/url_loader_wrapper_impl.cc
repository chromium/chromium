// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/url_loader_wrapper_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace chrome_pdf {

namespace {

// We should read with delay to prevent block UI thread, and reduce CPU usage.
constexpr base::TimeDelta kReadDelayMs = base::TimeDelta::FromMilliseconds(2);

pp::URLRequestInfo MakeRangeRequest(pp::Instance* plugin_instance,
                                    const std::string& url,
                                    const std::string& referrer_url,
                                    uint32_t position,
                                    uint32_t size) {
  pp::URLRequestInfo request(plugin_instance);
  request.SetURL(url);
  request.SetMethod("GET");
  request.SetFollowRedirects(false);
  request.SetCustomReferrerURL(referrer_url);

  // According to rfc2616, byte range specifies position of the first and last
  // bytes in the requested range inclusively. Therefore we should subtract 1
  // from the position + size, to get index of the last byte that needs to be
  // downloaded.
  std::string str_header =
      base::StringPrintf("Range: bytes=%d-%d", position, position + size - 1);
  pp::Var header(str_header.c_str());
  request.SetHeaders(header);

  return request;
}

bool GetByteRangeFromStr(const std::string& content_range_str,
                         int* start,
                         int* end) {
  std::string range = content_range_str;
  if (!base::StartsWith(range, "bytes", base::CompareCase::INSENSITIVE_ASCII))
    return false;

  range = range.substr(strlen("bytes"));
  std::string::size_type pos = range.find('-');
  std::string range_end;
  if (pos != std::string::npos)
    range_end = range.substr(pos + 1);
  base::TrimWhitespaceASCII(range, base::TRIM_LEADING, &range);
  base::TrimWhitespaceASCII(range_end, base::TRIM_LEADING, &range_end);
  *start = atoi(range.c_str());
  *end = atoi(range_end.c_str());
  return true;
}

// If the headers have a byte-range response, writes the start and end
// positions and returns true if at least the start position was parsed.
// The end position will be set to 0 if it was not found or parsed from the
// response.
// Returns false if not even a start position could be parsed.
bool GetByteRangeFromHeaders(const std::string& headers, int* start, int* end) {
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n");
  while (it.GetNext()) {
    if (base::LowerCaseEqualsASCII(it.name_piece(), "content-range")) {
      if (GetByteRangeFromStr(it.values().c_str(), start, end))
        return true;
    }
  }
  return false;
}

bool IsDoubleEndLineAtEnd(const char* buffer, int size) {
  if (size < 2)
    return false;

  if (buffer[size - 1] == '\n' && buffer[size - 2] == '\n')
    return true;

  if (size < 4)
    return false;

  return buffer[size - 1] == '\n' && buffer[size - 2] == '\r' &&
         buffer[size - 3] == '\n' && buffer[size - 4] == '\r';
}

}  // namespace

URLLoaderWrapperImpl::URLLoaderWrapperImpl(pp::Instance* plugin_instance,
                                           const pp::URLLoader& url_loader)
    : plugin_instance_(plugin_instance),
      url_loader_(url_loader),
      callback_factory_(this) {
  SetHeadersFromLoader();
}

URLLoaderWrapperImpl::~URLLoaderWrapperImpl() {
  Close();
  // We should call callbacks to prevent memory leaks.
  // The callbacks don't do anything, because the objects that created the
  // callbacks have been destroyed.
  if (!did_open_callback_.IsOptional())
    did_open_callback_.RunAndClear(-1);
  if (!did_read_callback_.IsOptional())
    did_read_callback_.RunAndClear(-1);
}

int URLLoaderWrapperImpl::GetContentLength() const {
  return content_length_;
}

bool URLLoaderWrapperImpl::IsAcceptRangesBytes() const {
  return accept_ranges_bytes_;
}

bool URLLoaderWrapperImpl::IsContentEncoded() const {
  return content_encoded_;
}

std::string URLLoaderWrapperImpl::GetContentType() const {
  return content_type_;
}
std::string URLLoaderWrapperImpl::GetContentDisposition() const {
  return content_disposition_;
}

int URLLoaderWrapperImpl::GetStatusCode() const {
  return url_loader_.GetResponseInfo().GetStatusCode();
}

bool URLLoaderWrapperImpl::IsMultipart() const {
  return is_multipart_;
}

bool URLLoaderWrapperImpl::GetByteRangeStart(int* start) const {
  DCHECK(start);
  *start = byte_range_.start();
  return byte_range_.IsValid();
}

bool URLLoaderWrapperImpl::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) const {
  return url_loader_.GetDownloadProgress(bytes_received,
                                         total_bytes_to_be_received);
}

void URLLoaderWrapperImpl::Close() {
  url_loader_.Close();
  read_starter_.Stop();
}

void URLLoaderWrapperImpl::OpenRange(const std::string& url,
                                     const std::string& referrer_url,
                                     uint32_t position,
                                     uint32_t size,
                                     const pp::CompletionCallback& cc) {
  did_open_callback_ = cc;
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&URLLoaderWrapperImpl::DidOpen);
  int rv = url_loader_.Open(
      MakeRangeRequest(plugin_instance_, url, referrer_url, position, size),
      callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    callback.Run(rv);
}

void URLLoaderWrapperImpl::ReadResponseBody(char* buffer,
                                            int buffer_size,
                                            const pp::CompletionCallback& cc) {
  did_read_callback_ = cc;
  buffer_ = buffer;
  buffer_size_ = buffer_size;
  read_starter_.Start(
      FROM_HERE, kReadDelayMs,
      base::BindRepeating(&URLLoaderWrapperImpl::ReadResponseBodyImpl,
                          base::Unretained(this)));
}

void URLLoaderWrapperImpl::ReadResponseBodyImpl() {
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&URLLoaderWrapperImpl::DidRead);
  int rv = url_loader_.ReadResponseBody(buffer_, buffer_size_, callback);
  if (rv != PP_OK_COMPLETIONPENDING) {
    callback.Run(rv);
  }
}

void URLLoaderWrapperImpl::SetResponseHeaders(
    const std::string& response_headers) {
  response_headers_ = response_headers;
  ParseHeaders();
}

void URLLoaderWrapperImpl::ParseHeaders() {
  content_length_ = -1;
  accept_ranges_bytes_ = false;
  content_encoded_ = false;
  content_type_.clear();
  content_disposition_.clear();
  multipart_boundary_.clear();
  byte_range_ = gfx::Range::InvalidRange();
  is_multipart_ = false;

  if (response_headers_.empty())
    return;

  net::HttpUtil::HeadersIterator it(response_headers_.begin(),
                                    response_headers_.end(), "\n");
  while (it.GetNext()) {
    base::StringPiece name = it.name_piece();
    if (base::LowerCaseEqualsASCII(name, "content-length")) {
      content_length_ = atoi(it.values().c_str());
    } else if (base::LowerCaseEqualsASCII(name, "accept-ranges")) {
      accept_ranges_bytes_ = base::LowerCaseEqualsASCII(it.values(), "bytes");
    } else if (base::LowerCaseEqualsASCII(name, "content-encoding")) {
      content_encoded_ = true;
    } else if (base::LowerCaseEqualsASCII(name, "content-type")) {
      content_type_ = it.values();
      size_t semi_colon_pos = content_type_.find(';');
      if (semi_colon_pos != std::string::npos) {
        content_type_ = content_type_.substr(0, semi_colon_pos);
      }
      base::TrimWhitespaceASCII(content_type_, base::TRIM_ALL, &content_type_);
      // multipart boundary.
      std::string type = base::ToLowerASCII(it.values_piece());
      if (base::StartsWith(type, "multipart/", base::CompareCase::SENSITIVE)) {
        const char* boundary = strstr(type.c_str(), "boundary=");
        DCHECK(boundary);
        if (boundary) {
          multipart_boundary_ = std::string(boundary + 9);
          is_multipart_ = !multipart_boundary_.empty();
        }
      }
    } else if (base::LowerCaseEqualsASCII(name, "content-disposition")) {
      content_disposition_ = it.values();
    } else if (base::LowerCaseEqualsASCII(name, "content-range")) {
      int start = 0;
      int end = 0;
      if (GetByteRangeFromStr(it.values().c_str(), &start, &end)) {
        byte_range_ = gfx::Range(start, end);
      }
    }
  }
}

void URLLoaderWrapperImpl::DidOpen(int32_t result) {
  SetHeadersFromLoader();
  did_open_callback_.RunAndClear(result);
}

void URLLoaderWrapperImpl::DidRead(int32_t result) {
  if (multi_part_processed_) {
    // Reset this flag so we look inside the buffer in calls of DidRead for this
    // response only once.  Note that this code DOES NOT handle multi part
    // responses with more than one part (we don't issue them at the moment, so
    // they shouldn't arrive).
    is_multipart_ = false;
  }
  if (result <= 0 || !is_multipart_) {
    did_read_callback_.RunAndClear(result);
    return;
  }
  if (result <= 2) {
    // TODO(art-snake): Accumulate data for parse headers.
    did_read_callback_.RunAndClear(result);
    return;
  }

  char* start = buffer_;
  size_t length = result;
  multi_part_processed_ = true;
  for (int i = 2; i < result; ++i) {
    if (IsDoubleEndLineAtEnd(buffer_, i)) {
      int start_pos = 0;
      int end_pos = 0;
      if (GetByteRangeFromHeaders(std::string(buffer_, i), &start_pos,
                                  &end_pos)) {
        byte_range_ = gfx::Range(start_pos, end_pos);
        start += i;
        length -= i;
      }
      break;
    }
  }
  result = length;
  if (result == 0) {
    // Continue receiving.
    return ReadResponseBodyImpl();
  }
  DCHECK_GT(result, 0);
  memmove(buffer_, start, result);

  did_read_callback_.RunAndClear(result);
}

void URLLoaderWrapperImpl::SetHeadersFromLoader() {
  pp::URLResponseInfo response = url_loader_.GetResponseInfo();
  pp::Var headers_var = response.GetHeaders();

  SetResponseHeaders(headers_var.is_string() ? headers_var.AsString() : "");
}

}  // namespace chrome_pdf
