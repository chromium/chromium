// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/safe_ref.h"
#include "base/memory/safety_checks.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_discard_reason.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/restore_type.h"
#include "content/public/common/referrer.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_connection_info.h"
#include "net/ssl/ssl_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-forward.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"
#include "third_party/blink/public/mojom/navigation/renderer_content_settings.mojom-forward.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class GURL;

namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
}  // namespace net

namespace perfetto::protos::pbzero {
class NavigationHandle;
}  // namespace perfetto::protos::pbzero

namespace content {
class CommitDeferringCondition;
struct GlobalRenderFrameHostId;
struct GlobalRequestID;
class NavigationEntry;
class NavigationThrottle;
class NavigationUIData;
class RenderFrameHost;
class SiteInstance;
class WebContents;

// A NavigationHandle tracks information related to a single navigation.
// NavigationHandles are provided to several WebContentsObserver methods to
// allow observers to track specific navigations. Observers should clear any
// references to a NavigationHandle at the time of
// WebContentsObserver::DidFinishNavigation, just before the handle is
// destroyed.
class CONTENT_EXPORT NavigationHandle : public base::SupportsUserData {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  ~NavigationHandle() override = default;

  // Parameters available at navigation start time -----------------------------
  //
  // These parameters are always available during the navigation. Note that
  // some may change during navigation (e.g. due to server redirects).

  // Get a unique ID for this navigation.
  virtual int64_t GetNavigationId() const = 0;

  // Get the page UKM ID that will be in use once this navigation fully commits
  // (typically the eventual value of
  // GetRenderFrameHost()->GetPageUkmSourceId()).
  //
  // WARNING: For prerender activations, this will return a UKM ID that is
  // different from the eventual value of
  // GetRenderFrameHost()->GetPageUkmSourceId(). See
  // https://chromium.googlesource.com/chromium/src/+/main/content/browser/preloading/prerender/README.md#ukm-source-ids
  // for more details.
  virtual ukm::SourceId GetNextPageUkmSourceId() = 0;

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  // This URL may not be the same as the virtual URL returned from
  // WebContents::GetVisibleURL and WebContents::GetLastCommittedURL. For
  // example, viewing a page's source navigates to the URL of the page, but the
  // virtual URL is prefixed with "view-source:".
  // Note: The URL of a NavigationHandle can change over its lifetime.
  // e.g. URLs might be rewritten by the renderer before being committed.
  virtual const GURL& GetURL() = 0;

  // Returns the SiteInstance where the frame being navigated was at the start
  // of the navigation.  If a frame in SiteInstance A navigates a frame in
  // SiteInstance B to a URL in SiteInstance C, then this returns B.
  virtual SiteInstance* GetStartingSiteInstance() = 0;

  // Returns the SiteInstance of the initiator of the navigation.  If a frame in
  // SiteInstance A navigates a frame in SiteInstance B to a URL in SiteInstance
  // C, then this returns A.
  virtual SiteInstance* GetSourceSiteInstance() = 0;

  // Whether the navigation is taking place in a main frame or in a subframe.
  // This can also return true for navigations in the root of a non-primary
  // page, so consider whether you want to call IsInPrimaryMainFrame() instead.
  // See the documentation below for details. The return value remains constant
  // over the navigation lifetime.
  virtual bool IsInMainFrame() const = 0;

  // Whether the navigation is taking place in the main frame of the primary
  // frame tree. With MPArch (crbug.com/1164280), a WebContents may have
  // additional frame trees for prerendering pages in addition to the primary
  // frame tree (holding the page currently shown to the user). The return
  // value remains constant over the navigation lifetime.
  // See docs/frame_trees.md for more details.
  virtual bool IsInPrimaryMainFrame() const = 0;

  // Whether the navigation is taking place in a main frame which does not have
  // an outer document. For example, this will return true for the primary main
  // frame and for a prerendered main frame, but false for a <fencedframe>. See
  // documentation for `RenderFrameHost::GetParentOrOuterDocument()` for more
  // details.
  virtual bool IsInOutermostMainFrame() = 0;

