// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_HANDLE_H_
#define CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_HANDLE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/types/optional_util.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_request_headers.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/mojom/navigation/renderer_content_settings.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"
#include "url/gurl.h"

namespace content {

class MockNavigationHandle : public NavigationHandle {
 public:
  MockNavigationHandle();
  explicit MockNavigationHandle(WebContents* web_contents);
  MockNavigationHandle(const GURL& url, RenderFrameHost* render_frame_host);
  ~MockNavigationHandle() override;

  // NavigationHandle implementation:
  int64_t GetNavigationId() const override { return navigation_id_; }
  ukm::SourceId GetNextPageUkmSourceId() override {
    return ukm::ConvertToSourceId(navigation_id_,
                                  ukm::SourceIdObj::Type::NAVIGATION_ID);
  }
  const GURL& GetURL() override { return url_; }
  const GURL& GetPreviousPrimaryMainFrameURL() override {
    return previous_primary_main_frame_url_;
  }
  SiteInstance* GetStartingSiteInstance() override {
    return starting_site_instance_;
  }
  SiteInstance* GetSourceSiteInstance() override {
    return source_site_instance_;
  }
  bool IsInMainFrame() const override {
    return render_frame_host_ ? !render_frame_host_->GetParent() : true;
  }
  MOCK_CONST_METHOD0(IsInPrerenderedMainFrame, bool());
  bool IsPrerenderedPageActivation() const override {
    return is_prerendered_page_activation_;
  }
  bool IsInFencedFrameTree() const override { return is_in_fenced_frame_tree_; }
  FrameType GetNavigatingFrameType() const override {
    NOTIMPLEMENTED();
    return FrameType::kPrimaryMainFrame;
  }
  // By default, MockNavigationHandles are renderer-initiated navigations.
  bool IsRendererInitiated() override { return is_renderer_initiated_; }
  blink::mojom::NavigationInitiatorActivationAndAdStatus
  GetNavigationInitiatorActivationAndAdStatus() override {
    return blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kDidNotStartWithTransientActivation;
  }
  bool IsSameOrigin() override {
    NOTIMPLEMENTED();
    return false;
  }
  bool IsInPrimaryMainFrame() const override {
    return is_in_primary_main_frame_;
  }
  bool IsInOutermostMainFrame() override {
    return !GetParentFrameOrOuterDocument();
  }
  content::FrameTreeNodeId GetFrameTreeNodeId() override {
    if (IsInPrimaryMainFrame()) {
      CHECK(web_contents_);
      return web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId();
    }
    CHECK(render_frame_host_);
    return render_frame_host_->GetFrameTreeNodeId();
  }
  MOCK_METHOD0(GetPreviousRenderFrameHostId, GlobalRenderFrameHostId());
  MOCK_METHOD(int, GetExpectedRenderProcessHostId, ());
  bool IsServedFromBackForwardCache() override {
    return is_served_from_bfcache_;
  }
  bool IsPageActivation() const override {
    MockNavigationHandle* handle = const_cast<MockNavigationHandle*>(this);
    return handle->IsPrerenderedPageActivation() ||
           handle->IsServedFromBackForwardCache();
  }
  RenderFrameHost* GetParentFrame() override {
    return render_frame_host_ ? render_frame_host_->GetParent() : nullptr;
  }
  RenderFrameHost* GetParentFrameOrOuterDocument() override {
    return render_frame_host_ ? render_frame_host_->GetParentOrOuterDocument()
                              : nullptr;
  }
  WebContents* GetWebContents() override { return web_contents_; }
  MOCK_METHOD0(NavigationStart, base::TimeTicks());
  MOCK_METHOD0(NavigationInputStart, base::TimeTicks());
  MOCK_METHOD0(GetNavigationHandleTiming, const NavigationHandleTiming&());
  bool WasStartedFromContextMenu() override {
    return was_started_from_context_menu_;
  }
  MOCK_METHOD0(GetSearchableFormURL, const GURL&());
  MOCK_METHOD0(GetSearchableFormEncoding, const std::string&());
  ReloadType GetReloadType() const override { return reload_type_; }
  RestoreType GetRestoreType() const override {
    return RestoreType::kNotRestored;
  }
  const GURL& GetBaseURLForDataURL() override { return base_url_for_data_url_; }
  MOCK_METHOD0(IsPost, bool());
  MOCK_METHOD0(GetRequestMethod, std::string());
  const blink::mojom::Referrer& GetReferrer() override { return referrer_; }
  void SetReferrer(blink::mojom::ReferrerPtr referrer) override {
    referrer_ = *referrer;
  }
  MOCK_METHOD0(HasUserGesture, bool());
  ui::PageTransition GetPageTransition() override { return page_transition_; }
  MOCK_METHOD0(GetNavigationUIData, NavigationUIData*());
  MOCK_METHOD0(IsExternalProtocol, bool());
  net::Error GetNetErrorCode() override { return net_error_code_; }
  RenderFrameHost* GetRenderFrameHost() const override {
    return render_frame_host_;
  }
  bool IsSameDocument() const override { return is_same_document_; }
  bool IsHistory() const override {
    NOTIMPLEMENTED();
    return false;
  }
  MOCK_METHOD0(WasServerRedirect, bool());
  const std::vector<GURL>& GetRedirectChain() override {
    return redirect_chain_;
  }
  bool HasCommitted() const override { return has_committed_; }
  bool IsErrorPage() const override { return is_error_page_; }
  MOCK_METHOD0(HasSubframeNavigationEntryCommitted, bool());
  MOCK_METHOD0(DidReplaceEntry, bool());
  MOCK_METHOD0(ShouldUpdateHistory, bool());
  MOCK_METHOD0(GetSocketAddress, net::IPEndPoint());
  const net::HttpRequestHeaders& GetRequestHeaders() override {
    return request_headers_;
  }
  MOCK_METHOD1(RemoveRequestHeader, void(const std::string&));
  MOCK_METHOD2(SetRequestHeader, void(const std::string&, const std::string&));
  MOCK_METHOD2(SetCorsExemptRequestHeader,
               void(const std::string&, const std::string&));
  const net::HttpResponseHeaders* GetResponseHeaders() override {
    return response_headers_.get();
  }
  MOCK_METHOD1(
      SetLCPPNavigationHint,
      void(const blink::mojom::LCPCriticalPathPredictorNavigationTimeHint&));
  MOCK_METHOD0(
      GetLCPPNavigationHint,
      const blink::mojom::LCPCriticalPathPredictorNavigationTimeHintPtr&());
  MOCK_METHOD0(GetConnectionInfo, net::HttpConnectionInfo());
  const std::optional<net::SSLInfo>& GetSSLInfo() override { return ssl_info_; }
  const std::optional<net::AuthChallengeInfo>& GetAuthChallengeInfo() override {
    return auth_challenge_info_;
  }
  void SetAuthChallengeInfo(const net::AuthChallengeInfo& challenge);
  net::ResolveErrorInfo GetResolveErrorInfo() override {
    return resolve_error_info_;
  }
  MOCK_METHOD0(GetIsolationInfo, net::IsolationInfo());
  const GlobalRequestID& GetGlobalRequestID() override {
    return global_request_id_;
  }
  MOCK_METHOD0(IsDownload, bool());
  bool IsFormSubmission() override { return is_form_submission_; }
  MOCK_METHOD0(WasInitiatedByLinkClick, bool());
  MOCK_METHOD0(IsSignedExchangeInnerResponse, bool());
  MOCK_METHOD0(HasPrefetchedAlternativeSubresourceSignedExchange, bool());
  bool WasResponseCached() override { return was_response_cached_; }
  const std::string& GetHrefTranslate() override { return href_translate_; }
  const std::optional<blink::Impression>& GetImpression() override {
    return impression_;
  }
  const std::optional<blink::LocalFrameToken>& GetInitiatorFrameToken()
      override {
    return initiator_frame_token_;
  }
  int GetInitiatorProcessId() override { return initiator_process_id_; }
  const std::optional<url::Origin>& GetInitiatorOrigin() override {
    return initiator_origin_;
  }
  const std::optional<GURL>& GetInitiatorBaseUrl() override {
    return initiator_base_url_;
  }
  const std::vector<std::string>& GetDnsAliases() override {
    static const base::NoDestructor<std::vector<std::string>>
        emptyvector_result;
    return *emptyvector_result;
  }
  MOCK_METHOD(void,
              RegisterThrottleForTesting,
              (std::unique_ptr<NavigationThrottle>));
  MOCK_METHOD(bool, IsDeferredForTesting, ());
  MOCK_METHOD(bool, IsCommitDeferringConditionDeferredForTesting, ());
  MOCK_METHOD(void,
              RegisterSubresourceOverride,
              (blink::mojom::TransferrableURLLoaderPtr));
  MOCK_METHOD(bool, IsSameProcess, ());
  MOCK_METHOD(NavigationEntry*, GetNavigationEntry, (), (const, override));
  MOCK_METHOD(int, GetNavigationEntryOffset, ());
  MOCK_METHOD(void,
              ForceEnableOriginTrials,
              (const std::vector<std::string>& trials));
  MOCK_METHOD(void, SetIsOverridingUserAgent, (bool));
  MOCK_METHOD(void, SetSilentlyIgnoreErrors, ());
  MOCK_METHOD(void, SetVisitedLinkSalt, (uint64_t));
  MOCK_METHOD(network::mojom::WebSandboxFlags, SandboxFlagsInitiator, ());
  MOCK_METHOD(network::mojom::WebSandboxFlags, SandboxFlagsInherited, ());
  MOCK_METHOD(network::mojom::WebSandboxFlags, SandboxFlagsToCommit, ());
  MOCK_METHOD(bool, IsWaitingToCommit, ());
  MOCK_METHOD(bool, WasResourceHintsReceived, ());
  MOCK_METHOD(bool, IsPdf, ());
  void WriteIntoTrace(perfetto::TracedProto<TraceProto>) const override {}
  MOCK_METHOD(bool, SetNavigationTimeout, (base::TimeDelta));
  MOCK_METHOD(void, CancelNavigationTimeout, ());
  MOCK_METHOD(PreloadingTriggerType, GetPrerenderTriggerType, ());
  MOCK_METHOD(std::string, GetPrerenderEmbedderHistogramSuffix, ());
  MOCK_METHOD(void, SetAllowCookiesFromBrowser, (bool));
  MOCK_METHOD(void, GetResponseBody, (ResponseBodyCallback));
  MOCK_METHOD(std::optional<NavigationDiscardReason>,
              GetNavigationDiscardReason,
              ());

#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(const base::android::JavaRef<jobject>&,
              GetJavaNavigationHandle,
              ());
#endif

