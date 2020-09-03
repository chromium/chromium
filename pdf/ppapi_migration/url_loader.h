// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_URL_LOADER_H_
#define PDF_PPAPI_MIGRATION_URL_LOADER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"

namespace blink {
class WebAssociatedURLLoader;
struct WebAssociatedURLLoaderOptions;
}  // namespace blink

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

// Abstraction for a Blink or Pepper URL loader.
class UrlLoader : public base::RefCounted<UrlLoader> {
 public:
  UrlLoader(const UrlLoader&) = delete;
  UrlLoader& operator=(const UrlLoader&) = delete;

  // Tries to grant the loader the capability to make unrestricted cross-origin
  // requests ("universal access," in `blink::SecurityOrigin` terms). Must be
  // called before `Open()`.
  virtual void GrantUniversalAccess() = 0;

  // Mimic `pp::URLLoader`:
  virtual void Open(const UrlRequest& request, ResultCallback callback) = 0;
  virtual bool GetDownloadProgress(
      int64_t& bytes_received,
      int64_t& total_bytes_to_be_received) const = 0;
  virtual void ReadResponseBody(base::span<char> buffer,
                                ResultCallback callback) = 0;
  virtual void Close() = 0;

  // Returns the URL response (not including the body). Only valid after
  // `Open()` completes.
  const UrlResponse& response() const { return response_; }

 protected:
  UrlLoader();
  virtual ~UrlLoader();

  UrlResponse& mutable_response() { return response_; }

 private:
  friend class base::RefCounted<UrlLoader>;

  UrlResponse response_;
};

// A Blink URL loader. This implementation tries to emulate a combination of
// `content::PepperURLLoaderHost` and `ppapi::proxy::URLLoaderResource`.
class BlinkUrlLoader final : public UrlLoader,
                             public blink::WebAssociatedURLLoaderClient {
 public:
  // Client interface required by `BlinkUrlLoader`. Instances should be passed
  // using weak pointers, as the loader can be shared, and may outlive the
  // client.
  class Client {
   public:
    // Returns a new `blink::WebAssociatedURLLoader` from the current local
    // frame. May return `nullptr` if the local frame no longer exists.
    virtual std::unique_ptr<blink::WebAssociatedURLLoader>
    CreateAssociatedURLLoader(
        const blink::WebAssociatedURLLoaderOptions& options) = 0;

   protected:
    ~Client() = default;
  };

  explicit BlinkUrlLoader(base::WeakPtr<Client> client);
  BlinkUrlLoader(const BlinkUrlLoader&) = delete;
  BlinkUrlLoader& operator=(const BlinkUrlLoader&) = delete;

  // UrlLoader:
  void GrantUniversalAccess() override;
  void Open(const UrlRequest& request, ResultCallback callback) override;
  bool GetDownloadProgress(int64_t& bytes_received,
                           int64_t& total_bytes_to_be_received) const override;
  void ReadResponseBody(base::span<char> buffer,
                        ResultCallback callback) override;
  void Close() override;

  // blink::WebAssociatedURLLoaderClient:
  bool WillFollowRedirect(
      const blink::WebURL& new_url,
      const blink::WebURLResponse& redirect_response) override;
  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidDownloadData(uint64_t data_length) override;
  void DidReceiveData(const char* data, int data_length) override;
  void DidReceiveCachedMetadata(const char* data, int data_length) override;
  void DidFinishLoading() override;
  void DidFail(const blink::WebURLError& error) override;

 private:
  // Private because the class is RefCounted.
  ~BlinkUrlLoader() override;

  base::WeakPtr<Client> client_;
  bool grant_universal_access_ = false;

  std::unique_ptr<blink::WebAssociatedURLLoader> blink_loader_;
};

// A Pepper URL loader.
class PepperUrlLoader final : public UrlLoader {
 public:
  explicit PepperUrlLoader(pp::InstanceHandle plugin_instance);
  PepperUrlLoader(const PepperUrlLoader&) = delete;
  PepperUrlLoader& operator=(const PepperUrlLoader&) = delete;

  // UrlLoader:
  void GrantUniversalAccess() override;
  void Open(const UrlRequest& request, ResultCallback callback) override;
  bool GetDownloadProgress(int64_t& bytes_received,
                           int64_t& total_bytes_to_be_received) const override;
  void ReadResponseBody(base::span<char> buffer,
                        ResultCallback callback) override;
  void Close() override;

 private:
  // Private because the class is RefCounted.
  ~PepperUrlLoader() override;

  void DidOpen(ResultCallback callback, int32_t result);

  pp::InstanceHandle plugin_instance_;
  pp::URLLoader pepper_loader_;

  base::WeakPtrFactory<PepperUrlLoader> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_URL_LOADER_H_