  // Prerender2:
  // Whether the navigation is taking place in the main frame of the
  // prerendered frame tree. Prerender will create separate frame trees to load
  // a page in the background, which later then be activated by a separate
  // prerender page activation navigation in the primary main frame. This
  // returns false for prerender page activation navigations, which should be
  // checked by IsPrerenderedPageActivation(). The return value remains
  // constant over the navigation lifetime.
  virtual bool IsInPrerenderedMainFrame() const = 0;

  // Prerender2:
  // Returns true if this navigation will activate a prerendered page. It is
  // only meaningful to call this after BeginNavigation().
  virtual bool IsPrerenderedPageActivation() const = 0;

  // FencedFrame:
  // Returns true if the navigation is taking place in a frame in a fenced frame
  // tree.
  virtual bool IsInFencedFrameTree() const = 0;

  // Returns the type of the frame in which this navigation is taking place.
  virtual FrameType GetNavigatingFrameType() const = 0;

  // Whether the navigation was initiated by the renderer process. Examples of
  // renderer-initiated navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState() or window.history.replaceState()
  //  * using window.history.forward() or window.history.back()
  //
  // This method returns false for browser-initiated navigations, including:
  //  * any navigation initiated from the omnibox
  //  * navigations via suggestions in browser UI
  //  * navigations via browser UI: Ctrl-R, refresh/forward/back/home buttons
  //  * any other "explicit" URL navigations, e.g. bookmarks
  virtual bool IsRendererInitiated() = 0;

  // The navigation initiator's user activation and ad status.
  virtual blink::mojom::NavigationInitiatorActivationAndAdStatus
  GetNavigationInitiatorActivationAndAdStatus() = 0;

  // Whether the previous document in this frame was same-origin with the new
  // one created by this navigation.
  //
  // |HasCommitted()| must be true before calling this function.
  //
  // Note: This doesn't take the initiator of the navigation into consideration.
  // For instance, a parent (A) can initiate a navigation in its iframe,
  // replacing document (B) by (C). This methods compare (B) with (C).
  virtual bool IsSameOrigin() = 0;

  // Returns the FrameTreeNode ID for the frame in which the navigation is
  // performed. This ID is browser-global and uniquely identifies a frame that
  // hosts content. The return value remains constant over the navigation
  // lifetime.
  //
  // However, because of prerender activations, the RenderFrameHost that this
  // navigation is committed into may later transfer to another FrameTreeNode.
  // See documentation for RenderFrameHost::GetFrameTreeNodeId() for more
  // details.
  virtual FrameTreeNodeId GetFrameTreeNodeId() = 0;

  // Returns the RenderFrameHost for the parent frame, or nullptr if this
  // navigation is taking place in the main frame. This value will not change
  // during a navigation.
  virtual RenderFrameHost* GetParentFrame() = 0;

  // Returns the document owning the frame this NavigationHandle is located
  // in, which will either be a parent (for <iframe>s) or outer document (for
  // <fencedframe>). See documentation for
  // `RenderFrameHost::GetParentOrOuterDocument()` for more details.
  virtual RenderFrameHost* GetParentFrameOrOuterDocument() = 0;

  // The WebContents the navigation is taking place in.
  virtual WebContents* GetWebContents();

  // The time the navigation started, recorded either in the renderer or in the
  // browser process. Corresponds to Navigation Timing API.
  virtual base::TimeTicks NavigationStart() = 0;

  // The time the input leading to the navigation started. Will not be
  // set if unknown.
  virtual base::TimeTicks NavigationInputStart() = 0;

  // The timing information of loading for the navigation.
  virtual const NavigationHandleTiming& GetNavigationHandleTiming() = 0;

  // Whether or not the navigation was started within a context menu.
  virtual bool WasStartedFromContextMenu() = 0;

  // Returns the URL and encoding of an INPUT field that corresponds to a
  // searchable form request.
  virtual const GURL& GetSearchableFormURL() = 0;
  virtual const std::string& GetSearchableFormEncoding() = 0;

