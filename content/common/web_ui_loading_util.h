// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEB_UI_LOADING_UTIL_H_
#define CONTENT_COMMON_WEB_UI_LOADING_UTIL_H_

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace webui {

enum class GetRequestedRangeError {
  kNoRanges,
  kMultipleRanges,
  kParseFailed,
};

CONTENT_EXPORT void CallOnError(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    int error_code);

// Get the requested byte range in the request headers if present. For
// simplicity, only single byte ranges are considered valid input. If there are
// zero or multiple byte ranges, an error is returned. This is deemed
// sufficient for WebUI content.
CONTENT_EXPORT base::expected<net::HttpByteRange, GetRequestedRangeError>
GetRequestedRange(const net::HttpRequestHeaders& headers);

// Send the given bytes to the client. Use byte range if present. Returns true
// on success and false on failure. In case of failure, this function sends the
// appropriate error code to the client.
CONTENT_EXPORT bool SendData(
    network::mojom::URLResponseHeadPtr headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    std::optional<net::HttpByteRange> requested_range,
    scoped_refptr<base::RefCountedMemory> bytes);

// Returns a pair of (pipe, output size) that points to the data from `bytes`,
// bounded by `requested_range` if set.
CONTENT_EXPORT std::pair<mojo::ScopedDataPipeConsumerHandle, size_t> GetPipe(
    scoped_refptr<base::RefCountedMemory> bytes,
    std::optional<net::HttpByteRange> requested_range);

}  // namespace webui

}  // namespace content

#endif  // CONTENT_COMMON_WEB_UI_LOADING_UTIL_H_
