// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PAC_FILE_FETCHER_IMPL_H_
#define NET_PROXY_RESOLUTION_PAC_FILE_FETCHER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/pac_file_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

class GURL;

namespace net {

class URLRequestContext;

// Implementation of PacFileFetcher that downloads scripts using the
// specified request context.
class NET_EXPORT PacFileFetcherImpl : public PacFileFetcher,
                                      public URLRequest::Delegate {
 public:
  // Creates a PacFileFetcher that issues requests through
  // |url_request_context|. |url_request_context| must remain valid for the
  // lifetime of PacFileFetcherImpl.
  // Note that while a request is in progress, we will be holding a reference
  // to |url_request_context|. Be careful not to create cycles between the
  // fetcher and the context; you can break such cycles by calling Cancel().
  //
  // Fetch() supports the following URL schemes, provided the underlying
  // |url_request_context| also supports them:
  //
  //   * http://
  //   * https://
  //   * ftp://
  //   * data:
  static std::unique_ptr<PacFileFetcherImpl> Create(
      URLRequestContext* url_request_context);

  ~PacFileFetcherImpl() override;

  // Used by unit-tests to modify the default limits.
  base::TimeDelta SetTimeoutConstraint(base::TimeDelta timeout);
  size_t SetSizeConstraint(size_t size_bytes);

  void OnResponseCompleted(URLRequest* request, int net_error);

  // PacFileFetcher methods:
  int Fetch(const GURL& url,
            base::string16* text,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag traffic_annotation) override;
  void Cancel() override;
  URLRequestContext* GetRequestContext() const override;
  void OnShutdown() override;

  // URLRequest::Delegate methods:
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(URLRequest* request,
                      const AuthChallengeInfo& auth_info) override;
  void OnSSLCertificateError(URLRequest* request,
                             int net_error,
                             const SSLInfo& ssl_info,
                             bool is_hsts_ok) override;
  void OnResponseStarted(URLRequest* request, int net_error) override;
  void OnReadCompleted(URLRequest* request, int num_bytes) override;

 private:
  enum { kBufSize = 4096 };

  explicit PacFileFetcherImpl(URLRequestContext* url_request_context);

  // Returns true if |url| has an acceptable URL scheme (i.e. http://, https://,
  // etc).
  bool IsUrlSchemeAllowed(const GURL& url) const;

  // Read more bytes from the response.
  void ReadBody(URLRequest* request);

  // Handles a response from Read(). Returns true if we should continue trying
  // to read. |num_bytes| is 0 for EOF, and < 0 on errors.
  bool ConsumeBytesRead(URLRequest* request, int num_bytes);

  // Called once the request has completed to notify the caller of
  // |response_code_| and |response_text_|.
  void FetchCompleted();

  // Clear out the state for the current request.
  void ResetCurRequestState();

  // Callback for time-out task of request with id |id|.
  void OnTimeout(int id);

  // The context used for making network requests.  Set to nullptr by
  // OnShutdown.
  URLRequestContext* url_request_context_;

  // Buffer that URLRequest writes into.
  scoped_refptr<IOBuffer> buf_;

  // The next ID to use for |cur_request_| (monotonically increasing).
  int next_id_;

  // The current (in progress) request, or NULL.
  std::unique_ptr<URLRequest> cur_request_;

  // State for current request (only valid when |cur_request_| is not NULL):

  // Unique ID for the current request.
  int cur_request_id_;

  // Callback to invoke on completion of the fetch.
  CompletionOnceCallback callback_;

  // Holds the error condition that was hit on the current request, or OK.
  int result_code_;

  // Holds the bytes read so far. Will not exceed |max_response_bytes|.
  std::string bytes_read_so_far_;

  // This buffer is owned by the owner of |callback|, and will be filled with
  // UTF16 response on completion.
  base::string16* result_text_;

  // The maximum number of bytes to allow in responses.
  size_t max_response_bytes_;

  // The maximum amount of time to wait for download to complete.
  base::TimeDelta max_duration_;

  // The time that the fetch started.
  base::TimeTicks fetch_start_time_;

  // The time that the first byte was received.
  base::TimeTicks fetch_time_to_first_byte_;

  // Factory for creating the time-out task. This takes care of revoking
  // outstanding tasks when |this| is deleted.
  base::WeakPtrFactory<PacFileFetcherImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PacFileFetcherImpl);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PAC_FILE_FETCHER_IMPL_H_