  // Returns the reload type for this navigation.
  virtual ReloadType GetReloadType() const = 0;

  // Returns the restore type for this navigation. RestoreType::NONE is returned
  // if the navigation is not a restore.
  virtual RestoreType GetRestoreType() const = 0;

  // Used for specifying a base URL for pages loaded via data URLs.
  virtual const GURL& GetBaseURLForDataURL() = 0;

  // Whether the navigation is done using HTTP POST method. This may change
  // during the navigation (e.g. after encountering a server redirect).
  //
  // Note: page and frame navigations can only be done using HTTP POST or HTTP
  // GET methods (and using other, scheme-specific protocols for non-http(s) URI
  // schemes like data: or file:).  Therefore //content public API exposes only
  // |bool IsPost()| as opposed to |const std::string& GetMethod()| method.
  virtual bool IsPost() = 0;

  // Gets the request method for the initial network request. Unlike `IsPost()`,
  // This will not change during the navigation (e.g. after encountering a
  // server redirect).
  virtual std::string GetRequestMethod() = 0;

  // Returns a sanitized version of the referrer for this request.
  virtual const blink::mojom::Referrer& GetReferrer() = 0;

  // Sets the referrer. The referrer may only be set during start and redirect
  // phases. If the referer is set in navigation start, it is reset during the
  // redirect. In other words, if you need to set a referer that applies to
  // redirects, then this must be called during DidRedirectNavigation().
  virtual void SetReferrer(blink::mojom::ReferrerPtr referrer) = 0;

  // Whether the navigation was initiated by a user gesture. Note that this
  // will return false for browser-initiated navigations.
  // TODO(clamy): This should return true for browser-initiated navigations.
  virtual bool HasUserGesture() = 0;

  // Returns the page transition type.
  virtual ui::PageTransition GetPageTransition() = 0;

  // Returns the NavigationUIData associated with the navigation.
  virtual NavigationUIData* GetNavigationUIData() = 0;

  // Whether the target URL cannot be handled by the browser's internal protocol
  // handlers.
  virtual bool IsExternalProtocol() = 0;

  // Whether the navigation is restoring a page from back-forward cache.
  virtual bool IsServedFromBackForwardCache() = 0;

  // Whether this navigation is activating an existing page (e.g. served from
  // the BackForwardCache or Prerender).
  virtual bool IsPageActivation() const = 0;

  // Navigation control flow --------------------------------------------------

  // The net error code if an error happened prior to commit. Otherwise it will
  // be net::OK.
  virtual net::Error GetNetErrorCode() = 0;

  // Returns the RenderFrameHost this navigation is committing in.  The
  // RenderFrameHost returned will be the final host for the navigation. (Use
  // WebContentsObserver::RenderFrameHostChanged() to observe RenderFrameHost
  // changes that occur during navigation.) This can only be accessed after a
  // response has been delivered for processing, or after the navigation fails
  // with an error page.
  //
  // Note that null will be returned for downloads and/or 204 responses, because
  // they don't commit a new document into a renderer process.
  virtual RenderFrameHost* GetRenderFrameHost() const = 0;

  // Returns the id of the "current RenderFrameHost" before this navigation
  // commits (which would potentially replace the "current RenderFrameHost").
  // In case a navigation happens within the same RenderFrameHost,
  // GetRenderFrameHost() and GetPreviousRenderFrameHostId() will refer to the
  // same RenderFrameHost.
  // Note: The value returned by this function may change over time, e.g. if
  // another navigation committed a different RenderFrameHost during the
  // lifetime of this navigation, causing the "current RenderFrameHost" to
  // change to another RenderFrameHost. The value will only be guaranteed to
  // not change again after the navigation reaches the "ReadyToCommit" stage,
  // as at that point only that navigation can commit, guaranteeing no further
  // changes to the "current RenderFrameHost" until that navigation itself
  // potentially replaces the "current RenderFrameHost".
  // Note 2: Because of the potential "current RenderFrameHost" changes in the
  // middle of this navigation's lifetime, this function should not be assumed
  // to be the value of the "original current RenderFrameHost" (i.e. the current
  // RenderFrameHost value at NavigationHandle construction time). There is
  // currently no way to get that value, but it is tracked internally in
  // `NavigationRequest::current_render_frame_host_id_at_construction_`, so it
  // can potentially be exposed if needed in the future.
  virtual GlobalRenderFrameHostId GetPreviousRenderFrameHostId() = 0;

