// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/test_response_generator.h"

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_string.h"

using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLResponse;

namespace media {

TestResponseGenerator::TestResponseGenerator(const GURL& gurl,
                                             int64_t content_length)
    : gurl_(gurl), content_length_(content_length) {}

WebURLError TestResponseGenerator::GenerateError() {
  return WebURLError(net::ERR_ABORTED, WebURL());
}

WebURLResponse TestResponseGenerator::Generate200() {
  WebURLResponse response(gurl_);
  response.SetHttpStatusCode(200);

  response.SetHttpHeaderField(
      WebString::FromUTF8("Content-Length"),
      WebString::FromUTF8(base::NumberToString(content_length_)));
  response.SetExpectedContentLength(content_length_);
  return response;
}

WebURLResponse TestResponseGenerator::Generate206(int64_t first_byte_offset) {
  return GeneratePartial206(first_byte_offset, content_length_ - 1, kNormal);
}

WebURLResponse TestResponseGenerator::Generate206(int64_t first_byte_offset,
                                                  Flags flags) {
  return GeneratePartial206(first_byte_offset, content_length_ - 1, flags);
}

WebURLResponse TestResponseGenerator::GeneratePartial206(
    int64_t first_byte_offset,
    int64_t last_byte_offset) {
  return GeneratePartial206(first_byte_offset, last_byte_offset, kNormal);
}

WebURLResponse TestResponseGenerator::GeneratePartial206(
    int64_t first_byte_offset,
    int64_t last_byte_offset,
    Flags flags) {
  int64_t range_content_length = content_length_ - first_byte_offset;

  WebURLResponse response(gurl_);
  response.SetHttpStatusCode(206);

  if ((flags & kNoAcceptRanges) == 0) {
    response.SetHttpHeaderField(WebString::FromUTF8("Accept-Ranges"),
                                WebString::FromUTF8("bytes"));
  }

  if ((flags & kNoContentRange) == 0) {
    std::string content_range = base::StringPrintf(
        "bytes %" PRId64 "-%" PRId64 "/",
        first_byte_offset, last_byte_offset);
    if (flags & kNoContentRangeInstanceSize)
      content_range += "*";
    else
      content_range += base::StringPrintf("%" PRId64, content_length_);
    response.SetHttpHeaderField(WebString::FromUTF8("Content-Range"),
                                WebString::FromUTF8(content_range));
  }

  if ((flags & kNoContentLength) == 0) {
    response.SetHttpHeaderField(
        WebString::FromUTF8("Content-Length"),
        WebString::FromUTF8(base::NumberToString(range_content_length)));
    response.SetExpectedContentLength(range_content_length);
  }
  return response;
}

WebURLResponse TestResponseGenerator::GenerateResponse(int code) {
  WebURLResponse response(gurl_);
  response.SetHttpStatusCode(code);
  return response;
}

WebURLResponse TestResponseGenerator::Generate404() {
  return GenerateResponse(404);
}

WebURLResponse TestResponseGenerator::GenerateFileResponse(
    int64_t first_byte_offset) {
  WebURLResponse response(gurl_);
  response.SetHttpStatusCode(0);

  if (first_byte_offset >= 0) {
    response.SetExpectedContentLength(content_length_ - first_byte_offset);
  } else {
    response.SetExpectedContentLength(-1);
  }
  return response;
}

}  // namespace media
