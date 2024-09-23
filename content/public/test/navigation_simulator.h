// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/reload_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-forward.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace net {
class IPEndPoint;
class HttpResponseHeaders;
class SSLInfo;
}  // namespace net

namespace content {

class NavigationController;
class NavigationHandle;
class RenderFrameHost;
class WebContents;
struct GlobalRequestID;

// An interface for simulating a navigation in unit tests. Supports both
// renderer and browser-initiated navigations.
// Note: this should not be used in browser tests.
class NavigationSimulator {
 public:
  // Simulates a browser-initiated navigation to |url| started in
  // |web_contents| from start to commit. Returns the RenderFrameHost that
  // committed the navigation.
  static RenderFrameHost* NavigateAndCommitFromBrowser(
      WebContents* web_contents,
      const GURL& url);

  // Simulates the page reloading. Returns the RenderFrameHost that committed
  // the navigation.
  static RenderFrameHost* Reload(WebContents* web_contents);

  // Simulates a back navigation from start to commit. Returns the
  // RenderFrameHost that committed the navigation.
  static RenderFrameHost* GoBack(WebContents* web_contents);

  // Simulates a forward navigation from start to commit. Returns the
  // RenderFrameHost that committed the navigation.
  static RenderFrameHost* GoForward(WebContents* web_contents);

  // Simulates a navigation to the given offset of the web_contents navigation
  // controller, from start to finish.
  static RenderFrameHost* GoToOffset(WebContents* web_contents, int offset);

  // Simulates a renderer-initiated navigation to |url| started in
  // |render_frame_host| from start to commit. Returns the RenderFramehost that
  // committed the navigation.
  static RenderFrameHost* NavigateAndCommitFromDocument(
      const GURL& original_url,
      RenderFrameHost* render_frame_host);

  // Simulates a failed browser-initiated navigation to |url| started in
  // |web_contents| from start to commit. Returns the RenderFrameHost that
  // committed the error page for the navigation, or nullptr if the navigation
  // error did not result in an error page.
  static RenderFrameHost* NavigateAndFailFromBrowser(WebContents* web_contents,
                                                     const GURL& url,
                                                     int net_error_code);

  // Simulates the page reloading and failing. Returns the RenderFrameHost that
  // committed the error page for the navigation, or nullptr if the navigation
  // error did not result in an error page.
  static RenderFrameHost* ReloadAndFail(WebContents* web_contents,
                                        int net_error_code);

  // Simulates a failed back navigation. Returns the RenderFrameHost that
  // committed the error page for the navigation, or nullptr if the navigation
  // error did not result in an error page.
  static RenderFrameHost* GoBackAndFail(WebContents* web_contents,
                                        int net_error_code);

  // Simulates a failed offset navigation. Returns the RenderFrameHost that
  // committed the error page for the navigation, or nullptr if the navigation
  // error did not result in an error page.
  static RenderFrameHost* GoToOffsetAndFail(WebContents* web_contents,
                                            int offset,
                                            int net_error_code);

  // Simulates a failed renderer-initiated navigation to |url| started in
  // |render_frame_host| from start to commit. Returns the RenderFramehost that
  // committed the error page for the navigation, or nullptr if the navigation
  // error did not result in an error page.
  static RenderFrameHost* NavigateAndFailFromDocument(
      const GURL& original_url,
      int net_error_code,
      RenderFrameHost* render_frame_host);

  // ---------------------------------------------------------------------------

  // All the following methods should be used when more precise control over the
  // navigation is needed.

  // Creates a NavigationSimulator that will be used to simulate a
  // browser-initiated navigation to |original_url| started in |contents|.
  static std::unique_ptr<NavigationSimulator> CreateBrowserInitiated(
      const GURL& original_url,
      WebContents* contents);

  // Creates a NavigationSimulator that will be used to simulate a history
  // navigation to one of the |web_contents|'s navigation controller |offset|.
  // E.g. offset -1 for back navigations and 1 for forward navigations. If
  // |is_renderer_initiated| is true, the navigation will simulate a history
  // navigation initiated via JS.
  static std::unique_ptr<NavigationSimulator> CreateHistoryNavigation(
      int offset,
      WebContents* web_contents,
      bool is_renderer_initiated);

  // Creates a NavigationSimulator that will be used to simulate a
  // renderer-initiated navigation to |original_url| started by
  // |render_frame_host|.
  static std::unique_ptr<NavigationSimulator> CreateRendererInitiated(
      const GURL& original_url,
      RenderFrameHost* render_frame_host);

