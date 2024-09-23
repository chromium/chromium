// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_LOADER_URL_LOADER_H_
#define PDF_LOADER_URL_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"

namespace blink {
class WebAssociatedURLLoader;
class WebString;
class WebURL;
class WebURLRequest;
struct WebAssociatedURLLoaderOptions;
}  // namespace blink

namespace net {
class SiteForCookies;
}  // namespace net

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
  std::string custom_referrer_url;

  // HTTP headers as a single string of `\n`-delimited key-value pairs.
  std::string headers;

  // Request body.
  std::string body;

  // Thresholds for throttling filling of the loader's internal buffer. Filling
  // will stop after exceeding the upper threshold, and resume after dropping
  // below the lower threshold.
  //
  // Default values taken from `ppapi/shared_impl/url_request_info_data.cc`. The
  // PDF viewer never changes the defaults in production, so these fields mostly
  // exist for testing purposes.
  size_t buffer_lower_threshold = 50 * 1000 * 1000;
  size_t buffer_upper_threshold = 100 * 1000 * 1000;
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
  std::string headers;
};

// A Blink URL loader. This implementation tries to emulate a combination of
// `content::PepperURLLoaderHost` and `ppapi::proxy::URLLoaderResource`.
class UrlLoader final : public blink::WebAssociatedURLLoaderClient {
 public:
  // Client interface required by `UrlLoader`. Instances should be passed using
  // weak pointers, as the loader can be shared, and may outlive the client.
  class Client {
   public:
    // Returns `true` if the client is still usable. The client may require
    // resources that can become unavailable, such as a local frame. Rather than
    // handling missing resources separately for each method, callers can just
    // verify validity once, before making any other calls.
    virtual bool IsValid() const = 0;

    // Completes `partial_url` using the current document.
    virtual blink::WebURL CompleteURL(
        const blink::WebString& partial_url) const = 0;

    // Gets the site-for-cookies for the current document.
    virtual net::SiteForCookies SiteForCookies() const = 0;

    // Sets the referrer on `request` to `referrer_url` using the current frame.
    virtual void SetReferrerForRequest(blink::WebURLRequest& request,
                                       const blink::WebURL& referrer_url) = 0;

    // Returns a new `blink::WebAssociatedURLLoader` from the current frame.
    virtual std::unique_ptr<blink::WebAssociatedURLLoader>
    CreateAssociatedURLLoader(
        const blink::WebAssociatedURLLoaderOptions& options) = 0;

   protected:
    ~Client() = default;
  };

  explicit UrlLoader(base::WeakPtr<Client> client);
  UrlLoader(const UrlLoader&) = delete;
  UrlLoader& operator=(const UrlLoader&) = delete;
  ~UrlLoader() override;

  // Mimic `pp::URLLoader`:
  void Open(const UrlRequest& request, base::OnceCallback<void(int)> callback);
  void ReadResponseBody(base::span<char> buffer,
                        base::OnceCallback<void(int)> callback);
  void Close();

  // Returns the URL response (not including the body). Only valid after
  // `Open()` completes.
  const UrlResponse& response() const { return response_; }

  // blink::WebAssociatedURLLoaderClient:
  bool WillFollowRedirect(
      const blink::WebURL& new_url,
      const blink::WebURLResponse& redirect_response) override;
  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidDownloadData(uint64_t data_length) override;
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFail(const blink::WebURLError& error) override;

 private:
  enum class LoadingState {
    // Before calling `Open()`.
    kWaitingToOpen,

    // After calling `Open()`, but before `DidReceiveResponse()` or `DidFail()`.
    kOpening,

    // After `DidReceiveResponse()`, but before `DidFinishLoading()` or
    // `DidFail()`. Zero or more calls allowed to `DidReceiveData()`.
    kStreamingData,

    // After `DidFinishLoading()` or `DidFail()`, or forced by `Close()`.
    // Details about how the load completed are in `complete_result_`.
    kLoadComplete,
  };

  // Aborts the load with `result`. Runs callback if pending.
  void AbortLoad(int32_t result);

  // Runs callback for `ReadResponseBody()` if pending.
  void RunReadCallback();

  void SetLoadComplete(int32_t result);

  base::WeakPtr<Client> client_;

  LoadingState state_ = LoadingState::kWaitingToOpen;
  int32_t complete_result_ = 0;

  std::unique_ptr<blink::WebAssociatedURLLoader> blink_loader_;

  bool ignore_redirects_ = false;
  base::OnceCallback<void(int)> open_callback_;

  UrlResponse response_;

  // Thresholds control buffer throttling, as defined in `UrlRequest`.
  size_t buffer_lower_threshold_ = 0;
  size_t buffer_upper_threshold_ = 0;
  bool deferring_loading_ = false;
  base::circular_deque<char> buffer_;

  base::OnceCallback<void(int)> read_callback_;
  base::raw_span<char> client_buffer_;
};

}  // namespace chrome_pdf

#endif  // PDF_LOADER_URL_LOADER_H_