  // Returns the id of the RenderProcessHost this navigation is expected to
  // commit in. The actual RenderProcessHost may change at commit time. It is
  // only valid to call this before commit.
  virtual int GetExpectedRenderProcessHostId() = 0;

  // Whether the navigation happened without changing document. Examples of
  // same document navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  // * same page history navigation
  virtual bool IsSameDocument() const = 0;

  // Whether the navigation is a history traversal navigation, which navigates
  // to a pre-existing NavigationEntry. Note that this will return false for
  // reloads, and return true for session restore navigations.
  virtual bool IsHistory() const = 0;

  // Whether the navigation has encountered a server redirect or not.
  virtual bool WasServerRedirect() = 0;

  // Lists the redirects that occurred on the way to the current page. The
  // current page is the last one in the list (so even when there's no redirect,
  // there will be one entry in the list).
  virtual const std::vector<GURL>& GetRedirectChain() = 0;

  // Whether the navigation has committed. Navigations that end up being
  // downloads or return 204/205 response codes do not commit (i.e. the
  // WebContents stays at the existing URL).
  // This returns true for either successful commits or error pages that
  // replace the previous page (distinguished by |IsErrorPage|), and false for
  // errors that leave the user on the previous page.
  virtual bool HasCommitted() const = 0;

  // Whether the navigation committed an error page.
  //
  // DO NOT use this before the navigation commit. It would always return false.
  // You can use it from WebContentsObserver::DidFinishNavigation().
  virtual bool IsErrorPage() const = 0;

  // Not all committed subframe navigations (i.e., !IsInMainFrame &&
  // HasCommitted) end up causing a change of the current NavigationEntry. For
  // example, some users of NavigationHandle may want to ignore the initial
  // commit in a newly added subframe or location.replace events in subframes
  // (e.g., ads), while still reacting to user actions like link clicks and
  // back/forward in subframes.  Such users should check if this method returns
  // true before proceeding.
  // Note: it's only valid to call this method for subframes for which
  // HasCommitted returns true.
  virtual bool HasSubframeNavigationEntryCommitted() = 0;

  // True if the committed entry has replaced the existing one. A non-user
  // initiated redirect causes such replacement.
  virtual bool DidReplaceEntry() = 0;

  // Returns true if the browser history should be updated. Otherwise only
  // the session history will be updated. E.g., on unreachable urls or other
  // navigations that the users may not think of as navigations (such as
  // happens with 'history.replaceState()'), or navigations in non-primary frame
  // trees that should not appear in history.
  virtual bool ShouldUpdateHistory() = 0;

  // The previous main frame URL that the user was on. This may be empty if
  // there was no last committed entry. It is only valid to call this for
  // navigations in the primary main frame itself or its subframes.
  virtual const GURL& GetPreviousPrimaryMainFrameURL() = 0;

  // Returns the remote address of the socket which fetched this resource.
  virtual net::IPEndPoint GetSocketAddress() = 0;

  // Returns the headers used for this request.
  virtual const net::HttpRequestHeaders& GetRequestHeaders() = 0;

  // Remove a request's header. If the header is not present, it has no effect.
  // Must be called during a redirect.
  virtual void RemoveRequestHeader(const std::string& header_name) = 0;

  // Set a request's header. If the header is already present, its value is
  // overwritten. When modified during a navigation start, the headers will be
  // applied to the initial network request. When modified during a redirect,
  // the headers will be applied to the redirected request.
  virtual void SetRequestHeader(const std::string& header_name,
                                const std::string& header_value) = 0;

