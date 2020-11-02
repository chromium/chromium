// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_PARAMS_H_

#include <memory>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/url_loader_factory.mojom-shared.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/navigation/triggering_event_info.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-shared.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-forward.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_source_location.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_timings.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_origin_policy.h"

#if INSIDE_BLINK
#include "base/memory/scoped_refptr.h"
#endif

namespace base {
class TickClock;
}

namespace blink {

class KURL;
class WebDocumentLoader;
class WebLocalFrame;

// This structure holds all information collected by Blink when
// navigation is being initiated.
struct BLINK_EXPORT WebNavigationInfo {
  WebNavigationInfo() = default;
  ~WebNavigationInfo() = default;

  // The main resource request.
  WebURLRequest url_request;

  // The frame type. This must not be kNone. See RequestContextFrameType.
  // TODO(dgozman): enforce this is not kNone.
  mojom::RequestContextFrameType frame_type =
      mojom::RequestContextFrameType::kNone;

  // The navigation type. See WebNavigationType.
  WebNavigationType navigation_type = kWebNavigationTypeOther;

  // The navigation policy. See WebNavigationPolicy.
  WebNavigationPolicy navigation_policy = kWebNavigationPolicyCurrentTab;

  // Whether the frame had a transient user activation
  // at the time this request was issued.
  bool has_transient_user_activation = false;

  // The load type. See WebFrameLoadType.
  WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;

  // During a history load, a child frame can be initially navigated
  // to an url from the history state. This flag indicates it.
  bool is_history_navigation_in_new_child_frame = false;

  // Whether the navigation is a result of client redirect.
  bool is_client_redirect = false;

  // WebLocalFrame that initiated this navigation request. May be null for
  // navigations that are not associated with a frame. Storing this pointer is
  // dangerous, it should be verified by comparing against a set of known active
  // frames before direct use.
  WebLocalFrame* initiator_frame;

  // Whether the navigation initiator frame has the
  // |network::mojom::blink::WebSandboxFlags::kDownloads| bit set in its sandbox
  // flags set.
  bool initiator_frame_has_download_sandbox_flag = false;

  // Whether the navigation initiator frame is an ad frame.
  bool initiator_frame_is_ad = false;

  // Whether this is a navigation in the opener frame initiated
  // by the window.open'd frame.
  bool is_opener_navigation = false;

  // Whether the runtime feature |BlockingDownloadsInSandbox| is enabled.
  bool blocking_downloads_in_sandbox_enabled = false;

  // Event information. See TriggeringEventInfo.
  TriggeringEventInfo triggering_event_info = TriggeringEventInfo::kUnknown;

  // If the navigation is a result of form submit, the form element is provided.
  WebFormElement form;

  // The location in the source which triggered the navigation.
  // Used to help web developers understand what caused the navigation.
  WebSourceLocation source_location;

  // The initiator of this navigation used by DevTools.
  WebString devtools_initiator_info;

  // Whether this navigation should check CSP.
  network::mojom::CSPDisposition
      should_check_main_world_content_security_policy =
          network::mojom::CSPDisposition::CHECK;

  // When navigating to a blob url, this token specifies the blob.
  CrossVariantMojoRemote<mojom::BlobURLTokenInterfaceBase> blob_url_token;

  // When navigation initiated from the user input, this tracks
  // the input start time.
  base::TimeTicks input_start;

  // This is the navigation relevant CSP to be used during request and response
  // checks.
  WebVector<WebContentSecurityPolicy> initiator_csp;

  // The navigation initiator source to be used when comparing an URL against
  // 'self'.
  WebContentSecurityPolicySourceExpression initiator_self_source;

  // The navigation initiator, if any.
  CrossVariantMojoRemote<mojom::NavigationInitiatorInterfaceBase>
      navigation_initiator_remote;

  // Specifies whether or not a MHTML Archive can be used to load a subframe
  // resource instead of doing a network request.
  enum class ArchiveStatus { Absent, Present };
  ArchiveStatus archive_status = ArchiveStatus::Absent;

  // The origin trial features activated in the document initiating this
  // navigation that should be applied in the document being navigated to.
  WebVector<int> initiator_origin_trial_features;

  // The value of hrefTranslate attribute of a link, if this navigation was
  // inititated by clicking a link.
  WebString href_translate;

  // Optional impression associated with this navigation. This is attached when
  // a navigation results from a click on an anchor tag that has conversion
  // measurement attributes.
  base::Optional<WebImpression> impression;

  // The navigation initiator's address space.
  network::mojom::IPAddressSpace initiator_address_space =
      network::mojom::IPAddressSpace::kUnknown;

