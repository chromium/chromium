// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/filter/source_stream.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket/connection_attempts.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {

class AuthChallengeInfo;
class AuthCredentials;
class CookieOptions;
class HttpRequestHeaders;
class HttpResponseInfo;
class IOBuffer;
struct LoadTimingInfo;
class ProxyChain;
class SSLCertRequestInfo;
class SSLInfo;
class SSLPrivateKey;
struct TransportInfo;
class UploadDataStream;
class X509Certificate;

class NET_EXPORT URLRequestJob {
 public:
  explicit URLRequestJob(URLRequest* request);

  URLRequestJob(const URLRequestJob&) = delete;
  URLRequestJob& operator=(const URLRequestJob&) = delete;

  virtual ~URLRequestJob();

  // Returns the request that owns this job.
  URLRequest* request() const {
    return request_;
  }

  // Sets the upload data, most requests have no upload data, so this is a NOP.
  // Job types supporting upload data will override this.
  virtual void SetUpload(UploadDataStream* upload_data_stream);

  // Sets extra request headers for Job types that support request
  // headers. Called once before Start() is called.
  virtual void SetExtraRequestHeaders(const HttpRequestHeaders& headers);

  // Sets the priority of the job. Called once before Start() is
  // called, but also when the priority of the parent request changes.
  virtual void SetPriority(RequestPriority priority);

  // If any error occurs while starting the Job, NotifyStartError should be
  // called asynchronously.
  // This helps ensure that all errors follow more similar notification code
  // paths, which should simplify testing.
  virtual void Start() = 0;

  // This function MUST somehow call NotifyDone/NotifyCanceled or some requests
  // will get leaked. Certain callers use that message to know when they can
  // delete their URLRequest object, even when doing a cancel. The default
  // Kill implementation calls NotifyCanceled, so it is recommended that
  // subclasses call URLRequestJob::Kill() after doing any additional work.
  //
  // The job should endeavor to stop working as soon as is convenient, but must
  // not send and complete notifications from inside this function. Instead,
  // complete notifications (including "canceled") should be sent from a
  // callback run from the message loop.
  //
  // The job is not obliged to immediately stop sending data in response to
  // this call, nor is it obliged to fail with "canceled" unless not all data
  // was sent as a result. A typical case would be where the job is almost
  // complete and can succeed before the canceled notification can be
  // dispatched (from the message loop).
  //
  // The job should be prepared to receive multiple calls to kill it, but only
  // one notification must be issued.
  virtual void Kill();

  // Called to read post-filtered data from this Job, returning the number of
  // bytes read, 0 when there is no more data, or net error if there was an
  // error. This is just the backend for URLRequest::Read, see that function for
  // more info.
  int Read(IOBuffer* buf, int buf_size);

  // Get the number of bytes received from network. The values returned by this
  // will never decrease over the lifetime of the URLRequestJob.
  virtual int64_t GetTotalReceivedBytes() const;

  // Get the number of bytes sent over the network. The values returned by this
  // will never decrease over the lifetime of the URLRequestJob.
  virtual int64_t GetTotalSentBytes() const;

  // Get the number of bytes of the body received from network.
  virtual int64_t GetReceivedBodyBytes() const;

  // Called to fetch the current load state for the job.
  virtual LoadState GetLoadState() const;

  // Called to fetch the charset for this request.  Only makes sense for some
  // types of requests. Returns true on success.  Calling this on a type that
  // doesn't have a charset will return false.
  virtual bool GetCharset(std::string* charset);

  // Called to get response info.
  virtual void GetResponseInfo(HttpResponseInfo* info);

  // This returns the times when events actually occurred, rather than the time
  // each event blocked the request.  See FixupLoadTimingInfo in url_request.h
  // for more information on the difference.
  virtual void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const;

  // Gets the remote endpoint that the network stack is currently fetching the
  // URL from. Returns true and fills in |endpoint| if it is available; returns
  // false and leaves |endpoint| unchanged if it is unavailable.
  virtual bool GetTransactionRemoteEndpoint(IPEndPoint* endpoint) const;

  // Populates the network error details of the most recent origin that the
  // network stack makes the request to.
  virtual void PopulateNetErrorDetails(NetErrorDetails* details) const;