  // Creates a NavigationSimulator for an already-started navigation via
  // LoadURL / Reload / GoToOffset / history.GoBack() scripts, etc. Can be used
  // to drive the navigation to completion.
  static std::unique_ptr<NavigationSimulator> CreateFromPending(
      NavigationController& controller);

  virtual ~NavigationSimulator() = default;

  // --------------------------------------------------------------------------

  // The following functions should be used to simulate events happening during
  // a navigation.
  //
  // Example of usage for a successful renderer-initiated navigation:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  //   simulator->Start();
  //   for (GURL redirect_url : redirects)
  //     simulator->Redirect(redirect_url);
  //   simulator->Commit();
  //
  // Example of usage for a failed renderer-initiated navigation:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  //   simulator->Start();
  //   for (GURL redirect_url : redirects)
  //     simulator->Redirect(redirect_url);
  //   simulator->Fail(net::ERR_TIMED_OUT);
  //   simulator->CommitErrorPage();
  //
  // Example of usage for a same-page renderer-initiated navigation:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->CommitSameDocument();
  //
  // Example of usage for a renderer-initiated navigation which is cancelled by
  // a throttle upon redirecting. Note that registering the throttle is done
  // elsewhere:
  //   unique_ptr<NavigationSimulator> simulator =
  //       NavigationSimulator::CreateRendererInitiated(
  //           original_url, render_frame_host);
  //   simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  //   simulator->Start();
  //   simulator->Redirect(redirect_url);
  //   EXPECT_EQ(NavigationThrottle::CANCEL,
  //             simulator->GetLastThrottleCheckResult());

  // Simulates the start of the navigation.
  virtual void Start() = 0;

  // Simulates a redirect to |new_url| for the navigation.
  virtual void Redirect(const GURL& new_url) = 0;

  // Simulates receiving the navigation response and choosing a final
  // RenderFrameHost to commit it.
  virtual void ReadyToCommit() = 0;

  // Simulates the commit of the navigation in the RenderFrameHost.
  virtual void Commit() = 0;

  // Simulates the commit of a navigation or an error page aborting.
  virtual void AbortCommit() = 0;

  // Simulates aborting the navigation from the renderer, e.g. window.stop(),
  // before it was committed in the renderer.
  // Note: this is only valid for renderer-initiated navigations.
  virtual void AbortFromRenderer() = 0;

  // Simulates the navigation failing with the error code |error_code|.
  // IMPORTANT NOTE: This is simulating a network connection error and implies
  // we do not get a response. Error codes like 204 are not properly managed.
  virtual void Fail(int error_code) = 0;

  // Simulates the commit of an error page following a navigation failure.
  virtual void CommitErrorPage() = 0;

  // Simulates the commit of a same-document navigation, ie fragment navigations
  // or pushState/popState navigations.
  virtual void CommitSameDocument() = 0;

  // Must be called after the simulated navigation or an error page has
  // committed. Returns the RenderFrameHost the navigation committed in.
  virtual RenderFrameHost* GetFinalRenderFrameHost() = 0;

  // Only used if AutoAdvance is turned off. Will wait until the current stage
  // of the navigation is complete.
  virtual void Wait() = 0;

  // Returns true if the navigation is deferred waiting for navigation throttles
  // to complete.
  virtual bool IsDeferred() = 0;

  // Returns true if a previous operation has caused the navigation to fail.
  virtual bool HasFailed() = 0;

  // --------------------------------------------------------------------------

  // The following functions are used to specify the parameters of the
  // navigation.

  // The following parameters are constant during the navigation and may only be
  // specified before calling |Start|.
  //
  // Sets the frame that initiated the navigation. Should only be specified for
  // renderer-initiated navigations. For now this frame must belong to the same
  // process as the frame that is navigating.
  //
  // TODO(crbug.com/40127276): Support cross-process initiators here by
  // using NavigationRequest::CreateBrowserInitiated() (like
  // RenderFrameProxyHost does) for the navigation.
  virtual void SetInitiatorFrame(RenderFrameHost* initiator_frame_host) = 0;
  virtual void SetTransition(ui::PageTransition transition) = 0;
  virtual void SetHasUserGesture(bool has_user_gesture) = 0;
  virtual void SetNavigationInputStart(
      base::TimeTicks navigation_input_start) = 0;
  virtual void SetNavigationStart(base::TimeTicks navigation_start) = 0;
  // Note: ReloadType should only be specified for browser-initiated
  // navigations.
  virtual void SetReloadType(ReloadType reload_type) = 0;

  // Sets the HTTP method for the navigation.
  virtual void SetMethod(const std::string& method) = 0;