  // Set a request's header that is exempt from CORS checks. This is only
  // honored if the NetworkContext was configured to allow any cors exempt
  // header (see
  // |NetworkContext::mojom::allow_any_cors_exempt_header_for_browser|) or
  // if |header_name| is specified in
  // |NetworkContextParams::cors_exempt_header_list|.
  virtual void SetCorsExemptRequestHeader(const std::string& header_name,
                                          const std::string& header_value) = 0;

  // Set LCP Critical Path Predictor hint data to be passed along to the
  // renderer process on the navigation commit.
  virtual void SetLCPPNavigationHint(
      const blink::mojom::LCPCriticalPathPredictorNavigationTimeHint& hint) = 0;

  // Peek into LCP Critical Path Predictor hint data attached to the navigation.
  virtual const blink::mojom::LCPCriticalPathPredictorNavigationTimeHintPtr&
  GetLCPPNavigationHint() = 0;

  // Returns the response headers for the request, or nullptr if there aren't
  // any response headers or they have not been received yet. The response
  // headers may change during the navigation (e.g. after encountering a server
  // redirect). The headers returned should not be modified, as modifications
  // will not be reflected in the network stack.
  virtual const net::HttpResponseHeaders* GetResponseHeaders() = 0;

  // Returns the connection info for the request, the default value is
  // HttpConnectionInfo::kUNKNOWN if there hasn't been a response (or redirect)
  // yet. The connection info may change during the navigation (e.g. after
  // encountering a server redirect).
  virtual net::HttpConnectionInfo GetConnectionInfo() = 0;

  // Returns the SSLInfo for a request that succeeded or failed due to a
  // certificate error. In the case of other request failures or of a non-secure
  // scheme, returns an empty object.
  virtual const std::optional<net::SSLInfo>& GetSSLInfo() = 0;

  // Returns the AuthChallengeInfo for the request, if the response contained an
  // authentication challenge.
  virtual const std::optional<net::AuthChallengeInfo>&
  GetAuthChallengeInfo() = 0;

  // Returns host resolution error info associated with the request.
  virtual net::ResolveErrorInfo GetResolveErrorInfo() = 0;

  // Gets the net::IsolationInfo associated with the navigation. Updated as
  // redirects are followed. When one of the origins used to construct the
  // IsolationInfo is opaque, the returned IsolationInfo will not be consistent
  // between calls.
  virtual net::IsolationInfo GetIsolationInfo() = 0;

  // Returns the ID of the URLRequest associated with this navigation. Can only
  // be called from NavigationThrottle::WillProcessResponse and
  // WebContentsObserver::ReadyToCommitNavigation.
  // In the case of transfer navigations, this is the ID of the first request
  // made. The transferred request's ID will not be tracked by the
  // NavigationHandle.
  virtual const GlobalRequestID& GetGlobalRequestID() = 0;

  // Returns true if this navigation resulted in a download. Returns false if
  // this navigation did not result in a download, or if download status is not
  // yet known for this navigation.  Download status is determined for a
  // navigation when processing final (post redirect) HTTP response headers.
  virtual bool IsDownload() = 0;

  // Returns true if this navigation was initiated by a form submission.
  virtual bool IsFormSubmission() = 0;

  // Returns true if this navigation was initiated by a link click.
  virtual bool WasInitiatedByLinkClick() = 0;

  // Returns true if the target is an inner response of a signed exchange.
  virtual bool IsSignedExchangeInnerResponse() = 0;

  // Returns true if prefetched alternative subresource signed exchange was sent
  // to the renderer process.
  virtual bool HasPrefetchedAlternativeSubresourceSignedExchange() = 0;

  // Returns true if the navigation response was cached.
  virtual bool WasResponseCached() = 0;

  // Returns the value of the hrefTranslate attribute if this navigation was
  // initiated from a link that had that attribute set.
  virtual const std::string& GetHrefTranslate() = 0;

