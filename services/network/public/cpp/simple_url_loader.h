// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_H_

#include <stdint.h>

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

template <class T>
class scoped_refptr;

namespace base {
class FilePath;
class TickClock;
class TimeDelta;
}  // namespace base

namespace net {
class HttpResponseHeaders;
struct NetworkTrafficAnnotationTag;
struct RedirectInfo;
}  // namespace net

namespace network {
struct ResourceRequest;
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace network {

COMPONENT_EXPORT(NETWORK_CPP)
BASE_DECLARE_FEATURE(kSimpleURLLoaderUseReadAndDiscardBodyOption);

class SimpleURLLoaderStreamConsumer;

// Creates and wraps a URLLoader, and runs it to completion. It's recommended
// that consumers use this class instead of URLLoader directly, due to the
// complexity of the API.
//
// Deleting a SimpleURLLoader before it completes cancels the requests and frees
// any resources it is using (including any partially downloaded files). A
// SimpleURLLoader may be safely deleted while it's invoking any callback method
// that was passed it.
//
// Each SimpleURLLoader can only be used for a single request.
//
// By default SimpleURLLoader will not return the response body for non-2xx
// HTTP codes. See SetAllowHttpErrorResults() for details.
//
// TODO(mmenke): Support the following:
// * Maybe some sort of retry backoff or delay?  ServiceURLLoaderContext enables
// throttling for its URLFetchers.  Could additionally/alternatively support
// 503 + Retry-After.
class COMPONENT_EXPORT(NETWORK_CPP) SimpleURLLoader {
 public:
  // When a failed request should automatically be retried. These are intended
  // to be ORed together.
  enum RetryMode {
    RETRY_NEVER = 0x0,
    // Retries whenever the server returns a 5xx response code.
    RETRY_ON_5XX = 0x1,
    // Retries on net::ERR_NETWORK_CHANGED.
    RETRY_ON_NETWORK_CHANGE = 0x2,
    // Retries on net::ERR_NAME_NOT_RESOLVED.
    RETRY_ON_NAME_NOT_RESOLVED = 0x4,
  };

  // The maximum size DownloadToString will accept.
  static constexpr size_t kMaxBoundedStringDownloadSize = 5 * 1024 * 1024;

  // Maximum upload body size to send as a block to the URLLoaderFactory. This
  // data may appear in memory twice for a while, in the retry case, and there
  // may briefly be 3 to 5 copies as it's copied over the Mojo pipe:  This
  // class's copy (with retries enabled), the source mojo pipe's input copy, the
  // copy on the IPC buffer, the destination mojo pipe's copy, and the network
  // service's copy.
  //
  // Only exposed for tests.
  static constexpr size_t kMaxUploadStringSizeToCopy = 256 * 1024;

  // Callback used when downloading the response body as a std::string.
  // |response_body| is the body of the response, or nullptr on failure. Note
  // that |response_body| may be nullptr even if there's a valid response code
  // like HTTP_OK, which could happen if there's an interruption before the
  // full response body is received. It is safe to delete the SimpleURLLoader
  // during the callback.
  using BodyAsStringCallbackDeprecated =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body)>;
  using BodyAsStringCallback =
      base::OnceCallback<void(std::optional<std::string> response_body)>;

  // Callback used when ignoring the response body. |headers| are the received
  // HTTP headers, or nullptr if none were received. It is safe to delete the
  // SimpleURLLoader during the callback.
  using HeadersOnlyCallback =
      base::OnceCallback<void(scoped_refptr<net::HttpResponseHeaders> headers)>;

  // Callback used when downloading the response body to a file. On failure,
  // |path| will be empty. It is safe to delete the SimpleURLLoader during the
  // callback.
  using DownloadToFileCompleteCallback =
      base::OnceCallback<void(base::FilePath path)>;

  // Callback used when a redirect is being followed. It is safe to delete the
  // SimpleURLLoader during the callback.
  // |url_before_redirect| is the url before redirect that sent the response.
  // |removed_headers| is used to set variations headers that need to be
  // removed for requests when a redirect to a non-Google URL occurs.
  using OnRedirectCallback =
      base::RepeatingCallback<void(const GURL& url_before_redirect,
                                   const net::RedirectInfo& redirect_info,
                                   const mojom::URLResponseHead& response_head,
                                   std::vector<std::string>* removed_headers)>;

  // Callback used when a response is received. It is safe to delete the
  // SimpleURLLoader during the callback.
  using OnResponseStartedCallback =
      base::OnceCallback<void(const GURL& final_url,
                              const mojom::URLResponseHead& response_head)>;

  // Callback used when an upload progress is reported. It is safe to
  // delete the SimpleURLLoader during the callback.
  using UploadProgressCallback =
      base::RepeatingCallback<void(uint64_t position, uint64_t total)>;

  // Callback used for reporting upload or download progress.
  // |current| is the number of bytes transferred thus far for the current
  // fetch attempt (so in case of retries, it might appear to go backwards). It
  // is safe to delete the SimpleURLLoader during the callback.
  using DownloadProgressCallback =
      base::RepeatingCallback<void(uint64_t current)>;

  // Creates a SimpleURLLoader for |resource_request|. The request can be
  // started by calling any one of the Download methods once. The loader may not
  // be reused.
  static std::unique_ptr<SimpleURLLoader> Create(
      std::unique_ptr<ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      base::Location created_from = base::Location::Current());

  // The TickClock to use to configure a timer that tracks if |timeout_duration|
  // has been reached or not. This can be removed once https://crbug.com/905412
  // is completed. When null, the timer falls back to base::TimeTicks::Now().
  static void SetTimeoutTickClockForTest(
      const base::TickClock* timeout_tick_clock);

  SimpleURLLoader(const SimpleURLLoader&) = delete;
  SimpleURLLoader& operator=(const SimpleURLLoader&) = delete;

  virtual ~SimpleURLLoader();

  // Starts the request using |url_loader_factory|. The SimpleURLLoader will
  // accumulate all downloaded data in an in-memory string of bounded size. If
  // |max_body_size| is exceeded, the request will fail with
  // net::ERR_INSUFFICIENT_RESOURCES. |max_body_size| must be no greater than
  // |kMaxBoundedStringDownloadSize|. For anything larger, it's recommended to
  // either save to a temp file, or consume the data as it is received.
  //
  // Whether the request succeeds or fails, the URLLoaderFactory pipe is closed,
  // or the body exceeds |max_body_size|, |body_as_string_callback| will be
  // invoked on completion. Deleting the SimpleURLLoader before the callback is
  // invoked will result in cancelling the request, and the callback will not be
  // called.
  virtual void DownloadToString(
      mojom::URLLoaderFactory* url_loader_factory,
      BodyAsStringCallbackDeprecated body_as_string_callback,
      size_t max_body_size) = 0;
  virtual void DownloadToString(mojom::URLLoaderFactory* url_loader_factory,
                                BodyAsStringCallback body_as_string_callback,
                                size_t max_body_size) = 0;

  // Same as DownloadToString, but downloads to a buffer of unbounded size,
  // potentially causing a crash if the amount of addressable memory is
  // exceeded. It's recommended consumers use one of the other download methods
  // instead (DownloadToString if the body is expected to be of reasonable
  // length, or DownloadToFile otherwise).
  virtual void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      mojom::URLLoaderFactory* url_loader_factory,
      BodyAsStringCallbackDeprecated body_as_string_callback) = 0;
  virtual void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      mojom::URLLoaderFactory* url_loader_factory,
      BodyAsStringCallback body_as_string_callback) = 0;

  // Starts the request using |url_loader_factory|. The SimpleURLLoader will
  // discard the response body as it is received and |headers_only_callback|
  // will be invoked on completion. It is safe to delete the SimpleURLLoader in
  // this callback.
  virtual void DownloadHeadersOnly(
      mojom::URLLoaderFactory* url_loader_factory,
      HeadersOnlyCallback headers_only_callback) = 0;

  // SimpleURLLoader will download the entire response to a file at the
  // specified path. File I/O will happen on another sequence, so it's safe to
  // use this on any sequence.
  //
  // If there's a file, network, Mojo, or http error, or the max limit
  // is exceeded, the file will be automatically destroyed before the callback
  // is invoked and en empty path passed to the callback, unless
  // SetAllowPartialResults() and/or SetAllowHttpErrorResults() were used to
  // indicate partial results are allowed.
  //
  // If the SimpleURLLoader is destroyed before it has invoked the callback, the
  // downloaded file will be deleted asynchronously and the callback will not be
  // invoked, regardless of other settings.
  virtual void DownloadToFile(
      mojom::URLLoaderFactory* url_loader_factory,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      const base::FilePath& file_path,
      int64_t max_body_size = std::numeric_limits<int64_t>::max()) = 0;

  // Same as DownloadToFile, but creates a temporary file instead of taking a
  // FilePath.
  virtual void DownloadToTempFile(
      mojom::URLLoaderFactory* url_loader_factory,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      int64_t max_body_size = std::numeric_limits<int64_t>::max()) = 0;

  // SimpleURLLoader will stream the response body to
  // SimpleURLLoaderStreamConsumer on the current thread. Destroying the
  // SimpleURLLoader will cancel the request, and prevent any subsequent
  // methods from being invoked on the Consumer. The SimpleURLLoader may also be
  // destroyed in any of the Consumer's callbacks.
  //
  // |stream_consumer| must remain valid until either the SimpleURLLoader is
  // deleted, or the consumer's OnComplete() method has been invoked by the
  // SimpleURLLoader.
  virtual void DownloadAsStream(
      mojom::URLLoaderFactory* url_loader_factory,
      SimpleURLLoaderStreamConsumer* stream_consumer) = 0;

  // Sets callback to be invoked during redirects. Callback may delete the
  // SimpleURLLoader.
  virtual void SetOnRedirectCallback(
      const OnRedirectCallback& on_redirect_callback) = 0;

  // Sets callback to be invoked when the response has started.
  // Callback may delete the SimpleURLLoader.
  virtual void SetOnResponseStartedCallback(
      OnResponseStartedCallback on_response_started_callback) = 0;

  // Sets callback to be invoked during resource uploads to provide
  // progress information. Callback may delete the SimpleURLLoader.
  virtual void SetOnUploadProgressCallback(
      UploadProgressCallback on_upload_progress_callback) = 0;

  // Sets callback to be invoked to notify of body download progress.
  // Note that this may be non-monotonic in case of retries.
  // DownloadHeadersOnly() will disregard this setting, and never invoke the
  // callback; otherwise it's guaranteed to fire at least once, with the final
  // size.
  //
  // Callback may delete the SimpleURLLoader.
  virtual void SetOnDownloadProgressCallback(
      DownloadProgressCallback on_download_progress_callback) = 0;

  // Sets whether partially received results are allowed. Defaults to false.
  // When true, if an error is received after reading the body starts or the max
  // allowed body size exceeded, the partial response body that was received
  // will be provided to the caller. The partial response body may be an empty
  // string.
  //
  // When downloading as a stream, this has no observable effect.
  //
  // May only be called before the request is started.
  virtual void SetAllowPartialResults(bool allow_partial_results) = 0;

  // Sets whether bodies of non-2xx responses are returned. May only be called
  // before the request is started.
  //
  // When false, if a non-2xx result is received (Other than a redirect), the
  // request will fail with net::ERR_HTTP_RESPONSE_CODE_FAILURE without waiting
  // to read the response body, though headers will be accessible through
  // response_info().
  //
  // When true, non-2xx responses are treated no differently than other
  // responses, so their response body is returned just as with any other
  // response code, and when they complete, NetError() will return net::OK, if
  // no other problem occurs.
  //
  // Defaults to false.
  virtual void SetAllowHttpErrorResults(bool allow_http_error_results) = 0;

  // Attaches the specified string as the upload body. Depending on the length
  // of the string, the string may be copied to the URLLoader, or may be
  // streamed to it from the current process. May only be called once, and only
  // if ResourceRequest passed to the constructor had a null |request_body|.
  //
  // |content_type| will overwrite any Content-Type header in the
  // ResourceRequest passed to Create().
  //
  // Short strings will be copied and then passed across processes. Long strings
  // if passed by value will be stored in-process and then streamed to the other
  // process.
  //
  // This number of overloads is rather unfortunate, but base::optional_ref
  // doesn't allow implicit conversions.
  virtual void AttachStringForUpload(std::string_view upload_data,
                                     std::string_view upload_content_type) = 0;
  virtual void AttachStringForUpload(std::string_view upload_data) = 0;
  virtual void AttachStringForUpload(const char* upload_data,
                                     std::string_view upload_content_type) = 0;
  virtual void AttachStringForUpload(const char* upload_data) = 0;
  virtual void AttachStringForUpload(std::string&& upload_data,
                                     std::string_view upload_content_type) = 0;
  virtual void AttachStringForUpload(std::string&& upload_data) = 0;

  // Helper method to attach a file for upload, so the consumer won't need to
  // open the file itself off-thread. May only be called once, and only if the
  // ResourceRequest passed to the constructor had a null |request_body|.
  //
  // The |offset| and |length| can optionally be set to specify the desired
  // range of the file to be uploaded. By default the entire file is uploaded.
  //
  // |content_type| will overwrite any Content-Type header in the
  // ResourceRequest passed to Create().
  virtual void AttachFileForUpload(
      const base::FilePath& upload_file_path,
      const std::string& upload_content_type,
      uint64_t offset = 0,
      uint64_t length = std::numeric_limits<uint64_t>::max()) = 0;
  virtual void AttachFileForUpload(const base::FilePath& upload_file_path,
                                   uint64_t offset,
                                   uint64_t length) = 0;
  void AttachFileForUpload(const base::FilePath& upload_file_path) {
    AttachFileForUpload(upload_file_path, 0,
                        std::numeric_limits<uint64_t>::max());
  }

  // Sets the when to try and the max number of times to retry a request, if
  // any. |max_retries| is the number of times to retry the request, not
  // counting the initial request. |retry_mode| is a combination of one or more
  // RetryModes, indicating when the request should be retried. If it is
  // RETRY_NEVER, |max_retries| must be 0.
  //
  // By default, a request will not be retried.
  //
  // When a request is retried, the the request will start again using the
  // initial content::ResourceRequest, even if the request was redirected.
  //
  // Calling this multiple times will overwrite the values previously passed to
  // this method. May only be called before the request is started.
  //
  // Cannot retry requests with an upload body that contains a data pipe that
  // was added to the ResourceRequest passed to Create() by the consumer.
  virtual void SetRetryOptions(int max_retries, int retry_mode) = 0;

  // Sets options for URLLoaderFactory::CreateLoaderAndStart. See
  // //network/public/mojom/url_loader_factory.mojom. This should be
  // called before the request is started.
  virtual void SetURLLoaderFactoryOptions(uint32_t options) = 0;

  // Sets request_id for URLLoaderFactory::CreateLoaderAndStart. See
  // network/public/mojom/url_loader_factory.mojom. This should be called before
  // the request is started.
  virtual void SetRequestID(int32_t request_id) = 0;

  // The amount of time to wait before giving up on a given network request and
  // considering it an error. If not set, then the request is allowed to take
  // as much time as it wants.
  virtual void SetTimeoutDuration(base::TimeDelta timeout_duration) = 0;

  // Returns the net::Error representing the final status of the request. May
  // only be called once the loader has informed the caller of completion.
  virtual int NetError() const = 0;

  // The URLResponseHead for the request. Will be nullptr if ResponseInfo
  // was never received or if `TakeResponseInfo()` has been called. May only be
  // called once the loader has informed the caller of completion.
  virtual const mojom::URLResponseHead* ResponseInfo() const = 0;

  // The URLResponseHead for the request. Ownership is transferred to the
  // caller. Will be nullptr if ResponseInfo was never received. May only be
  // called once the loader has informed the caller of completion.
  virtual mojom::URLResponseHeadPtr TakeResponseInfo() = 0;

  // The URLLoaderCompletionStatus for the request. Will be nullopt if the
  // response never completed. May only be called once the loader has informed
  // the caller of completion.
  virtual const std::optional<URLLoaderCompletionStatus>& CompletionStatus()
      const = 0;

  // Returns the URL that this loader is processing. May only be called once the
  // loader has informed the caller of completion.
  virtual const GURL& GetFinalURL() const = 0;

  // Indicates the request that this loader is processing was loaded from the
  // HTTP cache. May only be called once the loader has informed the caller of
  // completion.
  virtual bool LoadedFromCache() const = 0;

  // Indicates the total of decompressed bytes of the response body.
  // May only be called once the loader has informed the caller of completion.
  //
  // The value might be different than the number of bytes actually
  // received over the network. This happens, for example, in the case
  // of gzipped bodies (Content-Encoding: gzip).
  //
  // When |SetAllowPartialResults| is set to true and there is an error,
  // the method returns the total bytes decompressed bytes until the failure
  // occurred.
  virtual int64_t GetContentSize() const = 0;

  // Returns the number of times retry has been attempted.
  virtual int GetNumRetries() const = 0;

 protected:
  SimpleURLLoader();
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_H_