  // Sets whether this navigation originated as the result of a form submission.
  virtual void SetIsFormSubmission(bool is_form_submission) = 0;

  // The following parameters can change during redirects. They should be
  // specified before calling |Start| if they need to apply to the navigation to
  // the original url. Otherwise, they should be specified before calling
  // |Redirect|.
  virtual void SetReferrer(blink::mojom::ReferrerPtr referrer) = 0;

  // The following parameters can change at any point until the page fails or
  // commits. They should be specified before calling |Fail| or |Commit|.
  virtual void SetSocketAddress(const net::IPEndPoint& remote_endpoint) = 0;

  // Pretend the navigation response is served from cache.
  virtual void SetWasFetchedViaCache(bool was_fetched_via_cache) = 0;

  // Pretend the navigation is against an inner response of a signed exchange.
  virtual void SetIsSignedExchangeInnerResponse(
      bool is_signed_exchange_inner_response) = 0;

  // Simulate receiving Permissions-Policy headers.
  virtual void SetPermissionsPolicyHeader(
      blink::ParsedPermissionsPolicy permissions_policy_header) = 0;

  // Provides the contents mime type to be set at commit. It should be
  // specified before calling |ReadyToCommit| or |Commit|.
  virtual void SetContentsMimeType(const std::string& contents_mime_type) = 0;

  // Provides the response headers that should be received during the next
  // |Redirect| call. These headers will only be applied to the next
  // |Redirect| call, and will be reset afterwards.
  virtual void SetRedirectHeaders(
      scoped_refptr<net::HttpResponseHeaders> redirect_headers) = 0;

  // Provides the response headers received during |ReadyToCommit| specified
  // before calling |ReadyToCommit| or |Commit|.
  // Note that the mime type should be specified separately with
  // |SectContentsMimeType|.
  virtual void SetResponseHeaders(
      scoped_refptr<net::HttpResponseHeaders> response_headers) = 0;

  // Provides the response body received during |ReadyToCommit|. Must be
  // specified before calling |ReadyToCommit| or |Commit|.
  virtual void SetResponseBody(
      mojo::ScopedDataPipeConsumerHandle response_body) = 0;

  // Whether or not the NavigationSimulator automatically advances the
  // navigation past the stage requested (e.g. through asynchronous
  // NavigationThrottles). Defaults to true. Useful for testing throttles which
  // defer the navigation.
  //
  // If the test sets this to false, it should follow up any calls that result
  // in throttles deferring the navigation with a call to Wait().
  virtual void SetAutoAdvance(bool auto_advance) = 0;

  // Sets the ResolveErrorInfo to be set on the URLLoaderCompletionStatus.
  virtual void SetResolveErrorInfo(
      const net::ResolveErrorInfo& resolve_error_info) = 0;

  // Sets the SSLInfo to be set on the response. This should be called before
  // Commit().
  virtual void SetSSLInfo(const net::SSLInfo& ssl_info) = 0;

  // Sets the DNS aliases to be received in the URLResponseHead. The aliases
  // are what would be read from DNS CNAME records, and the alias chain should
  // be preserved in reverse order, from canonical name (i.e. address record
  // name) through to query name. This method should be called before Commit().
  virtual void SetResponseDnsAliases(std::vector<std::string> aliases) = 0;

  // Sets whether preload Link headers were received via Early Hints responses
  // during the navigation.
  virtual void SetEarlyHintsPreloadLinkHeaderReceived(bool received) = 0;

  // --------------------------------------------------------------------------

  // Gets the last throttle check result computed by the navigation throttles.
  // It is an error to call this before Start() is called.
  virtual NavigationThrottle::ThrottleCheckResult
  GetLastThrottleCheckResult() = 0;

  // Returns the NavigationHandle associated with the navigation being
  // simulated. It is an error to call this before Start() or after the
  // navigation has finished (successfully or not).
  virtual NavigationHandle* GetNavigationHandle() = 0;

  // Returns the GlobalRequestID for the simulated navigation request. Can be
  // invoked after the navigation has completed. It is an error to call this
  // before the simulated navigation has completed its WillProcessResponse
  // callback.
  virtual GlobalRequestID GetGlobalRequestID() = 0;

  // By default, committing a navigation will also simulate the load stopping.
  // In the cases where the NavigationSimulator needs to navigate but still be
  // in a loading state, use the functions below.

  // If |keep_loading| is true, maintain the loading state after committing.
  virtual void SetKeepLoading(bool keep_loading) = 0;

  // Simulate the ongoing load stopping successfully.
  virtual void StopLoading() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class NavigationSimulatorImpl;
  NavigationSimulator() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_