  // Returns, if available, the impression associated with the link clicked to
  // initiate this navigation. The impression is available for the entire
  // lifetime of the navigation.
  virtual const std::optional<blink::Impression>& GetImpression() = 0;

  // Returns the frame token associated with the frame that initiated the
  // navigation. This can be nullptr if the navigation was not associated with a
  // frame, or may return a valid frame token to a frame that no longer exists
  // because it was deleted before the navigation began. This parameter is
  // defined if and only if GetInitiatorProcessId below is.
  virtual const std::optional<blink::LocalFrameToken>&
  GetInitiatorFrameToken() = 0;

  // Return the ID of the renderer process of the frame host that initiated the
  // navigation. This is defined if and only if GetInitiatorFrameToken above is,
  // and it is only valid in conjunction with it.
  virtual int GetInitiatorProcessId() = 0;

  // Returns, if available, the origin of the document that has initiated the
  // navigation for this NavigationHandle.
  // NOTE: If this is a history navigation, the initiator origin will be the
  // origin that initiated the *original* navigation, not the history
  // navigation. This means that if there was no initiator origin for the
  // original navigation, but the history navigation was initiated by
  // javascript, the initiator origin will be null even though
  // IsRendererInitiated() returns true.
  virtual const std::optional<url::Origin>& GetInitiatorOrigin() = 0;

  // Returns, for renderer-initiated about:blank and about:srcdoc navigations,
  // the base url of the document that has initiated the navigation for this
  // NavigationHandle. The same caveats apply here as for GetInitiatorOrigin().
  virtual const std::optional<GURL>& GetInitiatorBaseUrl() = 0;

  // Retrieves any DNS aliases for the requested URL. Includes all known
  // aliases, e.g. from A, AAAA, or HTTPS, not just from the address used for
  // the connection, in no particular order.
  virtual const std::vector<std::string>& GetDnsAliases() = 0;

  // Whether the new document will be hosted in the same process as the current
  // document or not. Set only when the navigation commits.
  virtual bool IsSameProcess() = 0;

  // Returns the NavigationEntry associated with this, which may be null.
  virtual NavigationEntry* GetNavigationEntry() const = 0;

  // Returns the offset between the indices of the previous last committed and
  // the newly committed navigation entries.
  // (e.g. -1 for back navigations, 0 for reloads, 1 for forward navigations).
  //
  // Note that this value is computed when we create the navigation request
  // and doesn't fully cover all corner cases.
  // We try to approximate them with params.should_replace_entry, but in
  // some cases it's inaccurate:
  // - Main frame client redirects,
  // - History navigation to the page with subframes. The subframe
  //   navigations will return 1 here although they don't create a new
  //   navigation entry.
  virtual int GetNavigationEntryOffset() = 0;

  virtual void RegisterSubresourceOverride(
      blink::mojom::TransferrableURLLoaderPtr transferrable_loader) = 0;

  // Force enables the given origin trials for this navigation. This needs to
  // be called from WebContents::ReadyToCommitNavigation or earlier to have an
  // effect.
  virtual void ForceEnableOriginTrials(
      const std::vector<std::string>& trials) = 0;

  // Store whether or not we're overriding the user agent. This may only be
  // called from DidStartNavigation().
  virtual void SetIsOverridingUserAgent(bool override_ua) = 0;

  // Suppress any errors during a navigation and behave as if the user cancelled
  // the navigation: no error page will commit.
  virtual void SetSilentlyIgnoreErrors() = 0;

  // The :visited link hashtable is stored in shared memory and contains salted
  // hashes for all visits. Each salt corresponds to a unique origin, and
  // renderer processes are only informed of salts that correspond to their
  // origins. As a result, any given renderer process can only
  // learn about visits relevant to origins for which it has the salt.
  //
  // Here we store the salt corresponding to this navigation's origin to
  // be committed. It will allow the renderer process that commits this
  // navigation to learn about visits hashed with this salt. Setting a salt
  // value is optional - `commit_params` is constructed with a std::nullopt
  // default value. In these cases, VisitedLinkWriter is responsible for
  // sending salt values to the renderer after the :visited link hashtable has
  // been initialized.
  virtual void SetVisitedLinkSalt(uint64_t salt) = 0;