  // The frame policy specified by the frame owner element.
  // For top-level window with no opener, this is the default lax FramePolicy.
  FramePolicy frame_policy;
};

// This structure holds all information provided by the embedder that is
// needed for blink to load a Document. This is hence different from
// WebDocumentLoader::ExtraData, which is an opaque structure stored in the
// DocumentLoader and used by the embedder.
struct BLINK_EXPORT WebNavigationParams {
  WebNavigationParams();
  ~WebNavigationParams();

  // Allows to specify |devtools_navigation_token|, instead of creating
  // a new one.
  explicit WebNavigationParams(const base::UnguessableToken&);

  // Shortcut for navigating based on WebNavigationInfo parameters.
  static std::unique_ptr<WebNavigationParams> CreateFromInfo(
      const WebNavigationInfo&);

  // Shortcut for loading html with "text/html" mime type and "UTF8" encoding.
  static std::unique_ptr<WebNavigationParams> CreateWithHTMLString(
      base::span<const char> html,
      const WebURL& base_url);

#if INSIDE_BLINK
  // Shortcut for loading html with "text/html" mime type and "UTF8" encoding.
  static std::unique_ptr<WebNavigationParams> CreateWithHTMLBuffer(
      scoped_refptr<SharedBuffer> buffer,
      const KURL& base_url);
#endif

  // Fills |body_loader| based on the provided static data.
  static void FillBodyLoader(WebNavigationParams*, base::span<const char> data);
  static void FillBodyLoader(WebNavigationParams*, WebData data);

  // Fills |response| and |body_loader| based on the provided static data.
  // |url| must be set already.
  static void FillStaticResponse(WebNavigationParams*,
                                 WebString mime_type,
                                 WebString text_encoding,
                                 base::span<const char> data);

  // This block defines the request used to load the main resource
  // for this navigation.

  // This URL indicates the security origin and will be used as a base URL
  // to resolve links in the committed document.
  WebURL url;
  // The http method (if any) used to load the main resource.
  WebString http_method;
  // The referrer string and policy used to load the main resource.
  WebString referrer;
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;
  // The http body of the request used to load the main resource, if any.
  WebHTTPBody http_body;
  // The http content type of the request used to load the main resource, if
  // any.
  WebString http_content_type;
  // The origin of the request used to load the main resource, specified at
  // https://fetch.spec.whatwg.org/#concept-request-origin. Can be null.
  // TODO(dgozman,nasko): we shouldn't need both this and |origin_to_commit|.
  WebSecurityOrigin requestor_origin;
  // If non-null, used as a URL which we weren't able to load. For example,
  // history item will contain this URL instead of request's URL.
  // This URL can be retrieved through WebDocumentLoader::UnreachableURL.
  WebURL unreachable_url;

  // The IP address space from which this document was loaded.
  // https://wicg.github.io/cors-rfc1918/#address-space
  network::mojom::IPAddressSpace ip_address_space =
      network::mojom::IPAddressSpace::kUnknown;

  // The net error code for failed navigation. Must be non-zero when
  // |unreachable_url| is non-null.
  int error_code = 0;

  // This block defines the document content. The alternatives in the order
  // of precedence are:
  // 1. If |is_static_data| is false:
  //   1a. If url loads as an empty document (according to
  //       WebDocumentLoader::WillLoadUrlAsEmpty), the document will be empty.
  //   1b. If loading an iframe of mhtml archive, the document will be
  //       retrieved from the archive.
  // 2. Otherwise, provided redirects and response are used to construct
  //    the final response.
  //   2a. If body loader is present, it will be used to fetch the content.
  //   2b. If body loader is missing, but url is a data url, it will be
  //       decoded and used as response and document content.
  //   2c. If decoding data url fails, or url is not a data url, the
  //       navigation will fail.

  struct RedirectInfo {
    // New base url after redirect.
    WebURL new_url;
    // Http method used for redirect.
    WebString new_http_method;
    // New referrer string and policy used for redirect.
    WebString new_referrer;
    network::mojom::ReferrerPolicy new_referrer_policy;
    // Redirect response itself.
    // TODO(dgozman): we only use this response for navigation timings.
    // Perhaps, we can just get rid of it.
    WebURLResponse redirect_response;
  };
  // Redirects which happened while fetching the main resource.
  // TODO(dgozman): we are only interested in the final values instead of
  // all information about redirects.
  WebVector<RedirectInfo> redirects;
  // The final response for the main resource. This will be used to determine
  // the type of resulting document.
  WebURLResponse response;
  // The body loader which allows to retrieve the response body when available.
  std::unique_ptr<WebNavigationBodyLoader> body_loader;
  // Whether |response| and |body_loader| represent static data. In this case
  // we skip some security checks and insist on loading this exact content.
  bool is_static_data = false;

