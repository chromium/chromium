// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_URL_LOADER_THROTTLE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_URL_LOADER_THROTTLE_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/request_priority.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/common_export.h"

class GURL;

namespace net {
class HttpRequestHeaders;
struct RedirectInfo;
}

namespace network {
struct ResourceRequest;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace blink {

// A URLLoaderThrottle gets notified at various points during the process of
// loading a resource. At each stage, it has the opportunity to defer the
// resource load.
//
// Note that while a single throttle deferring a load at any given step will
// block the load from progressing further until a subsequent Delegate::Resume()
// call is made, it does NOT prevent subsequent throttles from processing the
// same step of the request if multiple throttles are affecting the load.
class BLINK_COMMON_EXPORT URLLoaderThrottle {
 public:
  // An interface for the throttle implementation to resume (when deferred) or
  // cancel the resource load. Please note that these methods could be called
  // in-band (i.e., inside URLLoaderThrottle notification methods such as
  // WillStartRequest), or out-of-band.
  //
  // It is guaranteed that throttles calling these methods won't be destroyed
  // synchronously.
  class BLINK_COMMON_EXPORT Delegate {
   public:
    // Cancels the resource load with the specified error code and an optional,
    // application-defined reason description.
    virtual void CancelWithError(int error_code,
                                 base::StringPiece custom_reason = "") = 0;

    // Resumes the deferred resource load. It is a no-op if the resource load is
    // not deferred or has already been canceled.
    virtual void Resume() = 0;

    virtual void SetPriority(net::RequestPriority priority);

    // Updates the request headers which is deferred  to be sent. This method
    // needs to be called when the response is deferred on WillStartRequest or
    // WillRedirectRequest and before calling Delegate::Resume().
    virtual void UpdateDeferredRequestHeaders(
        const net::HttpRequestHeaders& modified_request_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_request_headers);

    // Updates the response head which is deferred to be sent. This method needs
    // to be called when the response is deferred on
    // URLLoaderThrottle::WillProcessResponse() and before calling
    // Delegate::Resume().
    virtual void UpdateDeferredResponseHead(
        network::mojom::URLResponseHeadPtr new_response_head);

    // Pauses/resumes reading response body if the resource is fetched from
    // network.
    virtual void PauseReadingBodyFromNet();
    virtual void ResumeReadingBodyFromNet();

    // Replaces the URLLoader and URLLoaderClient endpoints held by the
    // ThrottlingURLLoader instance.
    virtual void InterceptResponse(
        mojo::PendingRemote<network::mojom::URLLoader> new_loader,
        mojo::PendingReceiver<network::mojom::URLLoaderClient>
            new_client_receiver,
        mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
        mojo::PendingReceiver<network::mojom::URLLoaderClient>*
            original_client_receiver);

    // Restarts the URL loader using |additional_load_flags|.
    //
    // Restarting is only valid while executing within
    // BeforeWillProcessResponse(), or during its deferred handling (before
    // having called Resume()).
    //
    // When a URL loader is restarted, throttles will NOT have their
    // WillStartRequest() method called again - that is only called for the
    // initial request start.
    //
    // If multiple throttles call RestartWithFlags() then the URL loader will be
    // restarted using a combined value of all of the |additional_load_flags|.
    virtual void RestartWithFlags(int additional_load_flags);

    // Restarts the URL loader using |additional_load_flags| and the unmodified
    // URL if it was changed in WillStartRequest().
    //
    // If called on an URL loader that did not modify the URL in
    // WillStartRequest(), this method has the same outcome as
    // RestartWithFlags().
    //
    // Restarting is only valid while executing within
    // BeforeWillProcessResponse(), or during its deferred handling (before
    // having called Resume()).
    //
    // When a URL loader is restarted, throttles will NOT have their
    // WillStartRequest() method called again - that is only called for the
    // initial request start.
    //
    // If multiple throttles call RestartWithFlags() and
    // RestartWithURLResetAndFlags() then the URL loader will be restarted
    // using a combined value of all of the |additional_load_flags|.
    virtual void RestartWithURLResetAndFlags(int additional_load_flags);

    // Restarts the URL loader immediately using |additional_load_flags| and the
    // unmodified URL if it was changed in WillStartRequest().
    //
    // Restarting is only valid before BeforeWillProcessResponse() is called.
    virtual void RestartWithURLResetAndFlagsNow(int additional_load_flags);

   protected:
    virtual ~Delegate();
  };

  virtual ~URLLoaderThrottle();

  // Detaches this object from the current sequence in preparation for a move to
  // a different sequence. If this method is called it must be before any of the
  // Will* methods below and may only be called once.
  virtual void DetachFromCurrentSequence();

  // Called before the resource request is started.
  // |request| needs to be modified before the callback returns (i.e.
  // asynchronously touching the pointer in defer case is not valid)
  // When |request->url| is modified it will make an internal redirect, which
  // might have some side-effects: drop upload streams data might be dropped,
  // redirect count may be reached.
  //
  // Implementations should be aware that throttling can happen multiple times
  // for the same |request|, even after one instance of the same throttle
  // subclass already modified the request. This happens, e.g., when a service
  // worker initially elects to handle a request but then later falls back to
  // network, so new throttles are created for another URLLoaderFactory to
  // handle the request.
  virtual void WillStartRequest(network::ResourceRequest* request, bool* defer);

  // Called when the request was redirected.  |redirect_info| contains the
  // redirect responses's HTTP status code and some information about the new
  // request that will be sent if the redirect is followed, including the new
  // URL and new method.
  //
  // Request headers added to |to_be_removed_request_headers| will be removed
  // before the redirect is followed. Headers added to
  // |modified_request_headers| and |modified_cors_exempt_request_headers| will
  // be merged into the existing request headers and cors_exempt_headers before
  // the redirect is followed.
  //
  // If |redirect_info->new_url| is modified by a throttle, the request will be
  // redirected to the new URL. Only one throttle can update this and it must
  // have the same origin as |redirect_info->new_url|. See the comments for
  // WillStartRequest on possible side-effects.
  virtual void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers);

  // Called when the response headers and meta data are available.
  // TODO(776312): Migrate this URL to URLResponseHead.
  virtual void WillProcessResponse(
      const GURL& response_url,
      network::mojom::URLResponseHead* response_head,
      bool* defer);

  // Called prior WillProcessResponse() to allow throttles to restart the URL
  // load by calling delegate_->RestartWithFlags().
  //
  // Having this method separate from WillProcessResponse() ensures that
  // WillProcessResponse() is called at most once even in the presence of
  // restarts.
  virtual void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      bool* defer);

  // Called if there is a non-OK net::Error in the completion status.
  virtual void WillOnCompleteWithError(
      const network::URLLoaderCompletionStatus& status,
      bool* defer);

  // Must return true if the throttle may make cross-scheme redirects
  // (which is usually considered unsafe, so allowed only if the setting
  // is made very explicitly).
  virtual bool makes_unsafe_redirect();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 protected:
  URLLoaderThrottle();

  Delegate* delegate_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_URL_LOADER_THROTTLE_H_
