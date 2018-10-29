// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "net/base/host_port_pair.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

class FrameTreeNode;
class MockNavigationClientImpl;
class NavigationHandle;
class NavigationHandleImpl;
class NavigationRequest;
class RenderFrameHost;
class TestRenderFrameHost;
struct Referrer;

namespace mojom {
class NavigationClient;
}

// An interface for simulating a navigation in unit tests. Supports both
// renderer and browser-initiated navigations.
// Note: this should not be used in browser tests.
class NavigationSimulator : public WebContentsObserver {
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

  // TODO(clamy, ahemery): Add GoForwardAndFail() if it becomes needed.

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
  // E.g. offset -1 for back navigations and 1 for forward navigations.
  static std::unique_ptr<NavigationSimulator> CreateHistoryNavigation(
      int offset,
      WebContents* web_contents);

  // Creates a NavigationSimulator that will be used to simulate a
  // renderer-initiated navigation to |original_url| started by
  // |render_frame_host|.
  static std::unique_ptr<NavigationSimulator> CreateRendererInitiated(
      const GURL& original_url,
      RenderFrameHost* render_frame_host);

  // Creates a NavigationSimulator for an already-started browser initiated
  // navigation via LoadURL / Reload / GoToOffset. Can be used to drive the
  // navigation to completion.
  static std::unique_ptr<NavigationSimulator> CreateFromPendingBrowserInitiated(
      WebContents* contents);

  ~NavigationSimulator() override;

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
  virtual void Start();

  // Simulates a redirect to |new_url| for the navigation.
  virtual void Redirect(const GURL& new_url);

  // Simulates receiving the navigation response and choosing a final
  // RenderFrameHost to commit it.
  virtual void ReadyToCommit();

  // Simulates the commit of the navigation in the RenderFrameHost.
  virtual void Commit();

  // Simulates the commit of a navigation or an error page aborting.
  virtual void AbortCommit();

  // Simulates the navigation failing with the error code |error_code| and
  // response headers |response_headers|.
  virtual void FailWithResponseHeaders(
      int error_code,
      scoped_refptr<net::HttpResponseHeaders> response_headers);

  // Simulates the navigation failing with the error code |error_code|.
  virtual void Fail(int error_code);

  // Simulates the commit of an error page following a navigation failure.
  virtual void CommitErrorPage();

  // Simulates the commit of a same-document navigation, ie fragment navigations
  // or pushState/popState navigations.
  virtual void CommitSameDocument();

  // Must be called after the simulated navigation or an error page has
  // committed. Returns the RenderFrameHost the navigation committed in.
  virtual RenderFrameHost* GetFinalRenderFrameHost();

  // Only used if AutoAdvance is turned off. Will wait until the current stage
  // of the navigation is complete.
  void Wait();

  // Returns true if the navigation is deferred waiting for navigation throttles
  // to complete.
  bool IsDeferred();

  // --------------------------------------------------------------------------

  // The following functions are used to specify the parameters of the
  // navigation.

  // The following parameters are constant during the navigation and may only be
  // specified before calling |Start|.
  virtual void SetTransition(ui::PageTransition transition);
  virtual void SetHasUserGesture(bool has_user_gesture);
  // Note: ReloadType should only be specified for browser-initiated
  // navigations.
  void SetReloadType(ReloadType reload_type);

  // Sets the HTTP method for the navigation.
  void SetMethod(const std::string& method);

  // The following parameters can change during redirects. They should be
  // specified before calling |Start| if they need to apply to the navigation to
  // the original url. Otherwise, they should be specified before calling
  // |Redirect|.
  virtual void SetReferrer(const Referrer& referrer);

  // The following parameters can change at any point until the page fails or
  // commits. They should be specified before calling |Fail| or |Commit|.
  virtual void SetSocketAddress(const net::HostPortPair& socket_address);

  // Pretend the navigation is against an inner response of a signed exchange.
  void SetIsSignedExchangeInnerResponse(bool is_signed_exchange_inner_response);

  // Sets the InterfaceProvider interface request to pass in as an argument to
  // DidCommitProvisionalLoad for cross-document navigations. If not called,
  // a stub will be passed in (which will never receive any interface requests).
  //
  // This interface connection would normally be created by the RenderFrame,
  // with the client end bound to |remote_interfaces_| to allow the new document
  // to access services exposed by the RenderFrameHost.
  virtual void SetInterfaceProviderRequest(
      service_manager::mojom::InterfaceProviderRequest request);

  // Provides the contents mime type to be set at commit. It should be
  // specified before calling |Commit|.
  virtual void SetContentsMimeType(const std::string& contents_mime_type);

  // Whether or not the NavigationSimulator automatically advances the
  // navigation past the stage requested (e.g. through asynchronous
  // NavigationThrottles). Defaults to true. Useful for testing throttles which
  // defer the navigation.
  //
  // If the test sets this to false, it should follow up any calls that result
  // in throttles deferring the navigation with a call to Wait().
  virtual void SetAutoAdvance(bool auto_advance);

  // --------------------------------------------------------------------------

  // Gets the last throttle check result computed by the navigation throttles.
  // It is an error to call this before Start() is called.
  virtual NavigationThrottle::ThrottleCheckResult GetLastThrottleCheckResult();

  // Returns the NavigationHandle associated with the navigation being
  // simulated. It is an error to call this before Start() or after the
  // navigation has finished (successfully or not).
  virtual NavigationHandle* GetNavigationHandle() const;