  // Called to determine if this response is a redirect.  Only makes sense
  // for some types of requests.  This method returns true if the response
  // is a redirect, and fills in the location param with the URL of the
  // redirect.  The HTTP status code (e.g., 302) is filled into
  // |*http_status_code| to signify the type of redirect.
  // |*insecure_scheme_was_upgraded| is set to true if the scheme of this
  // request was upgraded to HTTPS due to an 'upgrade-insecure-requests'
  // policy.
  //
  // The caller is responsible for following the redirect by setting up an
  // appropriate replacement Job. Note that the redirected location may be
  // invalid, the caller should be sure it can handle this.
  //
  // The default implementation inspects the response_info_.
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code,
                                  bool* insecure_scheme_was_upgraded);

  // Called to determine if it is okay to copy the reference fragment from the
  // original URL (if existent) to the redirection target when the redirection
  // target has no reference fragment.
  //
  // The default implementation returns true.
  virtual bool CopyFragmentOnRedirect(const GURL& location) const;

  // Called to determine if it is okay to redirect this job to the specified
  // location.  This may be used to implement protocol-specific restrictions.
  // If this function returns false, then the URLRequest will fail
  // reporting ERR_UNSAFE_REDIRECT.
  virtual bool IsSafeRedirect(const GURL& location);

  // Called to determine if this response is asking for authentication.  Only
  // makes sense for some types of requests.  The caller is responsible for
  // obtaining the credentials passing them to SetAuth.
  virtual bool NeedsAuth();

  // Returns a copy of the authentication challenge that came with the server's
  // response.
  virtual std::unique_ptr<AuthChallengeInfo> GetAuthChallengeInfo();

  // Resend the request with authentication credentials.
  virtual void SetAuth(const AuthCredentials& credentials);

  // Display the error page without asking for credentials again.
  virtual void CancelAuth();

  virtual void ContinueWithCertificate(
      scoped_refptr<X509Certificate> client_cert,
      scoped_refptr<SSLPrivateKey> client_private_key);

  // Continue processing the request ignoring the last error.
  virtual void ContinueDespiteLastError();

  void FollowDeferredRedirect(
      const std::optional<std::vector<std::string>>& removed_headers,
      const std::optional<net::HttpRequestHeaders>& modified_headers);

  // Returns true if the Job is done producing response data and has called
  // NotifyDone on the request.
  bool is_done() const { return done_; }

  // Get/Set expected content size
  int64_t expected_content_size() const { return expected_content_size_; }
  void set_expected_content_size(const int64_t& size) {
    expected_content_size_ = size;
  }

  // Whether we have processed the response for that request yet.
  bool has_response_started() const { return has_handled_response_; }

  // The number of bytes read before passing to the filter. This value reflects
  // bytes read even when there is no filter.
  // TODO(caseq): this is only virtual because of StreamURLRequestJob.
  // Consider removing virtual when StreamURLRequestJob is gone.
  virtual int64_t prefilter_bytes_read() const;

  // These methods are not applicable to all connections.
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual int GetResponseCode() const;

  // Returns the socket address for the connection.
  // See url_request.h for details.
  virtual IPEndPoint GetResponseRemoteEndpoint() const;

  // Called after a NetworkDelegate has been informed that the URLRequest
  // will be destroyed. This is used to track that no pending callbacks
  // exist at destruction time of the URLRequestJob, unless they have been
  // canceled by an explicit NetworkDelegate::NotifyURLRequestDestroyed() call.
  virtual void NotifyURLRequestDestroyed();

  // Returns the connection attempts made at the socket layer in the course of
  // executing the URLRequestJob. Should be called after the job has failed or
  // the response headers have been received.
  virtual ConnectionAttempts GetConnectionAttempts() const;

  // Sets a callback that will be invoked each time the request is about to
  // be actually sent and will receive actual request headers that are about
  // to hit the wire, including SPDY/QUIC internal headers.
  virtual void SetRequestHeadersCallback(RequestHeadersCallback callback) {}

  // Sets a callback that will be invoked each time the response is received
  // from the remote party with the actual response headers received.
  virtual void SetResponseHeadersCallback(ResponseHeadersCallback callback) {}

  // Sets a callback that will be invoked each time a 103 Early Hints response
  // is received from the remote party with the actual response headers
  // received.
  virtual void SetEarlyResponseHeadersCallback(
      ResponseHeadersCallback callback) {}

  // Set a callback that will be invoked when a matching shared dictionary is
  // available to determine whether it is allowed to use the dictionary.
  virtual void SetIsSharedDictionaryReadAllowedCallback(
      base::RepeatingCallback<bool()> callback) {}

  // Causes the current transaction always close its active socket on
  // destruction. Does not close H2/H3 sessions.
  virtual void CloseConnectionOnDestruction();

  // Returns true if the request should be retried after activating Storage
  // Access.
  virtual bool NeedsRetryWithStorageAccess();

  // Set a SharedDictionaryGetter which will be used to get a shared
  // dictionary for this request.
  virtual void SetSharedDictionaryGetter(
      SharedDictionaryGetter dictionary_getter) {}

  // Given |policy|, |original_referrer|, and |destination|, returns the
  // referrer URL mandated by |request|'s referrer policy.
  //
  // If |same_origin_out_for_metrics| is non-null, saves to
  // |*same_origin_out_for_metrics| whether |original_referrer| and
  // |destination| are cross-origin.
  // (This allows reporting in a UMA whether the request is same-origin, without
  // recomputing that information.)
  static GURL ComputeReferrerForPolicy(
      ReferrerPolicy policy,
      const GURL& original_referrer,
      const GURL& destination,
      bool* same_origin_out_for_metrics = nullptr);

 protected:
  // Notifies the job that we are connected.
  int NotifyConnected(const TransportInfo& info,
                      CompletionOnceCallback callback);

  // Notifies the job that a certificate is requested.
  void NotifyCertificateRequested(SSLCertRequestInfo* cert_request_info);

  // Notifies the job about an SSL certificate error.
  void NotifySSLCertificateError(int net_error,
                                 const SSLInfo& ssl_info,
                                 bool fatal);

  // Delegates to URLRequest.
  bool CanSetCookie(const net::CanonicalCookie& cookie,
                    CookieOptions* options,
                    const net::FirstPartySetMetadata& first_party_set_metadata,
                    CookieInclusionStatus* inclusion_status) const;

  // Notifies the job that headers have been received.
  void NotifyHeadersComplete();

  // Called when the final set headers have been received (no more redirects to
  // follow, and no more auth challenges that will be responded to).
  void NotifyFinalHeadersReceived();

  // Notifies the request that a start error has occurred.
  // NOTE: Must not be called synchronously from |Start|.
  void NotifyStartError(int net_error);

  // Used as an asynchronous callback for Kill to notify the URLRequest
  // that we were canceled.
  void NotifyCanceled();

  // See corresponding functions in url_request.h.
  void OnCallToDelegate(NetLogEventType type);
  void OnCallToDelegateComplete();

  // Called to read raw (pre-filtered) data from this Job. Reads at most
  // |buf_size| bytes into |buf|.
  // Possible return values:
  //   >= 0: Read completed synchronously. Return value is the number of bytes
  //         read. 0 means eof.
  //   ERR_IO_PENDING: Read pending asynchronously.
  //                   When the read completes, |ReadRawDataComplete| should be
  //                   called.
  //   Any other negative number: Read failed synchronously. Return value is a
  //                   network error code.
  // This method might hold onto a reference to |buf| (by incrementing the
  // refcount) until the method completes or is cancelled.
  virtual int ReadRawData(IOBuffer* buf, int buf_size);

  // Called to tell the job that a filter has successfully reached the end of
  // the stream.
  virtual void DoneReading();

  // Called to tell the job that the body won't be read because it's a redirect.
  // This is needed so that redirect headers can be cached even though their
  // bodies are never read.
  virtual void DoneReadingRedirectResponse();

  // Called to tell the job that the body won't be read (and headers won't be
  // cached) because we're going to retry the request.
  virtual void DoneReadingRetryResponse();

  // Called to set up a SourceStream chain for this request.
  // Subclasses should return the appropriate last SourceStream of the chain,
  // or nullptr on error.
  virtual std::unique_ptr<SourceStream> SetUpSourceStream();

  // Set the proxy chain that was used, if any.
  void SetProxyChain(const ProxyChain& proxy_chain);

  // The number of bytes read after passing through the filter. This value
  // reflects bytes read even when there is no filter.
  int64_t postfilter_bytes_read() const { return postfilter_bytes_read_; }

  // Turns an integer result code into an Error and a count of bytes read.
  // The semantics are:
  //   |result| >= 0: |*error| == OK, |*count| == |result|
  //   |result| < 0: |*error| = |result|, |*count| == 0
  static void ConvertResultToError(int result, Error* error, int* count);

  // Completion callback for raw reads. See |ReadRawData| for details.
  // |bytes_read| is either >= 0 to indicate a successful read and count of
  // bytes read, or < 0 to indicate an error.
  // On return, |this| may be deleted.
  void ReadRawDataComplete(int bytes_read);

  // The request that initiated this job. This value will never be nullptr.
  const raw_ptr<URLRequest> request_;

 private:
  class URLRequestJobSourceStream;

  // Helper method used to perform tasks after reading from |source_stream_| is
  // completed. |synchronous| true if the read completed synchronously.
  // See the documentation for |Read| above for the contract of this method.
  void SourceStreamReadComplete(bool synchronous, int result);

  // Invokes ReadRawData and records bytes read if the read completes
  // synchronously.
  int ReadRawDataHelper(IOBuffer* buf,
                        int buf_size,
                        CompletionOnceCallback callback);

  // Returns OK if |new_url| is a valid redirect target and an error code
  // otherwise.
  int CanFollowRedirect(const GURL& new_url);

  // Called in response to a redirect that was not canceled to follow the
  // redirect. The current job will be replaced with a new job loading the
  // given redirect destination.
  void FollowRedirect(
      const RedirectInfo& redirect_info,
      const std::optional<std::vector<std::string>>& removed_headers,
      const std::optional<net::HttpRequestHeaders>& modified_headers);

  // Called after every raw read. If |bytes_read| is > 0, this indicates
  // a successful read of |bytes_read| unfiltered bytes. If |bytes_read|
  // is 0, this indicates that there is no additional data to read.
  // If |bytes_read| is negative, no bytes were read.
  void GatherRawReadStats(int bytes_read);

  // Updates the profiling info and notifies observers that an additional
  // |bytes_read| unfiltered bytes have been read for this job.
  void RecordBytesRead(int bytes_read);

  // OnDone marks that request is done. It is really a glorified
  // set_status, but also does internal state checking and job tracking. It
  // should be called once per request, when the job is finished doing all IO.
  //
  // If |notify_done| is true, will notify the URLRequest if there was an error
  // asynchronously.  Otherwise, the caller will need to do this itself,
  // possibly through a synchronous return value.
  // TODO(mmenke):  Remove |notify_done|, and make caller handle notification.
  void OnDone(int net_error, bool notify_done);

  // Takes care of the notification initiated by OnDone() to avoid re-entering
  // the URLRequest::Delegate.
  void NotifyDone();

  // Indicates that the job is done producing data, either it has completed
  // all the data or an error has been encountered. Set exclusively by
  // NotifyDone so that it is kept in sync with the request.
  bool done_ = false;

  // Number of raw network bytes read from job subclass.
  int64_t prefilter_bytes_read_ = 0;

  // Number of bytes after applying |source_stream_| filters.
  int64_t postfilter_bytes_read_ = 0;

  // The first SourceStream of the SourceStream chain used.
  std::unique_ptr<SourceStream> source_stream_;

  // Keep a reference to the buffer passed in via URLRequestJob::Read() so it
  // doesn't get destroyed when the read has not completed.
  scoped_refptr<IOBuffer> pending_read_buffer_;

  // We keep a pointer to the read buffer while asynchronous reads are
  // in progress, so we are able to pass those bytes to job observers.
  scoped_refptr<IOBuffer> raw_read_buffer_;

  // Used by HandleResponseIfNecessary to track whether we've sent the
  // OnResponseStarted callback and potentially redirect callbacks as well.
  bool has_handled_response_ = false;

  // Expected content size
  int64_t expected_content_size_ = -1;

  // Set when a redirect is deferred. Redirects are deferred after validity
  // checks are performed, so this field must not be modified.
  std::optional<RedirectInfo> deferred_redirect_info_;

  // Non-null if ReadRawData() returned ERR_IO_PENDING, and the read has not
  // completed.
  CompletionOnceCallback read_raw_callback_;

  base::WeakPtrFactory<URLRequestJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_H_
