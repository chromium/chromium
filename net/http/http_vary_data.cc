// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_vary_data.h"

#include <stdlib.h>

#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

HttpVaryData::HttpVaryData() = default;

bool HttpVaryData::Init(const HttpRequestInfo& request_info,
                        const HttpResponseHeaders& response_headers) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);

  is_valid_ = false;
  bool processed_header = false;

  // Feed the MD5 context in the order of the Vary header enumeration.  If the
  // Vary header repeats a header name, then that's OK.
  //
  // If the Vary header contains '*' then we can just notice it based on
  // |cached_response_headers| in MatchesRequest(), and don't have to worry
  // about the specific headers.  We still want an HttpVaryData around, to let
  // us handle this case. See section 4.1 of RFC 7234.
  //
  size_t iter = 0;
  std::string name = "vary", request_header;
  while (response_headers.EnumerateHeader(&iter, name, &request_header)) {
    if (request_header == "*") {
      // What's in request_digest_ will never be looked at, but make it
      // deterministic so we don't serialize out uninitialized memory content.
      memset(&request_digest_, 0, sizeof(request_digest_));
      return is_valid_ = true;
    }
    AddField(request_info, request_header, &ctx);
    processed_header = true;
  }

  if (!processed_header)
    return false;

  base::MD5Final(&request_digest_, &ctx);
  return is_valid_ = true;
}

bool HttpVaryData::InitFromPickle(base::PickleIterator* iter) {
  is_valid_ = false;
  const char* data;
  if (iter->ReadBytes(&data, sizeof(request_digest_))) {
    memcpy(&request_digest_, data, sizeof(request_digest_));
    return is_valid_ = true;
  }
  return false;
}

void HttpVaryData::Persist(base::Pickle* pickle) const {
  DCHECK(is_valid());
  pickle->WriteBytes(&request_digest_, sizeof(request_digest_));
}

bool HttpVaryData::MatchesRequest(
    const HttpRequestInfo& request_info,
    const HttpResponseHeaders& cached_response_headers) const {
  // Vary: * never matches.
  if (cached_response_headers.HasHeaderValue("vary", "*"))
    return false;

  HttpVaryData new_vary_data;
  if (!new_vary_data.Init(request_info, cached_response_headers)) {
    // This case can happen if |this| was loaded from a cache that was populated
    // by a build before crbug.com/469675 was fixed.
    return false;
  }
  return memcmp(&new_vary_data.request_digest_, &request_digest_,
                sizeof(request_digest_)) == 0;
}

// static
std::string HttpVaryData::GetRequestValue(
    const HttpRequestInfo& request_info,
    const std::string& request_header) {
  // Unfortunately, we do not have access to all of the request headers at this
  // point.  Most notably, we do not have access to an Authorization header if
  // one will be added to the request.

  std::string result;
  if (request_info.extra_headers.GetHeader(request_header, &result))
    return result;

  return std::string();
}

// static
void HttpVaryData::AddField(const HttpRequestInfo& request_info,
                            const std::string& request_header,
                            base::MD5Context* ctx) {
  std::string request_value = GetRequestValue(request_info, request_header);

  // Append a character that cannot appear in the request header line so that we
  // protect against case where the concatenation of two request headers could
  // look the same for a variety of values for the individual request headers.
  // For example, "foo: 12\nbar: 3" looks like "foo: 1\nbar: 23" otherwise.
  request_value.append(1, '\n');

  base::MD5Update(ctx, request_value);
}

}  // namespace net