  // This block defines the type of the navigation.

  // The load type. See WebFrameLoadType for definition.
  WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
  // History item should be provided for back-forward load types.
  WebHistoryItem history_item;
  // Whether this navigation is a result of client redirect.
  bool is_client_redirect = false;
  // Cache mode to be used for subresources, instead of the one determined
  // by |frame_load_type|.
  base::Optional<blink::mojom::FetchCacheMode> force_fetch_cache_mode;

  // Miscellaneous parameters.

  // The origin in which a navigation should commit. When provided, Blink
  // should use this origin directly and not compute locally the new document
  // origin.
  WebSecurityOrigin origin_to_commit;
  // The devtools token for this navigation. See DocumentLoader
  // for details.
  base::UnguessableToken devtools_navigation_token;
  // Known timings related to navigation. If the navigation has
  // started in another process, timings are propagated from there.
  WebNavigationTimings navigation_timings;
  // Indicates that the frame was previously discarded.
  // was_discarded is exposed on Document after discard, see:
  // https://github.com/WICG/web-lifecycle
  bool was_discarded = false;
  // Whether this navigation had a transient user activation
  // when inititated.
  bool had_transient_activation = false;
  // Whether this navigation has a sticky user activation flag.
  bool is_user_activated = false;
  // Whether the navigation should be allowed to invoke a text fragment anchor.
  // This is based on a user activation but is different from the above bit as
  // it can be propagated across redirects and is consumed on use.
  bool has_text_fragment_token = false;
  // Whether this navigation was browser initiated.
  bool is_browser_initiated = false;
  // Whether the document should be able to access local file:// resources.
  bool grant_load_local_resources = false;
  // The previews state which should be used for this navigation.
  PreviewsState previews_state = PreviewsTypes::kPreviewsUnspecified;
  // The service worker network provider to be used in the new
  // document.
  std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
      service_worker_network_provider;
  // The AppCache host id for this navigation.
  base::UnguessableToken appcache_host_id;

  // Used for SignedExchangeSubresourcePrefetch.
  // This struct keeps the information about a prefetched signed exchange.
  struct BLINK_EXPORT PrefetchedSignedExchange {
    PrefetchedSignedExchange();
    PrefetchedSignedExchange(
        const WebURL& outer_url,
        const WebString& header_integrity,
        const WebURL& inner_url,
        const WebURLResponse& inner_response,
        CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
            loader_factory);
    ~PrefetchedSignedExchange();

    WebURL outer_url;
    WebString header_integrity;
    WebURL inner_url;
    WebURLResponse inner_response;
    CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
        loader_factory;
  };
  WebVector<std::unique_ptr<PrefetchedSignedExchange>>
      prefetched_signed_exchanges;
  // An optional tick clock to be used for document loader timing. This is used
  // for testing.
  const base::TickClock* tick_clock = nullptr;
  // The origin trial features activated in the document initiating this
  // navigation that should be applied in the document being navigated to.
  WebVector<int> initiator_origin_trial_features;

  base::Optional<WebOriginPolicy> origin_policy;

  // The physical URL of Web Bundle from which the document is loaded.
  // Used as an additional identifier for MemoryCache.
  WebURL web_bundle_physical_url;

  // The claimed URL inside Web Bundle file from which the document is loaded.
  // This URL is used for window.location and document.URL and relative path
  // computation in the document.
  WebURL web_bundle_claimed_url;

  // UKM source id to be associated with the Document that will be installed
  // in the current frame.
  ukm::SourceId document_ukm_source_id = ukm::kInvalidSourceId;

  // The frame policy specified by the frame owner element.
  // Should be base::nullopt for top level navigations
  base::Optional<FramePolicy> frame_policy;

  // A list of origin trial names to enable for the document being loaded.
  WebVector<WebString> force_enabled_origin_trials;

  // Whether the page is origin isolated.
  // https://github.com/WICG/origin-isolation
  bool origin_isolated = false;

  // List of client hints enabled for top-level frame. These still need to be
  // checked against feature policy before use.
  WebVector<network::mojom::WebClientHintsType> enabled_client_hints;

  // Whether the navigation is cross browsing context group (browsing instance).
  bool is_cross_browsing_context_group_navigation = false;

  // A list of additional content security policies to be enforced by blink.
  WebVector<WebString> forced_content_security_policies;

  // Blink's copy of the policy container containing security policies to be
  // enforced on the document created by this navigation.
  std::unique_ptr<WebPolicyContainerClient> policy_container;
};

}  // namespace blink

#endif
