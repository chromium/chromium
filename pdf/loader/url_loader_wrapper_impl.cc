// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(UNSAFE_BUFFERS_BUILD)
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "pdf/loader/url_loader_wrapper_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_util.h"
#include "pdf/loader/url_loader.h"
#include "ui/gfx/range/range.h"

namespace chrome_pdf {

namespace {

// We should read with delay to prevent block UI thread, and reduce CPU usage.
constexpr base::TimeDelta kReadDelayMs = base::Milliseconds(2);

UrlRequest MakeRangeRequest(const std::string& url,
                            const std::string& referrer_url,
                            uint32_t position,
                            uint32_t size) {
  UrlRequest request;
  request.url = url;
  request.method = "GET";
  request.ignore_redirects = true;
  request.custom_referrer_url = referrer_url;

  // According to rfc2616, byte range specifies position of the first and last
  // bytes in the requested range inclusively. Therefore we should subtract 1
  // from the position + size, to get index of the last byte that needs to be
  // downloaded.
  request.headers =
      base::StringPrintf("Range: bytes=%d-%d", position, position + size - 1);

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
    if (base::EqualsCaseInsensitiveASCII(it.name_piece(), "content-range")) {
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

URLLoaderWrapperImpl::URLLoaderWrapperImpl(
    std::unique_ptr<UrlLoader> url_loader)
    : url_loader_(std::move(url_loader)) {
  SetHeadersFromLoader();
}

URLLoaderWrapperImpl::~URLLoaderWrapperImpl() {
  Close();
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
  return url_loader_->response().status_code;
}

bool URLLoaderWrapperImpl::IsMultipart() const {
  return is_multipart_;
}

bool URLLoaderWrapperImpl::GetByteRangeStart(int* start) const {
  DCHECK(start);
  *start = byte_range_.start();
  return byte_range_.IsValid();
}

void URLLoaderWrapperImpl::Close() {
  url_loader_->Close();
  read_starter_.Stop();
}

void URLLoaderWrapperImpl::OpenRange(const std::string& url,
                                     const std::string& referrer_url,
                                     uint32_t position,
                                     uint32_t size,
                                     base::OnceCallback<void(int)> callback) {
  url_loader_->Open(
      MakeRangeRequest(url, referrer_url, position, size),
      base::BindOnce(&URLLoaderWrapperImpl::DidOpen, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void URLLoaderWrapperImpl::ReadResponseBody(
    base::span<char> buffer,
    base::OnceCallback<void(int)> callback) {
  buffer_ = buffer;
  read_starter_.Start(
      FROM_HERE, kReadDelayMs,
      base::BindOnce(&URLLoaderWrapperImpl::ReadResponseBodyImpl,
                     base::Unretained(this), std::move(callback)));
}

void URLLoaderWrapperImpl::ReadResponseBodyImpl(
    base::OnceCallback<void(int)> callback) {
  url_loader_->ReadResponseBody(
      buffer_, base::BindOnce(&URLLoaderWrapperImpl::DidRead,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void URLLoaderWrapperImpl::ParseHeaders(const std::string& response_headers) {
  content_length_ = -1;
  accept_ranges_bytes_ = false;
  content_encoded_ = false;
  content_type_.clear();
  content_disposition_.clear();
  multipart_boundary_.clear();
  byte_range_ = gfx::Range::InvalidRange();
  is_multipart_ = false;

  if (response_headers.empty())
    return;

  net::HttpUtil::HeadersIterator it(response_headers.begin(),
                                    response_headers.end(), "\n");
  while (it.GetNext()) {
    std::string_view name = it.name_piece();
    if (base::EqualsCaseInsensitiveASCII(name, "content-length")) {
      content_length_ = atoi(it.values().c_str());
    } else if (base::EqualsCaseInsensitiveASCII(name, "accept-ranges")) {
      accept_ranges_bytes_ =
          base::EqualsCaseInsensitiveASCII(it.values(), "bytes");
    } else if (base::EqualsCaseInsensitiveASCII(name, "content-encoding")) {
      content_encoded_ = true;
    } else if (base::EqualsCaseInsensitiveASCII(name, "content-type")) {
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
    } else if (base::EqualsCaseInsensitiveASCII(name, "content-disposition")) {
      content_disposition_ = it.values();
    } else if (base::EqualsCaseInsensitiveASCII(name, "content-range")) {
      int start = 0;
      int end = 0;
      if (GetByteRangeFromStr(it.values().c_str(), &start, &end)) {
        byte_range_ = gfx::Range(start, end);
      }
    }
  }
}

void URLLoaderWrapperImpl::DidOpen(base::OnceCallback<void(int)> callback,
                                   int32_t result) {
  SetHeadersFromLoader();
  std::move(callback).Run(result);
}

void URLLoaderWrapperImpl::DidRead(base::OnceCallback<void(int)> callback,
                                   int32_t result) {
  if (multi_part_processed_) {
    // Reset this flag so we look inside the buffer in calls of DidRead for this
    // response only once.  Note that this code DOES NOT handle multi part
    // responses with more than one part (we don't issue them at the moment, so
    // they shouldn't arrive).
    is_multipart_ = false;
  }
  if (result <= 0 || !is_multipart_) {
    std::move(callback).Run(result);
    return;
  }
  if (result <= 2) {
    // TODO(art-snake): Accumulate data for parse headers.
    std::move(callback).Run(result);
    return;
  }

  char* start = buffer_.data();
  size_t length = result;
  multi_part_processed_ = true;
  for (int i = 2; i < result; ++i) {
    if (IsDoubleEndLineAtEnd(buffer_.data(), i)) {
      int start_pos = 0;
      int end_pos = 0;
      if (GetByteRangeFromHeaders(std::string(buffer_.data(), i), &start_pos,
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
    return ReadResponseBodyImpl(std::move(callback));
  }
  DCHECK_GT(result, 0);
  memmove(buffer_.data(), start, result);

  std::move(callback).Run(result);
}

void URLLoaderWrapperImpl::SetHeadersFromLoader() {
  ParseHeaders(url_loader_->response().headers);
}

}  // namespace chrome_pdf
