// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_FETCHER_CORE_H_
#define NET_URL_REQUEST_URL_FETCHER_CORE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/ip_endpoint.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter_observer.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class DrainableIOBuffer;
class HttpResponseHeaders;
class IOBuffer;
class URLFetcherDelegate;
class URLFetcherResponseWriter;
class URLRequestContextGetter;
class URLRequestThrottlerEntryInterface;

class URLFetcherCore : public base::RefCountedThreadSafe<URLFetcherCore>,
                       public URLRequest::Delegate,
                       public URLRequestContextGetterObserver {
 public:
  URLFetcherCore(URLFetcher* fetcher,
                 const GURL& original_url,
                 URLFetcher::RequestType request_type,
                 URLFetcherDelegate* d,
                 net::NetworkTrafficAnnotationTag traffic_annotation);

  // Starts the load. It's important that this not happen in the constructor
  // because it causes the IO thread to begin AddRef()ing and Release()ing
  // us. If our caller hasn't had time to fully construct us and take a
  // reference, the IO thread could interrupt things, run a task, Release()
  // us, and destroy us, leaving the caller with an already-destroyed object
  // when construction finishes.
  void Start();

  // Stops any in-progress load and ensures no callback will happen. It is
  // safe to call this multiple times.
  void Stop();

  // URLFetcher-like functions.

  // For POST requests, set |content_type| to the MIME type of the
  // content and set |content| to the data to upload.
  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_content);
  void SetUploadFilePath(const std::string& upload_content_type,
                         const base::FilePath& file_path,
                         uint64_t range_offset,
                         uint64_t range_length,
                         scoped_refptr<base::TaskRunner> file_task_runner);
  void SetUploadStreamFactory(
      const std::string& upload_content_type,
      const URLFetcher::CreateUploadStreamCallback& callback);
  void SetChunkedUpload(const std::string& upload_content_type);
  // Adds a block of data to be uploaded in a POST body. This can only be
  // called after Start().
  void AppendChunkToUpload(const std::string& data, bool is_last_chunk);
  // |flags| are flags to apply to the load operation--these should be
  // one or more of the LOAD_* flags defined in net/base/load_flags.h.
  void SetLoadFlags(int load_flags);
  int GetLoadFlags() const;
  void SetAllowCredentials(bool allow_credentials);
  void SetReferrer(const std::string& referrer);
  void SetReferrerPolicy(URLRequest::ReferrerPolicy referrer_policy);
  void SetExtraRequestHeaders(const std::string& extra_request_headers);
  void AddExtraRequestHeader(const std::string& header_line);
  void SetRequestContext(URLRequestContextGetter* request_context_getter);
  // Set the origin that should be considered as "initiating" the fetch. This
  // URL
  // will be considered the "first-party" when applying cookie blocking policy
  // to requests, and treated as the request's initiator.
  void SetInitiator(const base::Optional<url::Origin>& initiator);
  // Set the key and data callback that is used when setting the user
  // data on any URLRequest objects this object creates.
  void SetURLRequestUserData(
      const void* key,
      const URLFetcher::CreateDataCallback& create_data_callback);
  void SetStopOnRedirect(bool stop_on_redirect);
  void SetAutomaticallyRetryOn5xx(bool retry);
  void SetMaxRetriesOn5xx(int max_retries);
  int GetMaxRetriesOn5xx() const;
  base::TimeDelta GetBackoffDelay() const;
  void SetAutomaticallyRetryOnNetworkChanges(int max_retries);
  void SaveResponseToFileAtPath(
      const base::FilePath& file_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  void SaveResponseToTemporaryFile(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  void SaveResponseWithWriter(
      std::unique_ptr<URLFetcherResponseWriter> response_writer);
  HttpResponseHeaders* GetResponseHeaders() const;
  IPEndPoint GetSocketAddress() const;
  const ProxyServer& ProxyServerUsed() const;
  bool WasFetchedViaProxy() const;
  bool WasCached() const;
  const GURL& GetOriginalURL() const;
  const GURL& GetURL() const;
  const URLRequestStatus& GetStatus() const;
  int GetResponseCode() const;
  int64_t GetReceivedResponseContentLength() const;
  int64_t GetTotalReceivedBytes() const;
  // Reports that the received content was malformed (i.e. failed parsing
  // or validation). This makes the throttling logic that does exponential
  // back-off when servers are having problems treat the current request as
  // a failure. Your call to this method will be ignored if your request is
  // already considered a failure based on the HTTP response code or response
  // headers.
  void ReceivedContentWasMalformed();
  bool GetResponseAsString(std::string* out_response_string) const;
  bool GetResponseAsFilePath(bool take_ownership,
                             base::FilePath* out_response_path);

  // Overridden from URLRequest::Delegate:
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int bytes_read) override;
  void OnCertificateRequested(URLRequest* request,
                              SSLCertRequestInfo* cert_request_info) override;

  // Overridden from URLRequestContextGetterObserver:
  void OnContextShuttingDown() override;

  URLFetcherDelegate* delegate() const { return delegate_; }
  static void CancelAll();
  static int GetNumFetcherCores();
  static void SetEnableInterceptionForTests(bool enabled);
  static void SetIgnoreCertificateRequests(bool ignored);

 private:
  friend class base::RefCountedThreadSafe<URLFetcherCore>;

  // TODO(mmenke):  Remove this class.
  class Registry {
   public:
    Registry();
    ~Registry();

    void AddURLFetcherCore(URLFetcherCore* core);
    void RemoveURLFetcherCore(URLFetcherCore* core);

    void CancelAll();

    int size() const {
      return fetchers_.size();
    }

   private:
    std::set<URLFetcherCore*> fetchers_;

    DISALLOW_COPY_AND_ASSIGN(Registry);
  };

  ~URLFetcherCore() override;

  // Wrapper functions that allow us to ensure actions happen on the right
  // thread.
  void StartOnIOThread();
  void StartURLRequest();
  void DidInitializeWriter(int result);
  void StartURLRequestWhenAppropriate();
  void CancelURLRequest(int error);
  void OnCompletedURLRequest(base::TimeDelta backoff_delay);
  void InformDelegateFetchIsComplete();
  void NotifyMalformedContent();
  void DidFinishWriting(int result);
  void RetryOrCompleteUrlFetch();

  // Cancels the URLRequest and informs the delegate that it failed with the
  // specified error. Must be called on network thread.
  void CancelRequestAndInformDelegate(int result);

  // Deletes the request, removes it from the registry, and removes the
  // destruction observer.
  void ReleaseRequest();

  // Returns the max value of exponential back-off release time for
  // |original_url_| and |url_|.
  base::TimeTicks GetBackoffReleaseTime();

  void CompleteAddingUploadDataChunk(const std::string& data,
                                     bool is_last_chunk);

  // Writes all bytes stored in |data| with |response_writer_|.
  // Returns OK if all bytes in |data| get written synchronously. Otherwise,
  // returns ERR_IO_PENDING or a network error code.
  int WriteBuffer(scoped_refptr<DrainableIOBuffer> data);

  // Used to implement WriteBuffer().
  void DidWriteBuffer(scoped_refptr<DrainableIOBuffer> data, int result);

  // Read response bytes from the request.
  void ReadResponse();

  // Notify Delegate about the progress of upload/download.
  void InformDelegateUploadProgress();
  void InformDelegateUploadProgressInDelegateSequence(int64_t current,
                                                      int64_t total);
  void InformDelegateDownloadProgress();
  void InformDelegateDownloadProgressInDelegateSequence(
      int64_t current,
      int64_t total,
      int64_t current_network_bytes);

  // Check if any upload data is set or not.
  void AssertHasNoUploadData() const;

  URLFetcher* fetcher_;              // Corresponding fetcher object
  GURL original_url_;                // The URL we were asked to fetch
  GURL url_;                         // The URL we eventually wound up at
  URLFetcher::RequestType request_type_;  // What type of request is this?
  URLRequestStatus status_;          // Status of the request
  URLFetcherDelegate* delegate_;     // Object to notify on completion
  // Task runner for the creating sequence. Used to interact with the delegate.
  const scoped_refptr<base::SequencedTaskRunner> delegate_task_runner_;
  // Task runner for network operations.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  // Task runner for upload file access.
  scoped_refptr<base::TaskRunner> upload_file_task_runner_;
  std::unique_ptr<URLRequest> request_;  // The actual request this wraps
  int load_flags_;                   // Flags for the load operation
  // Whether credentials are sent along with the request.
  base::Optional<bool> allow_credentials_;
  int response_code_;                // HTTP status code for the request
  scoped_refptr<IOBuffer> buffer_;
                                     // Read buffer
  scoped_refptr<URLRequestContextGetter> request_context_getter_;
                                     // Cookie/cache info for the request
  base::Optional<url::Origin> initiator_;  // The request's initiator
  // The user data to add to each newly-created URLRequest.
  const void* url_request_data_key_;
  URLFetcher::CreateDataCallback url_request_create_data_callback_;
  HttpRequestHeaders extra_request_headers_;
  scoped_refptr<HttpResponseHeaders> response_headers_;
  ProxyServer proxy_server_;
  bool was_cached_;
  int64_t received_response_content_length_;
  int64_t total_received_bytes_;
  IPEndPoint remote_endpoint_;

  bool upload_content_set_;          // SetUploadData has been called
  std::string upload_content_;       // HTTP POST payload
  base::FilePath upload_file_path_;  // Path to file containing POST payload
  uint64_t upload_range_offset_;     // Offset from the beginning of the file
                                     // to be uploaded.
  uint64_t upload_range_length_;     // The length of the part of file to be
                                     // uploaded.
  URLFetcher::CreateUploadStreamCallback
      upload_stream_factory_;        // Callback to create HTTP POST payload.
  std::string upload_content_type_;  // MIME type of POST payload
  std::string referrer_;             // HTTP Referer header value and policy
  URLRequest::ReferrerPolicy referrer_policy_;
  bool is_chunked_upload_;           // True if using chunked transfer encoding

  // Used to write to |chunked_stream|, even after ownership has been passed to
  // the URLRequest. Continues to be valid even after the request deletes its
  // upload data.
  std::unique_ptr<ChunkedUploadDataStream::Writer> chunked_stream_writer_;

  // Temporary storage of ChunkedUploadDataStream, before request is created.
  std::unique_ptr<ChunkedUploadDataStream> chunked_stream_;

  // Used to determine how long to wait before making a request or doing a
  // retry.
  //
  // Both of them can only be accessed on the IO thread.
  //
  // To determine the proper backoff timing, throttler entries for
  // both |original_URL| and |url| are needed. For example, consider
  // the case that URL A redirects to URL B, for which the server
  // returns a 500 response. In this case, the exponential back-off
  // release time of URL A won't increase. If only the backoff
  // constraints for URL A are considered, too many requests for URL A
  // may be sent in a short period of time.
  //
  // Both of these will be NULL if
  // URLRequestContext::throttler_manager() is NULL.
  scoped_refptr<URLRequestThrottlerEntryInterface>
      original_url_throttler_entry_;
  scoped_refptr<URLRequestThrottlerEntryInterface> url_throttler_entry_;

  // True if the URLFetcher has been cancelled.
  bool was_cancelled_;

  // Writer object to write response to the destination like file and string.
  std::unique_ptr<URLFetcherResponseWriter> response_writer_;

  // By default any server-initiated redirects are automatically followed. If
  // this flag is set to true, however, a redirect will halt the fetch and call
  // back to to the delegate immediately.
  bool stop_on_redirect_;
  // True when we're actually stopped due to a redirect halted by the above. We
  // use this to ensure that |url_| is set to the redirect destination rather
  // than the originally-fetched URL.
  bool stopped_on_redirect_;

  // If |automatically_retry_on_5xx_| is false, 5xx responses will be
  // propagated to the observer, if it is true URLFetcher will automatically
  // re-execute the request, after the back-off delay has expired.
  // true by default.
  bool automatically_retry_on_5xx_;
  // |num_retries_on_5xx_| indicates how many times we've failed to successfully
  // fetch this URL due to 5xx responses. Once this value exceeds the maximum
  // number of retries specified by the owner URLFetcher instance,
  // we'll give up.
  int num_retries_on_5xx_;
  // Maximum retries allowed when 5xx responses are received.
  int max_retries_on_5xx_;
  // Back-off time delay. 0 by default.
  base::TimeDelta backoff_delay_;

  // The number of retries that have been attempted due to ERR_NETWORK_CHANGED.
  int num_retries_on_network_changes_;
  // Maximum retries allowed when the request fails with ERR_NETWORK_CHANGED.
  // 0 by default.
  int max_retries_on_network_changes_;

  // Timer to poll the progress of uploading for POST and PUT requests.
  // When crbug.com/119629 is fixed, scoped_ptr is not necessary here.
  std::unique_ptr<base::RepeatingTimer> upload_progress_checker_timer_;
  // Number of bytes sent so far.
  int64_t current_upload_bytes_;
  // Number of bytes received so far.
  int64_t current_response_bytes_;
  // Total expected bytes to receive (-1 if it cannot be determined).
  int64_t total_response_bytes_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  static base::LazyInstance<Registry>::DestructorAtExit g_registry;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherCore);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_CORE_H_