  // Returns the GlobalRequestID for the simulated navigation request. Can be
  // invoked after the navigation has completed. It is an error to call this
  // before the simulated navigation has completed its WillProcessResponse
  // callback.
  content::GlobalRequestID GetGlobalRequestID() const;

 private:
  NavigationSimulator(const GURL& original_url,
                      bool browser_initiated,
                      WebContentsImpl* web_contents,
                      TestRenderFrameHost* render_frame_host);

  // Adds a test navigation throttle to |handle| which sanity checks various
  // callbacks have been properly called.
  void RegisterTestThrottle(NavigationHandle* handle);

  // Initializes a NavigationSimulator from an existing NavigationRequest. This
  // should only be needed if a navigation was started without a valid
  // NavigationSimulator.
  void InitializeFromStartedRequest(NavigationRequest* request);

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void StartComplete();
  void RedirectComplete(int previous_num_will_redirect_request_called,
                        int previous_did_redirect_navigation_called);
  void ReadyToCommitComplete(bool ran_throttles);
  void FailComplete(int error_code);

  void OnWillStartRequest();
  void OnWillRedirectRequest();
  void OnWillFailRequest();
  void OnWillProcessResponse();

  // Simulates a browser-initiated navigation starting. Returns false if the
  // navigation failed synchronously.
  bool SimulateBrowserInitiatedStart();

  // Simulates a renderer-initiated navigation starting. Returns false if the
  // navigation failed synchronously.
  bool SimulateRendererInitiatedStart();

  // This method will block waiting for throttle checks to complete if
  // |auto_advance_|. Otherwise will just set up state for checking the result
  // when the throttles end up finishing.
  void MaybeWaitForThrottleChecksComplete(base::OnceClosure complete_closure);

  // Sets |last_throttle_check_result_| and calls both the
  // |wait_closure_| and the |throttle_checks_complete_closure_|, if they are
  // set.
  void OnThrottleChecksComplete(NavigationThrottle::ThrottleCheckResult result);

  // Helper method to set the OnThrottleChecksComplete callback on the
  // NavigationHandle.
  void PrepareCompleteCallbackOnHandle();

  // Check if the navigation corresponds to a same-document navigation.
  // Only use on renderer-initiated navigations.
  bool CheckIfSameDocument();

  // Infers from internal parameters whether the navigation created a new
  // entry.
  bool DidCreateNewEntry();

  // Set the navigation to be done towards the specified navigation controller
  // offset. Typically -1 for back navigations or 1 for forward navigations.
  void SetSessionHistoryOffset(int offset);

  // Only used when PerNavigationMojoInterface is enabled.
  void StoreNavigationClientRequest(
      mojo::AssociatedInterfaceRequest<mojom::NavigationClient>
          navigation_client_request);

  enum State {
    INITIALIZATION,
    STARTED,
    READY_TO_COMMIT,
    FAILED,
    FINISHED,
  };

  State state_ = INITIALIZATION;

  // The WebContents in which the navigation is taking place.
  WebContentsImpl* web_contents_;

  // The renderer associated with this navigation.
  // Note: this can initially be null for browser-initiated navigations.
  TestRenderFrameHost* render_frame_host_;

  FrameTreeNode* frame_tree_node_;

  // The NavigationHandle associated with this navigation.
  NavigationHandleImpl* handle_;

  // Note: additional parameters to modify the navigation should be properly
  // initialized (if needed) in InitializeFromStartedRequest.
  GURL navigation_url_;
  net::HostPortPair socket_address_;
  bool is_signed_exchange_inner_response_ = false;
  std::string initial_method_;
  bool browser_initiated_;
  bool same_document_ = false;
  Referrer referrer_;
  ui::PageTransition transition_;
  ReloadType reload_type_ = ReloadType::NONE;
  int session_history_offset_ = 0;
  bool has_user_gesture_ = true;
  service_manager::mojom::InterfaceProviderRequest interface_provider_request_;
  std::string contents_mime_type_;

  bool auto_advance_ = true;

  // These are used to sanity check the content/public/ API calls emitted as
  // part of the navigation.
  int num_did_start_navigation_called_ = 0;
  int num_will_start_request_called_ = 0;
  int num_will_redirect_request_called_ = 0;
  int num_will_fail_request_called_ = 0;
  int num_did_redirect_navigation_called_ = 0;
  int num_will_process_response_called_ = 0;
  int num_ready_to_commit_called_ = 0;
  int num_did_finish_navigation_called_ = 0;

  // Holds the last ThrottleCheckResult calculated by the navigation's
  // throttles. Will be unset before WillStartRequest is finished. Will be unset
  // while throttles are being run, but before they finish.
  base::Optional<NavigationThrottle::ThrottleCheckResult>
      last_throttle_check_result_;

  // GlobalRequestID for the associated NavigationHandle. Only valid after
  // WillProcessResponse has been invoked on the NavigationHandle.
  content::GlobalRequestID request_id_;

  // Closure that is set when MaybeWaitForThrottleChecksComplete is called.
  // Called in OnThrottleChecksComplete.
  base::OnceClosure throttle_checks_complete_closure_;

  // Closure that is called in OnThrottleChecksComplete if we are waiting on the
  // result. Calling this will quit the nested run loop.
  base::OnceClosure wait_closure_;

  // A mock NavigationClient implementation that is used because we do not
  // actually have a renderer. The navigations would be instantly aborted if
  // this was not kept alive.
  // Only used when PerNavigationMojoInterface is enabled.
  std::unique_ptr<MockNavigationClientImpl> navigation_client_impl_;

  base::WeakPtrFactory<NavigationSimulator> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NAVIGATION_SIMULATOR_H_
