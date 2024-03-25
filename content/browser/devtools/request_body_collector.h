// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_REQUEST_BODY_COLLECTOR_H_
#define CONTENT_BROWSER_DEVTOOLS_REQUEST_BODY_COLLECTOR_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {
class ResourceRequestBody;
}

namespace content {

// Exported for unit tests.
class CONTENT_EXPORT RequestBodyCollector {
 public:
  using BodyEntry = base::expected<std::vector<uint8_t>, std::string>;
  using CompletionCallback = base::OnceCallback<void(std::vector<BodyEntry>)>;

  // This may either complete synchronously, if all bodies are bytes that are
  // immediately available, or asynchronously, if reading remote objects is
  // required. In the former case, it invokes `callback` right away and returns
  // null, in the latter case, the callback is deferred and an instance is
  // returned, that needs to be retained by the caller for as long as the
  // callback is desired.
  static std::unique_ptr<RequestBodyCollector> Collect(
      const network::ResourceRequestBody& request_body,
      CompletionCallback callback);

  ~RequestBodyCollector();

  RequestBodyCollector(const RequestBodyCollector&) = delete;
  RequestBodyCollector& operator=(const RequestBodyCollector&) = delete;

 private:
  class BodyReader;
  using ReadersMap = base::
      flat_map<std::unique_ptr<BodyReader>, size_t, base::UniquePtrComparator>;

  RequestBodyCollector();

  void OnReaderComplete(BodyReader* reader, BodyEntry entry);

  std::vector<BodyEntry> bodies_;
  ReadersMap readers_;
  CompletionCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_REQUEST_BODY_COLLECTOR_H_