  // The sandbox flags of the initiator of the navigation, if any.
  // WebSandboxFlags::kNone otherwise.
  virtual network::mojom::WebSandboxFlags SandboxFlagsInitiator() = 0;

  // The sandbox flags inherited at the beginning of the navigation.
  //
  // This is the sandbox flags intersection of:
  // - The parent document.
  // - The iframe.sandbox attribute.
  //
  // Contrary to `SandboxFlagsToCommit()`, this can be called at the beginning
  // of the navigation. However, this doesn't include the sandbox flags a
  // document applies on itself, via the "Content-Security-Policy: sandbox"
  // response header.
  //
  // See also: content/browser/renderer_host/sandbox_flags.md
  virtual network::mojom::WebSandboxFlags SandboxFlagsInherited() = 0;

  // The sandbox flags of the new document created by this navigation. This
  // function can only be called for cross-document navigations after receiving
  // the final response.
  // See also: content/browser/renderer_host/sandbox_flags.md
  //
  // TODO(arthursonzogni): After RenderDocument, this can be computed and stored
  // directly into the RenderDocumentHost.
  virtual network::mojom::WebSandboxFlags SandboxFlagsToCommit() = 0;

  // Whether the navigation was sent to be committed in a renderer by the
  // RenderFrameHost. This can either be for the commit of a successful
  // navigation or an error page.
  virtual bool IsWaitingToCommit() = 0;

  // Returns true when at least one preload or preconnect Link header was
  // received via an Early Hints response during this navigation. True only for
  // a main frame navigation.
  virtual bool WasResourceHintsReceived() = 0;

  // Whether this navigation is for PDF content in a PDF-specific renderer.
  virtual bool IsPdf() = 0;

  using TraceProto = perfetto::protos::pbzero::NavigationHandle;
  // Write a representation of this object into a trace.
  virtual void WriteIntoTrace(
      perfetto::TracedProto<TraceProto> context) const = 0;

  // Sets an overall request timeout for this navigation, which will cause the
  // navigation to fail if it expires before the navigation commits. This is
  // separate from any //net level timeouts. This can only be set at the
  // NavigationThrottle::WillRedirectRequest() stage of the navigation. Returns
  // `true` if the timeout is being started for the first time. Repeated calls
  // will be ignored (they won't reset the timeout) and will return `false`.
  virtual bool SetNavigationTimeout(base::TimeDelta timeout) = 0;
  // Cancels the request timeout for this navigation. If the navigation is still
  // happening, it will continue as if the timer wasn't set. Otherwise, this is
  // a no-op.
  virtual void CancelNavigationTimeout() = 0;

  // Configures whether a Cookie header added to this request should not be
  // overwritten by the network service.
  virtual void SetAllowCookiesFromBrowser(bool allow_cookies_from_browser) = 0;

  // Returns the contents of the response body via callback.
  //
  // This method should only be called by NavigationThrottle implementations.
  // When calling this method, the NavigationThrottle should either already be
  // deferred or be processing and about to be deferred.
  //
  // The callback may be called with an empty response body if:
  // - The NavigationThrottle resumes before the response body is read
  // - An unhandled MojoResult is encountered while reading the response body in
  //   `NavigationRequest::OnResponseBodyReady()`
  //
  // The response body is read from the data pipe using MOJO_READ_DATA_FLAG_PEEK
  // so that the body is not consumed before reaching its intended target.
  //
  // Only the first response body data that is read from the data pipe will be
  // passed into the callback.
  using ResponseBodyCallback =
      base::OnceCallback<void(const std::string& initial_body_chunk)>;
  virtual void GetResponseBody(ResponseBodyCallback callback) = 0;

  // Prerender2:
  // Used for metrics.
  virtual PreloadingTriggerType GetPrerenderTriggerType() = 0;
  virtual std::string GetPrerenderEmbedderHistogramSuffix() = 0;

