// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_HANDLE_H_
#define CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_isolation_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/referrer.mojom.h"
#include "url/gurl.h"

namespace content {

class MockNavigationHandle : public NavigationHandle {
 public:
  MockNavigationHandle();
  explicit MockNavigationHandle(WebContents* web_contents);
  MockNavigationHandle(const GURL& url, RenderFrameHost* render_frame_host);
  ~MockNavigationHandle() override;

  // NavigationHandle implementation:
  int64_t GetNavigationId() override { return navigation_id_; }
  const GURL& GetURL() override { return url_; }
  SiteInstance* GetStartingSiteInstance() override {
    return starting_site_instance_;
  }
  SiteInstance* GetSourceSiteInstance() override {
    return nullptr;  // Good enough for unit tests...
  }
  bool IsInMainFrame() override {
    return render_frame_host_ ? !render_frame_host_->GetParent() : true;
  }
  MOCK_METHOD0(IsParentMainFrame, bool());
  bool IsRendererInitiated() override { return true; }
  MOCK_METHOD0(GetFrameTreeNodeId, int());
  MOCK_METHOD0(GetPreviousRenderFrameHostId, GlobalFrameRoutingId());
  bool IsServedFromBackForwardCache() override { return false; }
  RenderFrameHost* GetParentFrame() override {
    return render_frame_host_ ? render_frame_host_->GetParent() : nullptr;
  }
  WebContents* GetWebContents() override { return web_contents_; }
  MOCK_METHOD0(NavigationStart, base::TimeTicks());
  MOCK_METHOD0(NavigationInputStart, base::TimeTicks());
  MOCK_METHOD0(WasStartedFromContextMenu, bool());
  MOCK_METHOD0(GetSearchableFormURL, const GURL&());
  MOCK_METHOD0(GetSearchableFormEncoding, const std::string&());
  ReloadType GetReloadType() override { return reload_type_; }
  RestoreType GetRestoreType() override { return RestoreType::NONE; }
  const GURL& GetBaseURLForDataURL() override { return base_url_for_data_url_; }
  MOCK_METHOD0(IsPost, bool());
  const blink::mojom::Referrer& GetReferrer() override { return referrer_; }
  MOCK_METHOD0(HasUserGesture, bool());
  ui::PageTransition GetPageTransition() override { return page_transition_; }
  MOCK_METHOD0(GetNavigationUIData, NavigationUIData*());
  MOCK_METHOD0(IsExternalProtocol, bool());
  net::Error GetNetErrorCode() override { return net_error_code_; }
  RenderFrameHost* GetRenderFrameHost() override { return render_frame_host_; }
  bool IsSameDocument() override { return is_same_document_; }
  MOCK_METHOD0(WasServerRedirect, bool());
  const std::vector<GURL>& GetRedirectChain() override {
    return redirect_chain_;
  }
  bool HasCommitted() override { return has_committed_; }
  bool IsErrorPage() override { return is_error_page_; }
  MOCK_METHOD0(HasSubframeNavigationEntryCommitted, bool());
  MOCK_METHOD0(DidReplaceEntry, bool());
  MOCK_METHOD0(ShouldUpdateHistory, bool());
  MOCK_METHOD0(GetPreviousURL, const GURL&());
  MOCK_METHOD0(GetSocketAddress, net::IPEndPoint());
  const net::HttpRequestHeaders& GetRequestHeaders() override {
    return request_headers_;
  }
  MOCK_METHOD1(RemoveRequestHeader, void(const std::string&));
  MOCK_METHOD2(SetRequestHeader, void(const std::string&, const std::string&));
  const net::HttpResponseHeaders* GetResponseHeaders() override {
    return response_headers_.get();
  }
  MOCK_METHOD0(GetConnectionInfo, net::HttpResponseInfo::ConnectionInfo());
  const base::Optional<net::SSLInfo>& GetSSLInfo() override {
    return ssl_info_;
  }
  const base::Optional<net::AuthChallengeInfo>& GetAuthChallengeInfo()
      override {
    return auth_challenge_info_;
  }
  MOCK_METHOD0(GetNetworkIsolationKey, net::NetworkIsolationKey());
  MOCK_METHOD0(GetGlobalRequestID, const GlobalRequestID&());
  MOCK_METHOD0(IsDownload, bool());
  bool IsFormSubmission() override { return is_form_submission_; }
  MOCK_METHOD0(WasInitiatedByLinkClick, bool());
  MOCK_METHOD0(IsSignedExchangeInnerResponse, bool());
  MOCK_METHOD0(HasPrefetchedAlternativeSubresourceSignedExchange, bool());
  bool WasResponseCached() override { return was_response_cached_; }
  const net::ProxyServer& GetProxyServer() override { return proxy_server_; }
  const std::string& GetHrefTranslate() override { return href_translate_; }
  const base::Optional<url::Origin>& GetInitiatorOrigin() override {
    return initiator_origin_;
  }
  MOCK_METHOD1(RegisterThrottleForTesting,
               void(std::unique_ptr<NavigationThrottle>));
  MOCK_METHOD0(IsDeferredForTesting, bool());
  MOCK_METHOD1(RegisterSubresourceOverride,
               void(mojom::TransferrableURLLoaderPtr));
  MOCK_METHOD0(FromDownloadCrossOriginRedirect, bool());
  MOCK_METHOD0(IsSameProcess, bool());
  MOCK_METHOD0(GetNavigationEntryOffset, int());

  void set_url(const GURL& url) { url_ = url; }
  void set_starting_site_instance(SiteInstance* site_instance) {
    starting_site_instance_ = site_instance;
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
  void set_is_form_submission(bool is_form_submission) {
    is_form_submission_ = is_form_submission;
  }
  void set_was_response_cached(bool was_response_cached) {
    was_response_cached_ = was_response_cached;
  }
  void set_proxy_server(const net::ProxyServer& proxy_server) {
    proxy_server_ = proxy_server;
  }
  void set_initiator_origin(const url::Origin& initiator_origin) {
    initiator_origin_ = initiator_origin;
  }
  void set_reload_type(ReloadType reload_type) { reload_type_ = reload_type; }

 private:
  int64_t navigation_id_;
  GURL url_;
  SiteInstance* starting_site_instance_ = nullptr;
  WebContents* web_contents_ = nullptr;
  GURL base_url_for_data_url_;
  blink::mojom::Referrer referrer_;
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;
  net::Error net_error_code_ = net::OK;
  RenderFrameHost* render_frame_host_ = nullptr;
  bool is_same_document_ = false;
  std::vector<GURL> redirect_chain_;
  bool has_committed_ = false;
  bool is_error_page_ = false;
  net::HttpRequestHeaders request_headers_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  base::Optional<net::SSLInfo> ssl_info_;
  base::Optional<net::AuthChallengeInfo> auth_challenge_info_;
  bool is_form_submission_ = false;
  bool was_response_cached_ = false;
  net::ProxyServer proxy_server_;
  base::Optional<url::Origin> initiator_origin_;
  ReloadType reload_type_ = content::ReloadType::NONE;
  std::string href_translate_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_NAVIGATION_HANDLE_H_