  base::SafeRef<NavigationHandle> GetSafeRef() override {
    return weak_factory_.GetSafeRef();
  }
  MOCK_METHOD(bool, ExistingDocumentWasDiscarded, (), (const));

  CommitDeferringCondition* GetCommitDeferringConditionForTesting() override {
    return nullptr;
  }

  void SetContentSettings(
      blink::mojom::RendererContentSettingsPtr content_settings) override {}
  blink::mojom::RendererContentSettingsPtr GetContentSettingsForTesting()
      override {
    return nullptr;
  }
  MOCK_METHOD(void, SetIsAdTagged, ());

  blink::RuntimeFeatureStateContext& GetMutableRuntimeFeatureStateContext()
      override {
    return runtime_feature_state_context_;
  }
  MOCK_METHOD(std::optional<url::Origin>, GetOriginToCommit, ());
  // End of NavigationHandle implementation.

  void set_url(const GURL& url) { url_ = url; }
  void set_previous_primary_main_frame_url(
      const GURL& previous_primary_main_frame_url) {
    previous_primary_main_frame_url_ = previous_primary_main_frame_url;
  }
  void set_starting_site_instance(SiteInstance* site_instance) {
    starting_site_instance_ = site_instance;
  }
  void set_source_site_instance(SiteInstance* site_instance) {
    source_site_instance_ = site_instance;
  }
  void set_page_transition(ui::PageTransition page_transition) {
    page_transition_ = page_transition;
  }
  void set_net_error_code(net::Error error_code) {
    net_error_code_ = error_code;
  }
  void set_render_frame_host(RenderFrameHost* render_frame_host) {
    render_frame_host_ = render_frame_host;
  }
  void set_is_same_document(bool is_same_document) {
    is_same_document_ = is_same_document;
  }
  void set_is_served_from_bfcache(bool is_served_from_bfcache) {
    is_served_from_bfcache_ = is_served_from_bfcache;
  }
  void set_is_prerendered_page_activation(bool is_prerendered_page_activation) {
    is_prerendered_page_activation_ = is_prerendered_page_activation;
  }
  void set_is_in_fenced_frame_tree(bool is_in_fenced_frame_tree) {
    is_in_fenced_frame_tree_ = is_in_fenced_frame_tree;
  }
  void set_is_renderer_initiated(bool is_renderer_initiated) {
    is_renderer_initiated_ = is_renderer_initiated;
  }
  void set_is_in_primary_main_frame(bool is_in_primary_main_frame) {
    is_in_primary_main_frame_ = is_in_primary_main_frame;
  }
  void set_redirect_chain(const std::vector<GURL>& redirect_chain) {
    redirect_chain_ = redirect_chain;
  }
  void set_has_committed(bool has_committed) { has_committed_ = has_committed; }
  void set_is_error_page(bool is_error_page) { is_error_page_ = is_error_page; }
  void set_request_headers(const net::HttpRequestHeaders& request_headers) {
    request_headers_ = request_headers;
  }
  void set_response_headers(
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    response_headers_ = response_headers;
  }
  void set_ssl_info(const net::SSLInfo& ssl_info) { ssl_info_ = ssl_info; }
  void set_global_request_id(const GlobalRequestID& global_request_id) {
    global_request_id_ = global_request_id;
  }
  void set_is_form_submission(bool is_form_submission) {
    is_form_submission_ = is_form_submission;
  }
  void set_was_response_cached(bool was_response_cached) {
    was_response_cached_ = was_response_cached;
  }
  void set_impression(const blink::Impression& impression) {
    impression_ = impression;
  }
  void set_initiator_frame_token(
      const blink::LocalFrameToken* initiator_frame_token) {
    initiator_frame_token_ = base::OptionalFromPtr(initiator_frame_token);
  }
  void set_initiator_process_id(int process_id) {
    initiator_process_id_ = process_id;
  }
  void set_initiator_origin(const url::Origin& initiator_origin) {
    initiator_origin_ = initiator_origin;
  }
  void set_reload_type(ReloadType reload_type) { reload_type_ = reload_type; }
  void set_was_started_from_context_menu(bool was_started_from_context_menu) {
    was_started_from_context_menu_ = was_started_from_context_menu;
  }

