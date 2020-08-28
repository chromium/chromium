// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_URL_LOADER_H_
#define PDF_PPAPI_MIGRATION_URL_LOADER_H_

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/url_loader.h"

namespace chrome_pdf {

// Properties for making a URL request.
struct UrlRequest final {
  UrlRequest();
  UrlRequest(const UrlRequest& other);
  UrlRequest(UrlRequest&& other) noexcept;
  UrlRequest& operator=(const UrlRequest& other);
  UrlRequest& operator=(UrlRequest&& other) noexcept;
  ~UrlRequest();

  // Request URL.
  std::string url;

  // HTTP method.
  std::string method;

  // Whether to ignore redirects. By default, redirects are followed
  // automatically.
  bool ignore_redirects = false;

  // Custom referrer URL.
  base::Optional<std::string> custom_referrer_url;

  // HTTP headers as a single string of `\n`-delimited key-value pairs.
  base::Optional<std::string> headers;

  // Request body.
  std::string body;
};

// Properties returned from a URL request. Does not include the response body.
struct UrlResponse final {
  UrlResponse();
  UrlResponse(const UrlResponse& other);
  UrlResponse(UrlResponse&& other) noexcept;
  UrlResponse& operator=(const UrlResponse& other);
  UrlResponse& operator=(UrlResponse&& other) noexcept;
  ~UrlResponse();

  // HTTP status code.
  int32_t status_code = 0;

  // HTTP headers as a single string of `\n`-delimited key-value pairs.
  base::Optional<std::string> headers;
};

// Thin wrapper around a `pp::URLLoader`. Unlike a `pp::URLLoader`, this class
// does not perform its own reference counting, but relies on `scoped_refptr`.
//
// TODO(crbug.com/1099022): Make this abstract, and add a Blink implementation.
class UrlLoader : public base::RefCounted<UrlLoader> {
 public:
  UrlLoader();
  explicit UrlLoader(pp::InstanceHandle plugin_instance);
  UrlLoader(const UrlLoader&) = delete;
  UrlLoader& operator=(const UrlLoader&) = delete;

  // Tries to grant the loader the capability to make unrestricted cross-origin
  // requests ("universal access," in `blink::SecurityOrigin` terms).
  void GrantUniversalAccess();

  // Mimic `pp::URLLoader`:
  void Open(const UrlRequest& request, ResultCallback callback);
  bool GetDownloadProgress(int64_t& bytes_received,
                           int64_t& total_bytes_to_be_received) const;
  const UrlResponse& response() const { return response_; }
  void ReadResponseBody(base::span<char> buffer, ResultCallback callback);
  void Close();

 private:
  friend class base::RefCounted<UrlLoader>;

  ~UrlLoader();

  void DidOpen(ResultCallback callback, int32_t result);

  pp::InstanceHandle plugin_instance_;
  pp::URLLoader pepper_loader_;
  UrlResponse response_;

  base::WeakPtrFactory<UrlLoader> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_URL_LOADER_H_
