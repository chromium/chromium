// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/url_loader_wrapper_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_util.h"
#include "pdf/loader/result_codes.h"
#include "pdf/loader/url_loader.h"
#include "ui/gfx/range/range.h"

namespace chrome_pdf {

namespace {

// We should read with delay to prevent block UI thread, and reduce CPU usage.
constexpr base::TimeDelta kReadDelayMs = base::Milliseconds(2);

constexpr std::string_view kBytes = "bytes";

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

std::optional<gfx::Range> GetByteRangeFromStr(
    std::string_view content_range_str) {
  if (!base::StartsWith(content_range_str, kBytes,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return std::nullopt;
  }

  std::string range(content_range_str.substr(kBytes.size()));
  std::string::size_type pos = range.find('-');
  std::string range_end;
  if (pos != std::string::npos) {
    range_end = range.substr(pos + 1);
  }
  base::TrimWhitespaceASCII(range, base::TRIM_LEADING, &range);
  base::TrimWhitespaceASCII(range_end, base::TRIM_LEADING, &range_end);
  return gfx::Range(atoi(range.c_str()), atoi(range_end.c_str()));
}

// If the headers have a byte-range response, and at least the start position
// was parsed, returns a range with the start and end positions.
// The end position will be set to 0 if it was not found or parsed from the
// response.
// Returns std::nullopt if not even a start position could be parsed.
std::optional<gfx::Range> GetByteRangeFromHeaders(std::string_view headers) {
  net::HttpUtil::HeadersIterator it(headers, "\n");
  while (it.GetNext()) {
    if (base::EqualsCaseInsensitiveASCII(it.name_piece(), "content-range")) {
      std::optional<gfx::Range> range = GetByteRangeFromStr(it.values());
      if (range.has_value()) {
        return range;
      }
    }
  }
  return std::nullopt;
}

bool IsDoubleEndLineAtEnd(std::string_view buffer) {
  return buffer.ends_with("\n\n") || buffer.ends_with("\r\n\r\n");
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
                                     base::OnceCallback<void(bool)> callback) {
  url_loader_->Open(
      MakeRangeRequest(url, referrer_url, position, size),
      base::BindOnce(&URLLoaderWrapperImpl::DidOpen, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void URLLoaderWrapperImpl::ReadResponseBody(
    base::span<uint8_t> buffer,
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

  net::HttpUtil::HeadersIterator it(response_headers, "\n");
  while (it.GetNext()) {
    std::string_view name = it.name_piece();
    if (base::EqualsCaseInsensitiveASCII(name, "content-length")) {
      content_length_ = atoi(it.values().c_str());
    } else if (base::EqualsCaseInsensitiveASCII(name, "accept-ranges")) {
      accept_ranges_bytes_ =
          base::EqualsCaseInsensitiveASCII(it.values(), kBytes);
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
        static constexpr std::string_view kBoundary = "boundary=";
        size_t boundary = type.find(kBoundary);
        if (boundary != std::string::npos) {
          multipart_boundary_ = type.substr(boundary + kBoundary.size());
          is_multipart_ = !multipart_boundary_.empty();
        }
      }
    } else if (base::EqualsCaseInsensitiveASCII(name, "content-disposition")) {
      content_disposition_ = it.values();
    } else if (base::EqualsCaseInsensitiveASCII(name, "content-range")) {
      std::optional<gfx::Range> range = GetByteRangeFromStr(it.values());
      if (range.has_value()) {
        byte_range_ = range.value();
      }
    }
  }
}

void URLLoaderWrapperImpl::DidOpen(base::OnceCallback<void(bool)> callback,
                                   Result result) {
  SetHeadersFromLoader();
  std::move(callback).Run(result == Result::kSuccess);
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

  base::span<uint8_t> start = buffer_.first(static_cast<size_t>(result));
  multi_part_processed_ = true;
  for (size_t i = 2; i < static_cast<size_t>(result); ++i) {
    auto buffer = base::as_string_view(buffer_.first(i));
    if (!IsDoubleEndLineAtEnd(buffer)) {
      continue;
    }

    std::optional<gfx::Range> range = GetByteRangeFromHeaders(buffer);
    if (range.has_value()) {
      byte_range_ = range.value();
      start = start.subspan(i);
    }
    break;
  }

  if (start.empty()) {
    // Continue receiving.
    return ReadResponseBodyImpl(std::move(callback));
  }

  base::span(buffer_).copy_prefix_from(start);
  std::move(callback).Run(base::checked_cast<int32_t>(start.size()));
}

void URLLoaderWrapperImpl::SetHeadersFromLoader() {
  ParseHeaders(url_loader_->response().headers);
}

}  // namespace chrome_pdf