  // Returns a SafeRef to this handle.
  virtual base::SafeRef<NavigationHandle> GetSafeRef() = 0;

  // Will calculate the origin that this NavigationRequest will commit. (This
  // should be reasonably accurate, but some browser-vs-renderer inconsistencies
  // might still exist - they are currently tracked in
  // https://crbug.com/1220238).
  //
  // Returns `nullopt` if the navigation will not commit (e.g. in case of
  // downloads, or 204 responses).  This may happen if and only if
  // `NavigationHandle::GetRenderFrameHost` returns null.
  //
  // This method may only be called after a response has been delivered for
  // processing, or after the navigation fails with an error page, because the
  // return value depends on headers in the HTTP response (e.g., a CSP sandbox
  // header may cause the origin to be opaque).
  virtual std::optional<url::Origin> GetOriginToCommit() = 0;

  // Testing methods ----------------------------------------------------------
  //
  // The following methods should be used exclusively for writing unit tests.

  // Registers a NavigationThrottle for tests. The throttle can
  // modify the request, pause the request or cancel the request. This will
  // take ownership of the NavigationThrottle.
  // Note: in non-test cases, NavigationThrottles should not be added directly
  // but returned by the implementation of
  // ContentBrowserClient::CreateThrottlesForNavigation. This ensures proper
  // ordering of the throttles.
  virtual void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle) = 0;

  // Returns whether this navigation is currently deferred.
  virtual bool IsDeferredForTesting() = 0;
  virtual bool IsCommitDeferringConditionDeferredForTesting() = 0;

#if BUILDFLAG(IS_ANDROID)
  // Returns a reference to NavigationHandle Java counterpart.
  virtual const base::android::JavaRef<jobject>& GetJavaNavigationHandle() = 0;
#endif

  // Returns the CommitDeferringCondition that is currently preventing this
  // navigation from committing, or nullptr if the navigation isn't currently
  // blocked on a CommitDeferringCondition.
  virtual CommitDeferringCondition* GetCommitDeferringConditionForTesting() = 0;

  // Returns true if the navigation is a reload due to the existing document
  // represented by the FrameTreeNode being previously discarded by the browser.
  // This can be used as soon as the navigation begins.
  virtual bool ExistingDocumentWasDiscarded() const = 0;

  // Returns a mutable reference to a blink::RuntimeFeatureStateContext object,
  // which exposes the getters and setters for Blink Runtime-Enabled Features to
  // the browser process. Any feature set using the RuntimeFeatureStateContext
  // before navigation commit will be communicated back to the renderer process.
  //
  // This function should be used from
  // `WebContentsObserver::ReadyToCommitNavigation()` or earlier. It cannot be
  // called after `READY_TO_COMMIT`.
  //
  // NOTE: These feature changes will apply to the "to-be-created" document and
  // cleared on redirects i.e. any RFSC's alterations prior to the final URL
  // will be lost.
  virtual blink::RuntimeFeatureStateContext&
  GetMutableRuntimeFeatureStateContext() = 0;

  // Some content settings must be enforced by the renderer (e.g. whether
  // running javascript is allowed). See ContentSettingsType for more details.
  virtual void SetContentSettings(
      blink::mojom::RendererContentSettingsPtr content_settings) = 0;

  // Makes a copy of the content settings.
  virtual blink::mojom::RendererContentSettingsPtr
  GetContentSettingsForTesting() = 0;

  // Allows the embedder to mark whether this navigation handle is being used
  // for advertising purposes. This is expected to be best-effort, and may be
  // inaccurate. Notably, this defers from the status from
  // `GetNavigationInitiatorActivationAndAdStatus()` as it can include other
  // signals outside of the initiator.
  virtual void SetIsAdTagged() = 0;

  // If the navigation is discarded without committing, returns the reason for
  // the discarding. See `NavigationDiscardReason` for the various cases.
  virtual std::optional<NavigationDiscardReason>
  GetNavigationDiscardReason() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