 private:
  int64_t navigation_id_;
  GURL url_;
  GURL previous_primary_main_frame_url_;
  raw_ptr<SiteInstance> starting_site_instance_ = nullptr;
  raw_ptr<SiteInstance, DanglingUntriaged> source_site_instance_ = nullptr;
  raw_ptr<WebContents, DanglingUntriaged> web_contents_ = nullptr;
  GURL base_url_for_data_url_;
  blink::mojom::Referrer referrer_;
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;
  net::Error net_error_code_ = net::OK;
  raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_ = nullptr;
  bool is_same_document_ = false;
  bool is_served_from_bfcache_ = false;
  bool is_prerendered_page_activation_ = false;
  bool is_in_fenced_frame_tree_ = false;
  bool is_renderer_initiated_ = true;
  bool is_in_primary_main_frame_ = true;
  std::vector<GURL> redirect_chain_;
  bool has_committed_ = false;
  bool is_error_page_ = false;
  net::HttpRequestHeaders request_headers_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::optional<net::SSLInfo> ssl_info_;
  std::optional<net::AuthChallengeInfo> auth_challenge_info_;
  net::ResolveErrorInfo resolve_error_info_;
  content::GlobalRequestID global_request_id_;
  bool is_form_submission_ = false;
  bool was_response_cached_ = false;
  std::optional<url::Origin> initiator_origin_;
  std::optional<GURL> initiator_base_url_;
  ReloadType reload_type_ = content::ReloadType::NONE;
  std::string href_translate_;
  std::optional<blink::Impression> impression_;
  std::optional<blink::LocalFrameToken> initiator_frame_token_;
  int initiator_process_id_ = ChildProcessHost::kInvalidUniqueID;
  bool was_started_from_context_menu_ = false;
  blink::RuntimeFeatureStateContext runtime_feature_state_context_;

  base::WeakPtrFactory<MockNavigationHandle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_HANDLE_H_
