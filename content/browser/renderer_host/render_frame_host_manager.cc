// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_manager.h"

#include <stddef.h>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/process_lock.h"
#include "content/browser/process_reuse_policy.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_owner.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_enums.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/spare_render_process_host_manager_impl.h"
#include "content/browser/security/coop/cross_origin_opener_policy_reporter.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "ui/gfx/mac/scoped_cocoa_disable_screen_updates.h"
#endif  // BUILDFLAG(IS_MAC)

namespace content {

using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;
using perfetto::protos::pbzero::ChromeTrackEvent;

namespace {

const char kBackForwardCachePageWithFormStorableHistogramName[] =
    "BackForwardCache.PageWithForm.Storable";

bool IsAbout(const GURL& url) {
  return url.IsAboutSrcdoc() || url.IsAboutBlank();
}

// Helper function to determine whether a navigation from `current_rfh` to
// `destination_effective_url_info` should swap BrowsingInstances to ensure that
// `destination_effective_url_info` ends up in a dedicated process.  This is the
// case when `destination_effective_url` has an origin that was just isolated
// dynamically, where leaving the navigation in the current BrowsingInstance
// would leave `destination_effective_url_info` without a dedicated process,
// since dynamic origin isolation applies only to future BrowsingInstances.  In
// the common case where `current_rfh` is a main frame, and there are no
// scripting references to it from other windows, it is safe to swap
// BrowsingInstances to ensure the new isolated origin takes effect.  Note that
// this applies even to same-site navigations, as well as to renderer-initiated
// navigations.
bool ShouldSwapBrowsingInstancesForDynamicIsolation(
    RenderFrameHostImpl* current_rfh,
    const UrlInfo& destination_effective_url_info) {
  // Only main frames are eligible to swap BrowsingInstances.
  if (!current_rfh->is_main_frame())
    return false;

  // Skip cases when there are other windows that might script this one.
  SiteInstanceImpl* current_instance = current_rfh->GetSiteInstance();
  if (current_instance->GetRelatedActiveContentsCount() > 1u)
    return false;

  // Check whether `destination_effective_url_info` would require a dedicated
  // process if we left it in the current BrowsingInstance.  If so, there's no
  // need to swap BrowsingInstances.
  auto& current_isolation_context = current_instance->GetIsolationContext();
  auto site_info_in_current_context = SiteInfo::Create(
      current_isolation_context, destination_effective_url_info);
  if (site_info_in_current_context.RequiresDedicatedProcess(
          current_isolation_context)) {
    return false;
  }

  // Finally, check whether `destination_effective_url_info` would require a
  // dedicated process if we were to swap to a fresh BrowsingInstance.  To check
  // this, use a new IsolationContext, rather than
  // current_instance->GetIsolationContext().
  IsolationContext future_isolation_context(
      current_instance->GetBrowserContext());
  auto site_info_in_future_context = SiteInfo::Create(
      future_isolation_context, destination_effective_url_info);
  return site_info_in_future_context.RequiresDedicatedProcess(
      future_isolation_context);
}

// Helper function to determine whether |dest_url_info| should be loaded in the
// same StoragePartition that |current_instance| is currently using.
bool DoesNavigationChangeStoragePartition(SiteInstanceImpl* current_instance,
                                          const UrlInfo& dest_url_info) {
  // Derive a new SiteInfo from |current_instance|, but don't treat the
  // navigation as related to avoid StoragePartition propagation logic. Note
  // that we discard WebExposedIsolationInfo in that computation, because we
  // want to consider change in StoragePartition independently from it.
  StoragePartitionConfig dest_partition_config =
      current_instance
          ->DeriveSiteInfo(dest_url_info, /*is_related=*/false,
                           /*disregard_web_exposed_isolation_info=*/true)
          .storage_partition_config();
  StoragePartitionConfig current_partition_config =
      current_instance->GetSiteInfo().storage_partition_config();
  return current_partition_config != dest_partition_config;
}

bool IsSiteInstanceCompatibleWithErrorIsolation(
    SiteInstanceImpl* site_instance,
    const FrameTreeNode& frame_tree_node,
    NavigationRequest::ErrorPageProcess error_page_process) {
  if (error_page_process ==
      NavigationRequest::ErrorPageProcess::kCurrentProcess) {
    // If an error page must commit in the current process, the current
    // SiteInstance must be reused.
    return site_instance ==
           frame_tree_node.current_frame_host()->GetSiteInstance();
  }

  if (!frame_tree_node.IsErrorPageIsolationEnabled()) {
    // With no error isolation or current process requirement, all SiteInstances
    // are compatible with any |error_page_process|.
    CHECK(error_page_process ==
              NavigationRequest::ErrorPageProcess::kNotErrorPage ||
          error_page_process ==
              NavigationRequest::ErrorPageProcess::kDestinationProcess);
    return true;
  }

  // When error page isolation is enabled, don't reuse |site_instance| if it's
  // an error page SiteInstance, but the navigation is not an error page
  // navigation. Similarly, don't reuse `site_instance` if it's not an error
  // page SiteInstance but the navigation will fail and actually need an error
  // page SiteInstance.
  bool is_site_instance_for_error_page =
      site_instance->GetSiteInfo().is_error_page();
  bool should_be_error_page_isolated =
      (error_page_process !=
           NavigationRequest::ErrorPageProcess::kNotErrorPage &&
       error_page_process !=
           NavigationRequest::ErrorPageProcess::kPostCommitErrorPage);
  return is_site_instance_for_error_page == should_be_error_page_isolated;
}

// Simple wrapper around WebExposedIsolationInfo::AreCompatible for easier use
// within the process model.
bool IsSiteInstanceCompatibleWithWebExposedIsolation(
    SiteInstanceImpl* site_instance,
    const std::optional<WebExposedIsolationInfo>& web_exposed_isolation_info) {
  return WebExposedIsolationInfo::AreCompatible(
      site_instance->GetWebExposedIsolationInfo(), web_exposed_isolation_info);
}

// Helper for appending more information to the optional |reason| parameter
// that some of the RenderFrameHostManager's methods expose for debugging /
// diagnostic purposes.
void AppendReason(std::string* reason, const char* value) {
  if (!reason)
    return;

  if (!reason->empty())
    reason->append("; ");
  reason->append(value);

  DCHECK_LT(reason->size(),
            static_cast<size_t>(base::debug::CrashKeySize::Size256));
}

perfetto::protos::pbzero::ShouldSwapBrowsingInstance
ShouldSwapBrowsingInstanceToProto(ShouldSwapBrowsingInstance result) {
  using ProtoLevel = perfetto::protos::pbzero::ShouldSwapBrowsingInstance;
  switch (result) {
    case ShouldSwapBrowsingInstance::kYes_ForceSwap:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_YES_FORCE_SWAP;
    case ShouldSwapBrowsingInstance::kYes_CrossSiteProactiveSwap:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_YES_CROSS_SITE_PROACTIVE_SWAP;
    case ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_YES_SAME_SITE_PROACTIVE_SWAP;
    case ShouldSwapBrowsingInstance::kNo_ProactiveSwapDisabled:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_PROACTIVE_SWAP_DISABLED;
    case ShouldSwapBrowsingInstance::kNo_NotMainFrame:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_NOT_MAIN_FRAME;
    case ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_HAS_RELATED_ACTIVE_CONTENTS;
    case ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_DOES_NOT_HAVE_SITE;
    case ShouldSwapBrowsingInstance::kNo_SourceURLSchemeIsNotHTTPOrHTTPS:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_SOURCE_URL_SCHEME_NOT_HTTP_OR_HTTPS;
    case ShouldSwapBrowsingInstance::kNo_SameSiteNavigation:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_SAME_SITE_NAVIGATION;
    case ShouldSwapBrowsingInstance::kNo_AlreadyHasMatchingBrowsingInstance:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_ALREADY_HAS_MATCHING_BROWSING_INSTANCE;
    case ShouldSwapBrowsingInstance::kNo_RendererDebugURL:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_RENDERER_DEBUG_URL;
    case ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_NOT_NEEDED_FOR_BACK_FORWARD_CACHE;
    case ShouldSwapBrowsingInstance::kNo_SameDocumentNavigation:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_SAME_DOCUMENT_NAVIGATION;
    case ShouldSwapBrowsingInstance::kNo_SameUrlNavigation:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_SAME_URL_NAVIGATION;
    case ShouldSwapBrowsingInstance::kNo_WillReplaceEntry:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_WILL_REPLACE_ENTRY;
    case ShouldSwapBrowsingInstance::kNo_Reload:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_RELOAD;
    case ShouldSwapBrowsingInstance::kNo_Guest:
      return ProtoLevel::SHOULD_SWAP_BROWSING_INSTANCE_NO_GUEST;
    case ShouldSwapBrowsingInstance::kNo_HasNotComittedAnyNavigation:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_HAS_NOT_COMMITTED_ANY_NAVIGATION;
    case ShouldSwapBrowsingInstance::kNo_NotPrimaryMainFrame:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_NOT_PRIMARY_MAIN_FRAME;
    case ShouldSwapBrowsingInstance::kNo_InitiatorRequestedNoProactiveSwap:
      return ProtoLevel::
          SHOULD_SWAP_BROWSING_INSTANCE_NO_INITIATOR_REQUESTED_NO_PROACTIVE_SWAP;
  }
}

void TraceShouldSwapBrowsingInstanceResult(FrameTreeNodeId frame_tree_node_id,
                                           ShouldSwapBrowsingInstance result) {
  TRACE_EVENT_INSTANT(
      "navigation",
      "RenderFrameHostManager::GetSiteInstanceForNavigation_ShouldSwapResult",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<ChromeTrackEvent>();
        auto* data = event->set_should_swap_browsing_instances_result();
        data->set_frame_tree_node_id(frame_tree_node_id.value());
        data->set_result(ShouldSwapBrowsingInstanceToProto(result));
      });
}

// This method tries to find a process for |new_instance| to reuse by starting
// from |rfh|'s outermost main frame, and then iterating through all the
// embedded fenced frame FrameTrees and trying to reuse their BrowsingInstance's
// default process (if one is set). By setting a process for |new_instance|, it
// is also setting its BrowsingInstance's default process, and as a result, it
// gets these groups of BrowsingInstances to share the same default process.
//
// Note that it is possible for a fenced frame BrowsingInstance to get assigned
// a default process first, before its embedder (for example: if the embedder
// only had a frame at an isolated site, which embeds a fenced frame at a
// non-isolated site). If we were to assign the embedder BrowsingInstance a
// default process later (from the previous example, if the embedder added a
// non-isolated iframe), we would iterate through the entire set of FrameTrees
// and find and reuse the fenced frame BrowsingInstance's default process.
//
// TODO(crbug.com/40232875): There are certain scenarios where this won't work,
// see bug for an example scenario/proposed fix.
void ReuseDefaultProcessFromDifferentBrowsingInstanceIfPossible(
    scoped_refptr<SiteInstanceImpl> new_instance,
    RenderFrameHostImpl* rfh) {
  DCHECK(!new_instance->RequiresDedicatedProcess());
  DCHECK(!new_instance->HasProcess());
  RenderFrameHostImpl* root = rfh->GetOutermostMainFrame();
  root->ForEachRenderFrameHostWithAction(
      [site_instance = std::move(new_instance),
       root](RenderFrameHostImpl* rfhi) {
        if (rfhi->GetParent())
          return RenderFrameHost::FrameIterationAction::kContinue;

        // Avoid traversing through any embedded pages that aren't fenced
        // frames. Note that we use rfhi->GetParentOrOuterDocumentOrEmbedder()
        // instead of rfhi->GetParentOrOuterDocument() to avoid traversing
        // through guests.
        if (rfhi != root && rfhi->GetParentOrOuterDocumentOrEmbedder() &&
            !rfhi->IsNestedWithinFencedFrame())
          return RenderFrameHost::FrameIterationAction::kSkipChildren;

        if (RenderProcessHost* default_process =
                rfhi->GetSiteInstance()
                    ->GetDefaultProcessForBrowsingInstance()) {
          site_instance->ReuseExistingProcessIfPossible(default_process);
          if (site_instance->HasProcess())
            return RenderFrameHost::FrameIterationAction::kStop;
        }

        return RenderFrameHost::FrameIterationAction::kContinue;
      });
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ProcessPerSiteWithMainFrameThresholdBlockReason {
  kNotBlocked = 0,
  kDisableProcessResuse = 1,
  kDevToolsWasEverAttached = 2,
  kDoesNotRequireDedicatedProcess = 3,
  kIsIpAddressOrLocalHost = 4,
  kSchemeIsNotHttpOrHttps = 5,
  kMaxValue = kSchemeIsNotHttpOrHttps,
};

void RecordProcessPerSiteWithMainFrameThresholdBlockReason(
    ProcessPerSiteWithMainFrameThresholdBlockReason reason) {
  base::UmaHistogramEnumeration(
      "SiteIsolation.ProcessPerSiteWithMainFrameThreshold.BlockReason", reason);
}

// If `site_instance` is for a main frame, try to reuse an existing process
// when an experimental process-per-site-up-to-main-frame-threshold feature is
// enabled, subject to a threshold for the maximum number of main frames that
// the process can host.
void UpdateProcessReusePolicyForProcessPerSiteWithMainFrameThreshold(
    SiteInstanceImpl* site_instance,
    FrameTreeNode* frame_tree_node) {
  if (!GetContentClient()
           ->browser()
           ->ShouldAllowProcessPerSiteForMultipleMainFrames(
               site_instance->GetBrowserContext())) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          features::kProcessPerSiteUpToMainFrameThreshold)) {
    return;
  }
  if (!frame_tree_node->IsOutermostMainFrame()) {
    return;
  }
  if (base::FeatureList::IsEnabled(features::kDisableProcessReuse)) {
    RecordProcessPerSiteWithMainFrameThresholdBlockReason(
        ProcessPerSiteWithMainFrameThresholdBlockReason::kDisableProcessResuse);
    return;
  }
  if (!features::kProcessPerSiteMainFrameAllowDevToolsAttached.Get() &&
      RenderFrameDevToolsAgentHost::WasEverAttachedToAnyFrame()) {
    RecordProcessPerSiteWithMainFrameThresholdBlockReason(
        ProcessPerSiteWithMainFrameThresholdBlockReason::
            kDevToolsWasEverAttached);
    return;
  }
  if (!site_instance->RequiresDedicatedProcess()) {
    RecordProcessPerSiteWithMainFrameThresholdBlockReason(
        ProcessPerSiteWithMainFrameThresholdBlockReason::
            kDoesNotRequireDedicatedProcess);
    return;
  }

  // ProcessPerSite doesn't work well when DevTools is attached because DevTools
  // assumes that there is only one main frame per renderer process
  // (https://crbug.com/1449114). Localhost and IP based host names are a common
  // target for DevTools to attach to. Exclude localhost and IP based host name
  // for process reuse to work around the problem, unless a field parameter
  // explicitly allows it.
  const GURL& site_url = site_instance->GetSiteURL();
  if (!features::kProcessPerSiteMainFrameAllowIPAndLocalhost.Get() &&
      (site_url.HostIsIPAddress() || net::IsLocalHostname(site_url.host()))) {
    RecordProcessPerSiteWithMainFrameThresholdBlockReason(
        ProcessPerSiteWithMainFrameThresholdBlockReason::
            kIsIpAddressOrLocalHost);
    return;
  }

  // Disallow process reuse when scheme is not HTTP(S).
  if (!site_url.SchemeIsHTTPOrHTTPS()) {
    RecordProcessPerSiteWithMainFrameThresholdBlockReason(
        ProcessPerSiteWithMainFrameThresholdBlockReason::
            kSchemeIsNotHttpOrHttps);
    return;
  }

  RecordProcessPerSiteWithMainFrameThresholdBlockReason(
      ProcessPerSiteWithMainFrameThresholdBlockReason::kNotBlocked);
  site_instance->set_process_reuse_policy(
      ProcessReusePolicy::
          REUSE_PENDING_OR_COMMITTED_SITE_WITH_MAIN_FRAME_THRESHOLD);
}

// Prepares the View and the DelegatedFrameHost when the page is restored from
// BackForwardCache with a ViewTransition (VT) on it.
void PrepareViewTransitionForBFCacheActivation(
    RenderFrameHostImpl* rfh_to_show) {
  // https://crbug.com/1415340: The View that's about to be restored from
  // BFCache has the fallback surface set to the last surface drawn before the
  // page entered the BFCache. If the ViewTransition's animation is delayed
  // (e.g., a renderer slow to produce a new frame), the last surface will be
  // embedded and displayed first. We will be showing the fallback surface
  // first, then then VT aimation, causing a visual glitch.
  //
  // To address this:
  // 1. We force a new allocation group of the browser's `viz::LocalSurfaceId`
  //    allocator. This arg ensures Viz doesn't draw any cached frames produced
  //    by this restored page by changing its allocation group.
  // 2. With a VT, we let BFCache-restored View always steal the
  //    fallback surface from the current View, and let the BFCached
  //    View's fallback content persist after `Show()`.

  auto* rwhv_base =
      static_cast<RenderWidgetHostViewBase*>(rfh_to_show->GetView());

  // Invalidates the current allocation group. For the next surface embedding,
  // the browser will be using a fresh allocation group, yet to be registered
  // with Viz.
  rwhv_base->InvalidateLocalSurfaceIdAndAllocationGroup();

  // Clears the fallback Surface so later on this BFCached new
  // View/DelegatedFrameHost with VT can take the fallback from the old page.
  rwhv_base->ClearFallbackSurfaceForCommitPending();

  // Marks the View/DelegatedFrameHost as evicted. This forces this new View to
  // take a fallback from the old page. If there isn't a fallback surface,
  // `ClearFallbackSurfaceForCommitPending` won't trigger an eviction. In such
  // cases we explicitly mark the View as evicted to force the View to take a
  // fallback. This seems to occur on Mac's content_shell.
  rwhv_base->set_is_evicted();
}

bool NavigationRequestUsesWebUI(NavigationRequest* request,
                                BrowserContext* browser_context) {
  return request->HasWebUI() ||
         (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
              browser_context, request->common_params().url) &&
          request->state() < NavigationRequest::CANCELING);
}

bool CanIntentionallyDeferSpeculativeRFHForRequest(
    NavigationRequest* request,
    BrowserContext* browser_context,
    FrameTreeNode* frame_tree_node) {
  return request->state() ==
             NavigationRequest::NavigationRequest::NOT_STARTED &&
         // We defer creation of the speculative RFH to allow the network
         // request to be sent first. If the navigation doesn't go through the
         // network, we shouldn't defer the creation of speculative RFH.
         request->NeedsUrlLoader() &&
         // If the navigation to a page with WebUI fails and the RFH
         // creation is deferred, the browser will try to create a RFH
         // and set a WebUI for the error page. This will cause the browser
         // to crash since the error page does not need a WebUI.
         !NavigationRequestUsesWebUI(request, browser_context) &&
         // Do not defer the creation of the RFH if the previous renderer
         // crashed or is not live (e.g. for initial RFHs), since we might need
         // to do an early RFH swap, which requires the speculative RFH to be
         // created before the network request is sent.
         frame_tree_node->current_frame_host()->IsRenderFrameLive() &&
         !frame_tree_node->current_frame_host()->must_be_replaced() &&
         // TODO(crbug.com/348125591): Workaround for a mysterious race
         // condition in V8 when navigating to a different site in devtools.
         !DevToolsAgentHost::IsDebuggerAttached(request->GetWebContents());
}

}  // namespace

RenderFrameHostManager::IsSameSiteGetter::IsSameSiteGetter()
    : is_same_site_(std::nullopt) {}

RenderFrameHostManager::IsSameSiteGetter::IsSameSiteGetter(bool is_same_site)
    : is_same_site_(is_same_site) {}

bool RenderFrameHostManager::IsSameSiteGetter::Get(
    const RenderFrameHostImpl& render_frame_host,
    const UrlInfo& url_info) {
  if (!is_same_site_.has_value()) {
    is_same_site_ = render_frame_host.IsNavigationSameSite(url_info);
  } else {
    DCHECK_EQ(is_same_site_.value(),
              render_frame_host.IsNavigationSameSite(url_info));
  }

  return is_same_site_.value();
}

RenderFrameHostManager::RenderFrameHostManager(FrameTreeNode* frame_tree_node,
                                               Delegate* delegate)
    : frame_tree_node_(frame_tree_node), delegate_(delegate) {
  DCHECK(frame_tree_node_);
}

RenderFrameHostManager::~RenderFrameHostManager() {
  DCHECK(!speculative_render_frame_host_);

  // Ensure that proxies associated with pending delete BrowsingContextStates
  // are deleted as well, otherwise these proxies outlive the FrameTreeNode.
  for (const auto& pending_delete_host : pending_delete_hosts_) {
    pending_delete_host->browsing_context_state()->ResetProxyHosts();
  }

  // If the current RenderFrameHost doesn't exist, then there is no need to
  // destroy proxies, as they are only accessible via RenderFrameHost. This
  // only occurs in MPArch activation, as frame trees are destroyed even when
  // the root has no associated RenderFrameHost, specifically when
  // RenderFrameHost has been moved during activation and the source
  // FrameTreeNode is being destroyed.
  if (!render_frame_host_) {
    return;
  }

  // Delete any RenderFrameProxyHosts. It is important to delete those prior to
  // deleting the current RenderFrameHost, since the CrossProcessFrameConnector
  // (owned by RenderFrameProxyHost) points to the RenderWidgetHostView
  // associated with the current RenderFrameHost and uses it during its
  // destructor.
  render_frame_host_->browsing_context_state()->ResetProxyHosts();

  SetRenderFrameHost(nullptr);
}

void RenderFrameHostManager::InitRoot(
    SiteInstanceImpl* site_instance,
    bool renderer_initiated_creation,
    blink::FramePolicy initial_main_frame_policy,
    const std::string& name,
    const base::UnguessableToken& devtools_frame_token) {
  bool is_legacy_browsing_context_state_mode =
      features::GetBrowsingContextMode() ==
      features::BrowsingContextStateImplementationType::
          kLegacyOneToOneWithFrameTreeNode;
  scoped_refptr<BrowsingContextState> browsing_context_state =
      base::MakeRefCounted<BrowsingContextState>(
          blink::mojom::FrameReplicationState::New(
              url::Origin(), name, "", blink::ParsedPermissionsPolicy(),
              network::mojom::WebSandboxFlags::kNone, initial_main_frame_policy,
              // should enforce strict mixed content checking
              blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
              // hashes of hosts for insecure request upgrades
              std::vector<uint32_t>(),
              false /* has_potentially_trustworthy_unique_origin */,
              false /* has_active_user_gesture */,
              false /* has_received_user_gesture_before_nav */,
              false /* is_ad_frame */),
          frame_tree_node_->parent(),
          is_legacy_browsing_context_state_mode
              ? static_cast<std::optional<BrowsingInstanceId>>(std::nullopt)
              : site_instance->GetBrowsingInstanceId(),
          is_legacy_browsing_context_state_mode
              ? static_cast<std::optional<base::UnguessableToken>>(std::nullopt)
              : site_instance->coop_related_group_token());
  browsing_context_state->CommitFramePolicy(initial_main_frame_policy);
  browsing_context_state->SetFrameName(name, "");
  UpdateProcessReusePolicyForProcessPerSiteWithMainFrameThreshold(
      site_instance, frame_tree_node_);
  SetRenderFrameHost(CreateRenderFrameHost(
      CreateFrameCase::kInitRoot, site_instance,
      /*frame_routing_id=*/MSG_ROUTING_NONE,
      mojo::PendingAssociatedRemote<mojom::Frame>(), blink::LocalFrameToken(),
      blink::DocumentToken(), devtools_frame_token, renderer_initiated_creation,
      browsing_context_state));

  // Creating a main RenderFrameHost also creates a new Page, so notify the
  // delegate about this.
  render_frame_host_->GetPage().NotifyPageBecameCurrent();
}

void RenderFrameHostManager::InitChild(
    SiteInstanceImpl* site_instance,
    int32_t frame_routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    const blink::LocalFrameToken& frame_token,
    const blink::DocumentToken& document_token,
    const base::UnguessableToken& devtools_frame_token,
    blink::FramePolicy frame_policy,
    std::string frame_name,
    std::string frame_unique_name) {
  bool is_legacy_browsing_context_state_mode =
      features::GetBrowsingContextMode() ==
      features::BrowsingContextStateImplementationType::
          kLegacyOneToOneWithFrameTreeNode;
  scoped_refptr<BrowsingContextState> browsing_context_state =
      base::MakeRefCounted<BrowsingContextState>(
          blink::mojom::FrameReplicationState::New(
              url::Origin(), frame_name, frame_unique_name,
              blink::ParsedPermissionsPolicy(),
              network::mojom::WebSandboxFlags::kNone, frame_policy,
              // should enforce strict mixed content checking
              blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
              // hashes of hosts for insecure request upgrades
              std::vector<uint32_t>(),
              false /* has_potentially_trustworthy_unique_origin */,
              false /* has_active_user_gesture */,
              false /* has_received_user_gesture_before_nav */,
              false /* is_ad_frame */),
          frame_tree_node_->parent(),
          is_legacy_browsing_context_state_mode
              ? static_cast<std::optional<BrowsingInstanceId>>(std::nullopt)
              : site_instance->GetBrowsingInstanceId(),
          is_legacy_browsing_context_state_mode
              ? static_cast<std::optional<base::UnguessableToken>>(std::nullopt)
              : site_instance->coop_related_group_token());
  browsing_context_state->CommitFramePolicy(frame_policy);
  SetRenderFrameHost(CreateRenderFrameHost(
      CreateFrameCase::kInitChild, site_instance, frame_routing_id,
      std::move(frame_remote), frame_token, document_token,
      devtools_frame_token,
      /*renderer_initiated_creation=*/false, browsing_context_state));
}

RenderWidgetHostViewBase* RenderFrameHostManager::GetRenderWidgetHostView()
    const {
  if (render_frame_host_)
    return static_cast<RenderWidgetHostViewBase*>(
        render_frame_host_->GetView());
  return nullptr;
}

bool RenderFrameHostManager::IsMainFrameForInnerDelegate() {
  return frame_tree_node_->IsMainFrame() &&
         frame_tree_node_->frame_tree()
             .delegate()
             ->GetOuterDelegateFrameTreeNodeId();
}

FrameTreeNode* RenderFrameHostManager::GetOuterDelegateNode() const {
  FrameTreeNodeId outer_contents_frame_tree_node_id =
      frame_tree_node_->frame_tree()
          .delegate()
          ->GetOuterDelegateFrameTreeNodeId();
  return FrameTreeNode::GloballyFindByID(outer_contents_frame_tree_node_id);
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToParent() {
  if (frame_tree_node_->IsMainFrame())
    return nullptr;

  return frame_tree_node_->GetBrowsingContextStateForSubframe()
      ->GetRenderFrameProxyHost(
          frame_tree_node_->parent()->GetSiteInstance()->group());
}

RenderFrameProxyHost* RenderFrameHostManager::GetProxyToOuterDelegate() {
  // Only the main frame should be able to reach the outer WebContents.
  DCHECK(frame_tree_node_->IsMainFrame());
  FrameTreeNode* outer_contents_frame_tree_node = GetOuterDelegateNode();
  if (!outer_contents_frame_tree_node ||
      !outer_contents_frame_tree_node->parent()) {
    return nullptr;
  }

  // We will create an outer delegate proxy in each BrowsingContextState in this
  // frame so it doesn't matter which BCS is used here.
  return render_frame_host_->browsing_context_state()->GetRenderFrameProxyHost(
      outer_contents_frame_tree_node->parent()->GetSiteInstance()->group(),
      BrowsingContextState::ProxyAccessMode::kAllowOuterDelegate);
}

RenderFrameProxyHost*
RenderFrameHostManager::GetProxyToParentOrOuterDelegate() {
  return IsMainFrameForInnerDelegate() ? GetProxyToOuterDelegate()
                                       : GetProxyToParent();
}

void RenderFrameHostManager::RemoveOuterDelegateFrame() {
  // Removing the outer delegate frame will destroy the inner WebContents. This
  // should only be called on the main frame.
  DCHECK(frame_tree_node_->IsMainFrame());
  FrameTreeNode* outer_delegate_frame_tree_node = GetOuterDelegateNode();
  DCHECK(outer_delegate_frame_tree_node->parent());
  outer_delegate_frame_tree_node->frame_tree().RemoveFrame(
      outer_delegate_frame_tree_node);
}

void RenderFrameHostManager::Stop() {
  render_frame_host_->Stop();

  // A loading speculative RenderFrameHost should also stop.
  if (speculative_render_frame_host_ &&
      speculative_render_frame_host_->is_loading()) {
    speculative_render_frame_host_->GetAssociatedLocalFrame()->StopLoading();
  }
}

void RenderFrameHostManager::SetIsLoading(bool is_loading) {
  render_frame_host_->render_view_host()->GetWidget()->SetIsLoading(is_loading);
}

void RenderFrameHostManager::BeforeUnloadCompleted(bool proceed) {
  // If beforeunload was dispatched as part of preparing this frame for
  // attaching an inner delegate, continue attaching now.
  if (is_attaching_inner_delegate()) {
    DCHECK(frame_tree_node_->parent());
    if (proceed) {
      CreateNewFrameForInnerDelegateAttachIfNecessary();
    } else {
      NotifyPrepareForInnerDelegateAttachComplete(false /* success */);
    }
    return;
  }

  bool proceed_to_fire_unload = false;
  delegate_->BeforeUnloadFiredFromRenderManager(proceed,
                                                &proceed_to_fire_unload);
  if (proceed_to_fire_unload) {
    // If we're about to close the tab and there's a speculative RFH, cancel it.
    // Otherwise, if the navigation in the speculative RFH completes before the
    // close in the current RFH, we'll lose the tab close.
    // TODO(crbug.com/40252524): This condition may no longer be needed.
    if (speculative_render_frame_host_) {
      DiscardSpeculativeRFH(NavigationDiscardReason::kWillRemoveFrame);
    }

    // TODO(crbug.com/40252524): This is not always browser-initiated, so
    // we should track whether the close is browser or renderer-initiated and
    // use that here.
    render_frame_host_->ClosePage(
        RenderFrameHostImpl::ClosePageSource::kBrowser);
  }
}

void RenderFrameHostManager::DidNavigateFrame(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture,
    bool is_same_document_navigation,
    bool clear_proxies_on_commit,
    const blink::FramePolicy& frame_policy,
    bool allow_paint_holding) {
  CommitPendingIfNecessary(render_frame_host, was_caused_by_user_gesture,
                           is_same_document_navigation, clear_proxies_on_commit,
                           allow_paint_holding);

  // Make sure any dynamic changes to this frame's sandbox flags and permissions
  // policy that were made prior to navigation take effect.  This should only
  // happen for cross-document navigations.
  if (!is_same_document_navigation) {
    if (!render_frame_host->browsing_context_state()->CommitFramePolicy(
            frame_policy)) {
      // The frame policy didn't change, no need to send updates to proxies.
      return;
    }

    // There should be no children of this frame; any policy changes should only
    // happen on navigation commit which will delete any child frames.
    DCHECK(!frame_tree_node_->child_count());

    if (!frame_tree_node_->parent()) {
      // Policy updates for root node happens only when the frame is a fenced
      // frame root.
      // Note: SendFramePolicyUpdatesToProxies doesn't need to be invoked for
      // MPArch fenced frames, because the root fenced frame must use a static
      // policy not to introduce a communication channel.
      CHECK(frame_tree_node_->IsFencedFrameRoot());
    } else {
      render_frame_host_->browsing_context_state()
          ->SendFramePolicyUpdatesToProxies(
              frame_tree_node_->parent()->GetSiteInstance()->group(),
              frame_policy);
    }
  }
}

void RenderFrameHostManager::CommitPendingIfNecessary(
    RenderFrameHostImpl* render_frame_host,
    bool was_caused_by_user_gesture,
    bool is_same_document_navigation,
    bool clear_proxies_on_commit,
    bool allow_paint_holding) {
  if (!speculative_render_frame_host_) {
    // There's no speculative RenderFrameHost so it must be that the current
    // RenderFrameHost completed a navigation.
    CHECK_EQ(render_frame_host_.get(), render_frame_host);
  }

  if (render_frame_host == speculative_render_frame_host_.get()) {
    // A cross-RenderFrameHost navigation completed, so show the new renderer.
    CommitPending(std::move(speculative_render_frame_host_),
                  std::move(stored_page_to_restore_), clear_proxies_on_commit,
                  allow_paint_holding);

    if (GetNavigationQueueingFeatureLevel() >=
        NavigationQueueingFeatureLevel::kAvoidRedundantCancellations) {
      // When avoiding redundant navigation cancellations, if there are other
      // navigation requests that are ongoing, set their "associated
      // RenderFrameHost type" NONE, as the old type may no longer be accurate:
      // - If it was previously set to CURRENT, the current RenderFrameHost
      // had already changed to the previously-speculative RenderFrameHost. It
      // most likely will commit to a new speculative RenderFrameHost, but that
      // doesn't exist yet and so we shouldn't change the type to SPECULATIVE.
      // - If it was previously set to SPECULATIVE, the previously-speculative
      // RenderFrameHost is no longer speculative. However we can't just set the
      // type to CURRENT, as the navigation might actually want to create a new
      // speculative RenderFrameHost too and not reuse the now-current RFH
      // (e.g., with RenderDocument).
      // A new "associated RenderFrameHost" type value will be recalculated when
      // the navigation recalculates its RenderFrameHost either at
      // StartNavigation (if it hasn't reached that stage yet) or ReadyToCommit
      // time. Note that we don't update this value for pending commit
      // navigations (and hence we only check the FrameTreeNode's
      // NavigationRequest), as the value is only used until before the
      // navigation gets to the "pending commit" stage.
      if (frame_tree_node_->navigation_request()) {
        frame_tree_node_->navigation_request()->SetAssociatedRFHType(
            NavigationRequest::AssociatedRenderFrameHostType::NONE);
      }
    } else {
      // Otherwise, if not attempting to avoid redundant cancellations, cancel
      // any other navigations that are ongoing if they're not pending commit.
      // Note that the pending commit navigations that are in the old RFH will
      // get deleted when the old RFH gets unloaded.
      frame_tree_node_->ResetNavigationRequest(
          NavigationDiscardReason::kCommittedNavigation);
    }
    return;
  }

  // A same-RenderFrameHost navigation committed.

  if (render_frame_host_->is_local_root() && render_frame_host_->GetView()) {
    // RenderFrames are created with a hidden RenderWidgetHost. When
    // navigation finishes, we show it if the delegate is shown. CommitPending()
    // takes care of this in the cross-process case, as well as other cases
    // where a RenderFrameHost is swapped in.
    if (!frame_tree_node_->frame_tree().IsHidden())
      render_frame_host_->GetView()->Show();

    // TODO(crbug.com/40264716): For same RenderFrameHost, it isn't clear
    // whether we should start the new content timer, but to be safe, we start
    // it here. The TODO here is to remove this call when we can.
    //
    // Note that this is only OK to do for non-prerender. For prerendering path,
    // setting this timeout is incorrect because it causes a clear of graphical
    // output on prerender activation.
    if (render_frame_host_->lifecycle_state() !=
        LifecycleStateImpl::kPrerendering) {
      auto* rwhi = static_cast<RenderWidgetHostImpl*>(
          render_frame_host_->GetView()->GetRenderWidgetHost());

      rwhi->StartNewContentRenderingTimeout();
      // Force the timer to expire immediately if we don't allow main frame
      // paint holding.
      if (frame_tree_node_->IsMainFrame() && !allow_paint_holding) {
        // We post task here, since this evicts a surface but the embedding of a
        // new surface would be done in the same stack as this call. The
        // ordering of whether the new surface has or has not yet been embedded
        // differs for different platforms, and we always want the new surface
        // to be embedded before we evict. Hence, we post a task. In practice
        // this still disables paint holding unless this task is delayed for a
        // long time.
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                &RenderWidgetHostImpl::ForceFirstFrameAfterNavigationTimeout,
                rwhi->GetWeakPtr()));
      }
    }
  }

  // If we are navigating away from a Page that has a form data associated with
  // it, record the metrics indicating that the Page was navigated away but
  // wasn't eligible for BFCache. Note that the metrics recording for the
  // cross-RFH case happens in RenderFrameHostManager::UnloadOldFrame().
  // We only care about main frame cross-document navigation since those are
  // the ones that can trigger BFCache.
  if (!render_frame_host->GetParentOrOuterDocument() &&
      !is_same_document_navigation) {
    BackForwardCacheMetrics* metrics =
        render_frame_host->GetBackForwardCacheMetrics();
    if (metrics && metrics->had_form_data_associated()) {
      UMA_HISTOGRAM_ENUMERATION(
          kBackForwardCachePageWithFormStorableHistogramName,
          BackForwardCacheMetrics::PageWithFormStorable::kPageSeen);
    }
  }
}

void RenderFrameHostManager::DidChangeOpener(
    const std::optional<blink::LocalFrameToken>& opener_frame_token,
    SiteInstanceGroup* source_site_instance_group) {
  FrameTreeNode* opener = nullptr;
  if (opener_frame_token) {
    RenderFrameHostImpl* opener_rfhi = RenderFrameHostImpl::FromFrameToken(
        source_site_instance_group->process()->GetID(), *opener_frame_token);
    // If |opener_rfhi| is null, the opener RFH has already disappeared.  In
    // this case, clear the opener rather than keeping the old opener around.
    if (opener_rfhi)
      opener = opener_rfhi->frame_tree_node();
  }

  if (frame_tree_node_->opener() == opener)
    return;

  frame_tree_node_->SetOpener(opener);

  render_frame_host_->browsing_context_state()->UpdateOpener(
      source_site_instance_group);

  if (render_frame_host_->GetSiteInstance()->group() !=
      source_site_instance_group) {
    UpdateOpener(render_frame_host_.get());
  }

  // Notify the speculative RenderFrameHosts as well.  This is necessary in case
  // a process swap has started while the message was in flight.
  if (speculative_render_frame_host_ &&
      speculative_render_frame_host_->GetSiteInstance()->group() !=
          source_site_instance_group) {
    UpdateOpener(speculative_render_frame_host_.get());
  }
}

std::unique_ptr<StoredPage> RenderFrameHostManager::TakePrerenderedPage() {
  DCHECK(frame_tree_node_->IsMainFrame());
  auto main_render_frame_host = SetRenderFrameHost(nullptr);
  return CollectPage(std::move(main_render_frame_host));
}

void RenderFrameHostManager::PrepareForCollectingPage(
    RenderFrameHostImpl* main_render_frame_host,
    StoredPage::RenderViewHostImplSafeRefSet* render_view_hosts,
    BrowsingContextState::RenderFrameProxyHostMap* proxy_hosts) {
  TRACE_EVENT("navigation", "RenderFrameHostManager::PrepareForCollectingPage");

  // We insert RenderViewHosts for all frames.
  main_render_frame_host->ForEachRenderFrameHost([&](RenderFrameHostImpl* rfh) {
    render_view_hosts->insert(rfh->render_view_host()->GetSafeRef());
    if (rfh->is_main_frame()) {
      for (auto& it : rfh->browsing_context_state()->proxy_hosts()) {
        // This avoids including the proxy created when starting a
        // new cross-process, cross-BrowsingInstance navigation, as well as any
        // restored proxies which are also in a different BrowsingInstance.
        if (rfh->GetSiteInstance()->group()->IsRelatedSiteInstanceGroup(
                it.second->site_instance_group())) {
          render_view_hosts->insert(
              it.second->GetRenderViewHost()->GetSafeRef());
        }
      }
    }
  });

  // When BrowsingContextState is decoupled from the FrameTreeNode and
  // RenderFrameHostManager (legacy mode is disabled), proxies and
  // replication state will be stored in a separate BrowsingContextState,
  // which won't need any updates. However, RenderViewHosts are still stored
  // in FrameTree (which, for example, is shared between the new page and
  // the page entering BFCache), so they have to be collected explicitly above.
  // Since proxies are not collected, we can return early here.
  if (features::GetBrowsingContextMode() ==
      features::BrowsingContextStateImplementationType::
          kSwapForCrossBrowsingInstanceNavigations) {
    return;
  }

  DCHECK_EQ(features::GetBrowsingContextMode(),
            features::BrowsingContextStateImplementationType::
                kLegacyOneToOneWithFrameTreeNode);

  // Prepare the proxies.
  SiteInstanceGroup* group = main_render_frame_host->GetSiteInstance()->group();

  // Store the proxies only for main frame in the primary FrameTree because the
  // FrameTreeNode gets reused for back/forward cache. It is not needed to
  // store proxies for embedded main frames since each have their unique
  // FrameTreeNode and their own BrowsingContextState.
  for (auto& it :
       main_render_frame_host->browsing_context_state()->proxy_hosts()) {
    // This avoids including the proxy created when starting a
    // new cross-process, cross-BrowsingInstance navigation, as well as any
    // restored proxies which are also in a different BrowsingInstance.
    if (group->IsRelatedSiteInstanceGroup(it.second->site_instance_group())) {
      DCHECK(base::Contains(*render_view_hosts,
                            it.second->GetRenderViewHost()->GetSafeRef()));
      auto pair = proxy_hosts->insert({it.first, std::move(it.second)});
      bool insertion_took_place = pair.second;
      // There should be only one proxy for any given SiteInstanceGroup, so this
      // should never replace an existing element.
      CHECK(insertion_took_place);
    }
  }

  // Remove the previously extracted proxies from the
  // RenderFrameHostManager, which also removes their respective
  // SiteInstanceGroup::Observer.
  for (auto& it : *proxy_hosts) {
    main_render_frame_host->browsing_context_state()
        ->DeleteRenderFrameProxyHost(it.second->site_instance_group());
  }
}

std::unique_ptr<StoredPage> RenderFrameHostManager::CollectPage(
    std::unique_ptr<RenderFrameHostImpl> main_render_frame_host) {
  DCHECK(main_render_frame_host->is_main_frame());

  StoredPage::RenderViewHostImplSafeRefSet render_view_hosts;
  BrowsingContextState::RenderFrameProxyHostMap proxy_hosts;

  PrepareForCollectingPage(main_render_frame_host.get(), &render_view_hosts,
                           &proxy_hosts);

  auto stored_page = std::make_unique<StoredPage>(
      std::move(main_render_frame_host), std::move(proxy_hosts),
      std::move(render_view_hosts));
  return stored_page;
}

void RenderFrameHostManager::UpdateOpener(
    RenderFrameHostImpl* render_frame_host) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::UpdateOpener",
               "render_frame_host", render_frame_host);

  // `render_frame_host` (the frame whose opener is being updated) might not
  // have had proxies for the new opener chain in its SiteInstance's group. Make
  // sure they exist.
  if (frame_tree_node_->opener()) {
    frame_tree_node_->opener()->render_manager()->CreateOpenerProxies(
        render_frame_host->GetSiteInstance()->group(), frame_tree_node_,
        render_frame_host->browsing_context_state());
  }

  auto opener_frame_token =
      GetOpenerFrameToken(render_frame_host->GetSiteInstance()->group());
  render_frame_host->GetAssociatedLocalFrame()->UpdateOpener(
      opener_frame_token);
}

void RenderFrameHostManager::UnloadOldFrame(
    std::unique_ptr<RenderFrameHostImpl> old_render_frame_host) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::UnloadOldFrame",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());

  // If the old RFH is not live, just return as there is no further work to do.
  // It will be deleted and there will be no proxy created.
  if (!old_render_frame_host->IsRenderFrameLive())
    return;

  // Reset any NavigationRequest in the RenderFrameHost. An unloaded
  // RenderFrameHost should not be trying to commit a navigation.
  // TODO(crbug.com/40186427): Ensure that there are no pending commit
  // cross-document NavigationRequests at this point. With navigation queuing,
  // this will be guaranteed because there will be only 1 pending commit
  // navigation at a time, which will be the navigation in the speculative
  // RenderFrameHost that replaced `old_render_frame_host`.
  old_render_frame_host->ResetOwnedNavigationRequests(
      NavigationDiscardReason::kCommittedNavigation);

  NavigationEntryImpl* last_committed_entry =
      GetNavigationController().GetLastCommittedEntry();
  BackForwardCacheMetrics* old_page_back_forward_cache_metrics =
      !old_render_frame_host->GetParentOrOuterDocument()
          ? last_committed_entry->back_forward_cache_metrics()
          : nullptr;

  // Record the metrics about the state of the old main frame at the moment when
  // we navigate away from it as it matters for whether the page is eligible for
  // being put into back-forward cache.
  //
  // This covers the cross-process navigation case and the same-process case is
  // handled in RenderFrameHostImpl::CommitNavigation, so the subframe state
  // can be captured before the frame navigates away.
  //
  // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
  // implementing back-forward cache.
  if (old_page_back_forward_cache_metrics) {
    old_page_back_forward_cache_metrics->RecordFeatureUsage(
        old_render_frame_host.get());
  }

  // BackForwardCache:
  //
  // If the old RenderFrameHost can be stored in the BackForwardCache, return
  // early without unloading and running unload handlers, as the document may
  // be restored later.
  if (!old_render_frame_host->GetParentOrOuterDocument()) {
    BackForwardCacheImpl& back_forward_cache =
        GetNavigationController().GetBackForwardCache();

    // The result of this eligibility check will only include sticky reasons.
    // Non-sticky reasons will be checked later and if any, the page will be
    // evicted from BFCache.
    BackForwardCacheCanStoreDocumentResultWithTree bfcache_eligibility =
        back_forward_cache.GetCurrentBackForwardCacheEligibility(
            old_render_frame_host.get());
    bool can_store = bfcache_eligibility.CanStore();
    if (old_page_back_forward_cache_metrics &&
        old_page_back_forward_cache_metrics->had_form_data_associated()) {
      UMA_HISTOGRAM_ENUMERATION(
          kBackForwardCachePageWithFormStorableHistogramName,
          BackForwardCacheMetrics::PageWithFormStorable::kPageSeen);
      if (can_store) {
        UMA_HISTOGRAM_ENUMERATION(
            kBackForwardCachePageWithFormStorableHistogramName,
            BackForwardCacheMetrics::PageWithFormStorable::kPageStored);
      }
    }
    TRACE_EVENT("navigation", "BackForwardCache_MaybeStorePage",
                "old_render_frame_host", old_render_frame_host,
                "bfcache_eligibility",
                bfcache_eligibility.flattened_reasons.ToString());
    if (can_store) {
      bool is_same_process =
          (old_render_frame_host->GetProcess() ==
           frame_tree_node_->current_frame_host()->GetProcess());
      if (old_render_frame_host->GetSiteInstance()->IsSameSiteWithURL(
              frame_tree_node_->current_url())) {
        base::UmaHistogramBoolean("BackForwardCache.ProcessReuse.SameSite",
                                  is_same_process);
      } else {
        base::UmaHistogramBoolean("BackForwardCache.ProcessReuse.CrossSite",
                                  is_same_process);
      }
      if (old_render_frame_host->GetSiteInstance()
              ->GetRelatedActiveContentsCount() > 0) {
        SCOPED_CRASH_KEY_NUMBER("rvh-double", "related_active_contents",
                                old_render_frame_host->GetSiteInstance()
                                    ->GetRelatedActiveContentsCount());
        SCOPED_CRASH_KEY_BOOL("rvh-double", "is_same_process", is_same_process);
        base::debug::DumpWithoutCrashing();
      }

      auto stored_page = CollectPage(std::move(old_render_frame_host));
      auto entry =
          std::make_unique<BackForwardCacheImpl::Entry>(std::move(stored_page));
      // Ensures RenderViewHosts are not reused while they are in the cache.
      for (const auto& rvh : entry->render_view_hosts()) {
        rvh->EnterBackForwardCache();
      }
      back_forward_cache.StoreEntry(std::move(entry));
      return;
    }

    if (old_page_back_forward_cache_metrics) {
      // Reasons set in the metrics object will be used for DevTools and
      // NotRestoredReasons API. We should include non-sticky reasons as well
      // here for better debugging, though non-sticky features might get cleaned
      // in pagehide handlers.
      BackForwardCacheCanStoreDocumentResultWithTree
          eligibility_including_non_sticky =
              back_forward_cache
                  .GetCompleteBackForwardCacheEligibilityForReporting(
                      old_render_frame_host.get());
      old_page_back_forward_cache_metrics->SetNotRestoredReasons(
          eligibility_including_non_sticky);
    }
  }

  // Create a replacement proxy for the old RenderFrameHost when we're switching
  // SiteInstanceGroups. There should not be one yet. This is done even if there
  // are no active frames besides this one to simplify cleanup logic on the
  // renderer side. See https://crbug.com/568836 for motivation.
  RenderFrameProxyHost* proxy = nullptr;
  if (render_frame_host_->GetSiteInstance()->group() !=
      old_render_frame_host->GetSiteInstance()->group()) {
    proxy =
        old_render_frame_host->browsing_context_state()
            ->CreateRenderFrameProxyHost(
                old_render_frame_host->GetSiteInstance()->group(),
                old_render_frame_host->render_view_host(), frame_tree_node_);
  }

  // |old_render_frame_host| will be deleted when its unload ACK is received,
  // or when the timer times out, or when the RFHM itself is deleted (whichever
  // comes first).
  auto insertion =
      pending_delete_hosts_.insert(std::move(old_render_frame_host));
  // Tell the old RenderFrameHost to swap out and be replaced by the proxy.
  (*insertion.first)->Unload(proxy, true);
}

void RenderFrameHostManager::DiscardUnusedFrame(
    std::unique_ptr<RenderFrameHostImpl> render_frame_host) {
  // RenderDocument: In the case of a local<->local RenderFrameHost swap, just
  // discard the RenderFrameHost. There are no other proxies associated.
  // SiteInstanceGroup: RenderFrameHosts in the same SiteInstanceGroup are all
  // local frames, even if they have different SiteInstances.
  if (render_frame_host->GetSiteInstance()->group() ==
      render_frame_host_->GetSiteInstance()->group()) {
    return;  // |render_frame_host| is released here.
  }

  // TODO(carlosk): this code is very similar to what can be found in
  // UnloadOldFrame and we should see that these are unified at some point.

  // If the SiteInstanceGroup for the pending RFH is being used by others,
  // ensure that the pending RenderFrameHost is replaced by a
  // RenderFrameProxyHost to allow other frames to communicate to this frame.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  RenderFrameProxyHost* proxy = nullptr;
  if (site_instance->HasSite() &&
      site_instance->group()->active_frame_count() > 1) {
    // A proxy already exists for the SiteInstanceGroup that |site_instance|
    // belongs to, so just reuse it. There is no need to call Unload() on the
    // |render_frame_host|, as this method is only called to discard a pending
    // or speculative RenderFrameHost, i.e. one that has never hosted an actual
    // document.
    proxy =
        render_frame_host->browsing_context_state()->GetRenderFrameProxyHost(
            site_instance->group());
    CHECK(proxy);
  }

  render_frame_host.reset();

  // If the old proxy isn't live, create the `blink::RemoteFrame` in the
  // renderer, so that other frames can still communicate with this frame.  See
  // https://crbug.com/653746.
  if (proxy && !proxy->is_render_frame_proxy_live())
    proxy->InitRenderFrameProxy();
}

bool RenderFrameHostManager::DeleteFromPendingList(
    RenderFrameHostImpl* render_frame_host) {
  auto it = pending_delete_hosts_.find(render_frame_host);
  if (it == pending_delete_hosts_.end())
    return false;
  pending_delete_hosts_.erase(it);
  return true;
}

// Prerender navigations match a prerender after calling
// GetFrameHostForNavigation, which means we might create a speculative RFH and
// then try to replace it with the prerendered RFH during activation. We can not
// just reset this RFH in RestorePage as the RFH would be in an invalid state
// for destruction. We need to properly clean up first. Hence this method.
// TODO(crbug.com/40174053): We should refactor prerender matching flow
// to ensure that we do not create speculative RFHs for prerender activation.
void RenderFrameHostManager::ActivatePrerender(
    std::unique_ptr<StoredPage> stored_page) {
  if (speculative_render_frame_host_) {
    DiscardUnusedFrame(UnsetSpeculativeRenderFrameHost(
        NavigationDiscardReason::kInternalCancellation));
  }

  // Reset the swap result of BrowsingInstance as prerender activation always
  // swaps BrowsingInstance.
  BackForwardCacheMetrics* back_forward_cache_metrics =
      render_frame_host_->GetBackForwardCacheMetrics();
  if (back_forward_cache_metrics)
    back_forward_cache_metrics->SetBrowsingInstanceSwapResult(std::nullopt,
                                                              nullptr);

  RestorePage(std::move(stored_page));
}

void RenderFrameHostManager::RestorePage(
    std::unique_ptr<StoredPage> stored_page) {
  TRACE_EVENT("navigation", "RenderFrameHostManager::RestorePage",
              ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);
  // Matched in CommitPending().
  stored_page->render_frame_host()->GetProcess()->AddPendingView();

  // speculative_render_frame_host_ and stored_page_to_restore_ will be
  // consumed during CommitPendingIfNecessary.
  // TODO(crbug.com/40276805): This is awkward to leave the entry in a
  // half consumed state and it would be clearer if we could not reuse
  // speculative_render_frame_host in the long run. For now, and to avoid
  // complex edge cases, we simply reuse it to preserve the understood logic in
  // CommitPending.

  // There should be no speculative RFH at this point. With BackForwardCache, it
  // should have never been created, and with prerender activation, it should
  // have been cleared out earlier. If a speculative RenderFrameHost used for
  // another NavigationRequest existed, then it must be a pending commit RFH,
  // which would delay the activation navigation from getting here (see also
  // ConcurrentNavigationsCommitDeferringCondition) until the pending commit
  // RFH finished the commit and becomes the current RenderFrameHost.
  DCHECK(!speculative_render_frame_host_);
  SCOPED_CRASH_KEY_BOOL("Bug1407526", "spec_rfh_exists",
                        !!speculative_render_frame_host_);
  speculative_render_frame_host_ = stored_page->TakeRenderFrameHost();
  // Now |stored_page| is destroyed and thus does not monitor cookie changes any
  // more. This is okay as eviction would not happen from this point.
  stored_page_to_restore_ = std::move(stored_page);
}

void RenderFrameHostManager::ClearRFHsPendingShutdown() {
  pending_delete_hosts_.clear();
}

void RenderFrameHostManager::ClearWebUIInstances() {
  current_frame_host()->ClearWebUI();
  if (speculative_render_frame_host_)
    speculative_render_frame_host_->ClearWebUI();
}

bool RenderFrameHostManager::HasPendingCommitForCrossDocumentNavigation()
    const {
  if (render_frame_host_->HasPendingCommitForCrossDocumentNavigation())
    return true;
  if (speculative_render_frame_host_) {
    return speculative_render_frame_host_
        ->HasPendingCommitForCrossDocumentNavigation();
  }
  return false;
}

void RenderFrameHostManager::DidCreateNavigationRequest(
    NavigationRequest* request) {
  TRACE_EVENT("navigation",
              "RenderFrameHostManager::DidCreateNavigationRequest",
              ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);

  const bool force_use_current_render_frame_host =
      // Since the frame from the back-forward cache is being committed to the
      // SiteInstance we already have, it is treated as current.
      request->IsServedFromBackForwardCache() ||
      // Avoid calling GetFrameHostForNavigation() for same-document navigations
      // since they should always occur in the current document, which means
      // also in the current SiteInstance.
      // State may have changed in the browser that would cause us to put the
      // document in a different SiteInstance if it was loaded again now, but we
      // do not want to load the document again, see https://crbug.com/1125106.
      request->IsSameDocument();

  if (force_use_current_render_frame_host) {
    // This method should generally be calling GetFrameHostForNavigation() in
    // order to choose the correct RenderFrameHost, and choose a speculative
    // RenderFrameHost when the navigation can not be performed in the current
    // frame. Getting this wrong has security consequences as it could allow a
    // document from a different security context to be loaded in the current
    // frame and gain access to things in-process that it should not.
    // However, there are some situations where we know that we want to perform
    // the navigation in the current frame. In that case we must be sure that
    // the renderer is not *controlling* the navigation. The BeginNavigation()
    // path allows the renderer to specify all the parameters of the
    // NavigationRequest, so we should never allow it to specify that the
    // navigation be performed in the current RenderFrameHost.
    CHECK(!request->from_begin_navigation());

    request->SetAssociatedRFHType(
        NavigationRequest::AssociatedRenderFrameHostType::CURRENT);

    // Cleanup existing speculative RenderFrameHost. This corresponds to
    // what is done inside GetFrameHostForNavigation(request), but we avoid
    // calling that method for navigations which will be forced into the current
    // document.
    if (ShouldAvoidRedundantNavigationCancellations()) {
      // When avoiding redundant navigation cancellations, only delete the
      // speculative RFH if it is unused. In particular, this means that a
      // speculative RFH with a pending-commit navigation won't be deleted
      // anymore.
      DiscardSpeculativeRFHIfUnused(
          request->GetTypeForNavigationDiscardReason());
    } else {
      // When the flag is disabled, always delete the speculative RFH, even if
      // it means cancelling a pending commit navigation in that RFH.
      DiscardSpeculativeRFH(request->GetTypeForNavigationDiscardReason());
    }
  } else {
    base::ElapsedTimer timer;
    BrowsingContextGroupSwap ignored_bcg_swap_info =
        BrowsingContextGroupSwap::CreateDefault();
    auto result = GetFrameHostForNavigation(request, &ignored_bcg_swap_info);
    if (result.has_value()) {
      DCHECK(result.value());
    } else if (result.error() ==
               GetFrameHostForNavigationFailed::kBlockedByPendingCommit) {
      frame_tree_node_->render_manager()
          ->speculative_frame_host()
          ->RecordMetricsForBlockedGetFrameHostAttempt(
              /* commit_attempt=*/false);
    }
    if (request->GetURL().SchemeIsHTTPOrHTTPS()) {
      base::UmaHistogramMicrosecondsTimes(
          "Navigation.GetFrameHostForNavigationTime"
          ".InDidCreateNavigationRequest.IsHTTPOrHTTPS",
          timer.Elapsed());
    }
  }
}

void RenderFrameHostManager::PerformEarlyRenderFrameHostSwapIfNeeded(
    NavigationRequest* request,
    bool is_called_after_did_start_navigation) {
  // The early swap is possible only when there's a speculative RenderFrameHost
  // to swap with the current one.
  if (!speculative_render_frame_host_) {
    return;
  }

  // Check if this is for a prerendered FrameTree. Note that we cannot check
  // the RFH's LifecycleState here instead, because it will be kSpeculative
  // even for prerendering RFHs at this point.
  //
  // For prerendering FrameTrees, skip the early swap to explicitly avoid a
  // LifecycleState transition from kSpeculative directly to kPrerender, and
  // force it to go through the regular path instead (i.e. through
  // kPendingCommit).
  if (frame_tree_node_->frame_tree().is_prerendering()) {
    return;
  }

  // Currently, the early swap might be invoked in two places:
  // - (Legacy timing) At the very beginning of navigation, as part of picking
  //   the target RenderFrameHost via GetFrameHostForNavigation().
  // - (New timing) After DidStartNavigation has been dispatched to observers
  //   and WillStartRequest navigation throttle events have been processed.
  //
  // `is_called_after_did_start_navigation` determines which timing was used
  // (legacy timing when false, new timing when true).  Currently, the legacy
  // timing is used when doing early RenderFrameHost swap for initial and
  // crashed frames. We want to only have the new timing and to move all early
  // swaps to happen after DidStartNavigation/WillStartRequest.
  // See crbug.com/1467011.
  if (is_called_after_did_start_navigation) {
    return;
  }

  using EarlySwapType = NavigationRequest::EarlyRenderFrameHostSwapType;
  EarlySwapType early_swap_type = EarlySwapType::kNone;

  if (!render_frame_host_->IsRenderFrameLive()) {
    // Currently, non-live frames do the early swap before reaching
    // DidStartNavigation.  This is possible in two cases: (1) if a frame's
    // process dies (e.g., due to a crash or OOM), and (2) if we navigate a
    // frame immediately after its creation, and the navigation cannot reuse the
    // initial non-live RFH and must create a speculative RFH.  For case (1),
    // must_be_replaced() will always be true, but note that there's also an
    // experimental feature that skips the early swap for case (1).  Case (2) is
    // possible in cases like WebUI, <webview> tags, and dynamic isolation on
    // Android.
    if (render_frame_host_->must_be_replaced()) {
      if (!ShouldSkipEarlyCommitPendingForCrashedFrame()) {
        // Note that we're being slightly imprecise here by using
        // kCrashedFrame for must_be_replaced(), which includes all cases
        // where a RenderFrameHost has had a process in the past but then lost
        // it via RenderProcessGone, which also includes cases like OOM.
        early_swap_type = EarlySwapType::kCrashedFrame;
      }
    } else {
      early_swap_type = EarlySwapType::kInitialFrame;
    }
  }

  if (early_swap_type == EarlySwapType::kNone) {
    return;
  }

  // Now, proceed with the early swap. There's no reason to sit around with a
  // sad tab or a newly created RFH while we wait for the navigation to
  // complete. Just switch to the speculative RFH now and allow the navigation
  // to proceed in that now-current RFH.
  //
  // TODO(alexmos,creis): Note that we currently don't care about
  // on{before}unload handlers because the current RFH isn't live.  However, if
  // we start doing early RFH swap for non-live current RFHs, we will need to
  // revisit this and ensure that beforeunload handlers run before the swap.
  //
  // If the corresponding RenderFrame is currently associated with a
  // proxy, send a SwapIn message to ensure that the RenderFrame swaps
  // into the frame tree and replaces that proxy on the renderer side.
  // Normally this happens at navigation commit time, but in this case
  // this must be done earlier to keep browser and renderer state in sync.
  // This is important to do before CommitPending(), which destroys the
  // corresponding proxy. See https://crbug.com/487872.
  // TODO(crbug.com/40052076): Make this logic more robust to
  // consider the case for failed navigations after CommitPending.
  RenderFrameHostImpl* speculative_rfh = speculative_render_frame_host_.get();
  if (speculative_rfh->browsing_context_state()->GetRenderFrameProxyHost(
          speculative_rfh->GetSiteInstance()->group())) {
    speculative_rfh->SwapIn();
  }
  speculative_rfh->OnCommittedSpeculativeBeforeNavigationCommit();

  // An Active RenderFrameHost MUST always have a PolicyContainerHost. A new
  // document is either:
  // - The initial empty document, via frame creation.
  // - A new document replacing the previous one, via a navigation.
  // Here this is an additional case: A new document (in a weird state) is
  // replacing the one crashed. In this case, it is not entirely clear what
  // PolicyContainerHost should be used. In the absence of anything better,
  // we simply keep the PolicyContainerHost that was previously active.
  speculative_rfh->SetPolicyContainerForEarlyCommitAfterCrash(
      current_frame_host()->policy_container_host()->Clone());

  if (request->HasWebUI()) {
    // If a WebUI has been created for the NavigationRequest, set it on the
    // RenderFrameHost picked for the navigation. Note that there is a
    // similar WebUI handling near the end of GetFrameHostForNavigation to
    // cover the non-early commit cases, which won't run if we already run this
    // code because `HasWebUI()` will return false after we take the WebUI from
    // the NavigationRequest here.
    //
    // TODO(crbug.com/40276607): Remove this logic after the early swap is moved
    // to happen after GetFrameHostForNavigation, rather than in the middle of
    // it.
    speculative_rfh->SetWebUI(*request);
    CHECK(speculative_rfh->web_ui());
  }

  CommitPending(
      std::move(speculative_render_frame_host_),
      /*pending_stored_page=*/nullptr,
      request->browsing_context_group_swap().ShouldClearProxiesOnCommit(),
      /*allow_paint_holding=*/false);
  request->SetAssociatedRFHType(
      NavigationRequest::AssociatedRenderFrameHostType::CURRENT);

  request->set_early_render_frame_host_swap_type(early_swap_type);
}

base::expected<RenderFrameHostImpl*, GetFrameHostForNavigationFailed>
RenderFrameHostManager::GetFrameHostForNavigation(
    NavigationRequest* request,
    BrowsingContextGroupSwap* browsing_context_group_swap,
    std::string* reason) {
  // GetFrameHostForNavigation will be called more than once during a navigation
  // (currently twice, on request and when it's about to commit in the
  // renderer).
  TRACE_EVENT("navigation", "RenderFrameHostManager::GetFrameHostForNavigation",
              ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.GetFrameHostForNavigation");

  DCHECK(!request->common_params().url.SchemeIs(url::kJavaScriptScheme))
      << "Don't call this method for JavaScript URLs as those create a "
         "temporary  NavigationRequest and we don't want to reset an ongoing "
         "navigation's speculative RFH.";
  // Same-document navigations should be committed in the current document
  // (and current RenderFrameHost), so we should not come here and ask where
  // we would load that document. The resulting SiteInstance may have changed
  // since we did load the current document, but we don't want to reload it if
  // that is the case. See crbug.com/1125106.
  DCHECK(!request->IsSameDocument());
  // TODO(crbug.com/40055210): Verify that we're not resetting the document
  // sequence number in a same-document navigation. This method will reset it
  // if the site instance changed. But this method should not be called for a
  // same document history navigation. Change back to a DCHECK() once this is
  // resolved.
  if (request->IsSameDocument())
    base::debug::DumpWithoutCrashing();

  // Navigations for inactive frames should be disallowed, except for the
  // following two cases:
  // 1) Prerendering. Even though prerendering is
  // considered an inactive state (i.e., not allowed to show any UI changes) it
  // is still allowed to navigate, fetch, load and run documents in the
  // background.
  // 2) Subframes in BFCached pages that have not (or will never) sent network
  // requests, if kEnableBackForwardCacheForOngoingSubframeNavigation is
  // enabled. Find more details in https://crbug.com/1511153.
  if (base::FeatureList::IsEnabled(
          features::kEnableBackForwardCacheForOngoingSubframeNavigation) &&
      current_frame_host()->lifecycle_state() ==
          LifecycleStateImpl::kInBackForwardCache) {
    CHECK(!request->IsInMainFrame());
    CHECK(!request->NeedsUrlLoader() ||
          (!request->HasLoader() &&
           request->state() <=
               NavigationRequest::NavigationState::WILL_START_REQUEST));
  }
  if (!(current_frame_host()->lifecycle_state() ==
            LifecycleStateImpl::kPrerendering ||
        (base::FeatureList::IsEnabled(
             features::kEnableBackForwardCacheForOngoingSubframeNavigation) &&
         current_frame_host()->lifecycle_state() ==
             LifecycleStateImpl::kInBackForwardCache))) {
    // Inactive frames should never be navigated. If this happens, log a
    // DumpWithoutCrashing to understand the root cause. See
    // https://crbug.com/926820 and https://crbug.com/927705.
    if (current_frame_host()->IsInactiveAndDisallowActivation(
            DisallowActivationReasonId::kNavigatingInInactiveFrame)) {
      DUMP_WILL_BE_NOTREACHED() << "Navigation in an inactive frame";
      DEBUG_ALIAS_FOR_GURL(url, request->common_params().url);
      base::debug::DumpWithoutCrashing();
    }
  }

  // Speculative RFHs are deleted immediately.
  if (speculative_render_frame_host_)
    DUMP_WILL_BE_CHECK(!speculative_render_frame_host_->must_be_replaced());

  // The appropriate RenderFrameHost to commit the navigation.
  RenderFrameHostImpl* navigation_rfh = nullptr;

  // First compute the SiteInstance to use for the navigation.
  SiteInstanceImpl* current_site_instance =
      render_frame_host_->GetSiteInstance();
  bool is_same_site =
      render_frame_host_->IsNavigationSameSite(request->GetUrlInfo());

  IsSameSiteGetter is_same_site_getter(is_same_site);
  std::string site_instance_reason;
  scoped_refptr<SiteInstanceImpl> dest_site_instance =
      GetSiteInstanceForNavigationRequest(request, is_same_site_getter,
                                          browsing_context_group_swap,
                                          &site_instance_reason);
  SCOPED_CRASH_KEY_STRING256("rvh-double", "si_reason", site_instance_reason);
  if (reason) {
    reason->append(site_instance_reason);
  }

  // A subframe should always be in the same BrowsingInstance as the parent
  // (see also https://crbug.com/1107269).
  RenderFrameHostImpl* parent = frame_tree_node_->parent();
  DCHECK(!parent ||
         dest_site_instance->IsRelatedSiteInstance(parent->GetSiteInstance()));

  // The SiteInstance determines whether to switch RenderFrameHost or not.
  bool use_current_rfh = current_site_instance == dest_site_instance;
  if (!use_current_rfh) {
    AppendReason(reason,
                 "GetFrameHostForNavigation / mismatched-site-instance");
  }

  if (frame_tree_node_->IsOutermostMainFrame()) {
    // Same-site navigations could swap BrowsingInstance as well. But we only
    // want to clear window.name on cross-site cross-BrowsingInstance main frame
    // navigations.
    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#resetBCName.
    request->set_is_cross_site_cross_browsing_context_group(
        !is_same_site &&
        !dest_site_instance->IsRelatedSiteInstance(current_site_instance));
  }

  // If a crashed RenderFrameHost must not be reused, replace it by a
  // new one immediately.
  if (use_current_rfh && render_frame_host_->must_be_replaced()) {
    use_current_rfh = false;
    AppendReason(reason, "GetFrameHostForNavigation / rfh-crashed");
  }

  // Force using a different RenderFrameHost when RenderDocument is enabled.
  if (use_current_rfh &&
      render_frame_host_->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    use_current_rfh = false;
    AppendReason(reason,
                 "GetFrameHostForNavigation / RenderDocument-enforcement");
  }

  // Create WebUI objects for this navigation if it is needed. Note that we
  // create this earlier than the `use_current_rfh` if clause below to ensure
  // we still create the WebUI objects even if we return early due to the
  // kBlockedByPendingCommit case. After a RenderFrameHost has been picked for
  // this navigation (either now or later on after this function is called again
  // upon reaching OnResponseStarted, in the case of navigation queueing), the
  // ownership of the WebUIImpl will move from the NavigationRequest to the
  // RenderFrameHost.
  // Note: We need to create the WebUI objects early in the navigation even when
  // there is no RenderFrameHost to host it yet, because the creation of
  // WebUIImpl will trigger the creation of WebUI data sources, which is needed
  // for WebUI navigations to reach the OnResponseStarted stage.
  CreateWebUIForNavigationIfNeeded(request, dest_site_instance.get(),
                                   use_current_rfh);
  bool notify_webui_of_rf_creation = request->HasWebUI();

  // For navigation queueing, if the speculative RFH is already committing a
  // cross-document navigation, avoid discarding it here: the commit needs to
  // complete in order for the browser and the renderer state to remain in
  // sync. See https://crbug.com/838348.
  //
  // In theory, it would be possible to simply avoid discarding it (see the
  // later branch for avoiding redundant cancellations: however, this
  // navigation race should be fairly rare, so for navigation queueing, do the
  // simple thing and give up trying to assign a RenderFrameHost for the
  // navigation.
  // TODO: crbug.com/345382623 Verify if deferring the creation for WebUI pages
  // is safe.
  if (ShouldQueueNavigationsWhenPendingCommitRFHExists() &&
      request->ShouldQueueDueToExistingPendingCommitRFH()) {
    return base::unexpected(
        GetFrameHostForNavigationFailed::kBlockedByPendingCommit);
  }

  if (base::FeatureList::IsEnabled(features::kDeferSpeculativeRFHCreation) &&
      !use_current_rfh) {
    DeferSpeculativeRFHAction defer_action =
        DeferSpeculativeRFHAction::kNotDeferred;
    if (CanIntentionallyDeferSpeculativeRFHForRequest(
            request, current_site_instance->GetBrowserContext(),
            frame_tree_node_)) {
      if (features::kWarmupSpareProcessCreationWhenDeferRFH.Get() &&
          !dest_site_instance->HasProcess()) {
        SpareRenderProcessHostManagerImpl::Get().WarmupSpare(
            dest_site_instance->GetBrowserContext());
        defer_action =
            DeferSpeculativeRFHAction::kDeferredWithRenderProcessWarmUp;
      } else {
        defer_action =
            DeferSpeculativeRFHAction::kDeferredWithoutRenderProcessWarmUp;
      }
    }
    if (request->state() == NavigationRequest::NavigationState::NOT_STARTED) {
      base::UmaHistogramEnumeration("Navigation.DeferSpeculativeRFHAction",
                                    defer_action);
    }
    if (defer_action != DeferSpeculativeRFHAction::kNotDeferred) {
      AppendReason(reason, "GetFrameHostForNavigation / intentional-defer");
      return base::unexpected(
          GetFrameHostForNavigationFailed::kIntentionalDefer);
    }
  }

  // We only do this if the policy allows it and are recovering a crashed frame.
  bool recovering_without_early_commit =
      ShouldSkipEarlyCommitPendingForCrashedFrame() &&
      render_frame_host_->must_be_replaced();
  if (use_current_rfh) {
    AppendReason(reason, "GetFrameHostForNavigation / use-current-rfh");
    navigation_rfh = render_frame_host_.get();

    // Set the associated RenderFrameHost type for the navigation, and discard
    // existing speculative RenderFrameHost. This can exist when the navigation
    // initially used a speculative RenderFrameHost but got redirected and now
    // uses the current RenderFrameHost. Note that we need to update the
    // associated RenderFrameHost type first so that
    // `DiscardSpeculativeRFHIfUnused()` can work correctly.
    request->SetAssociatedRFHType(
        NavigationRequest::AssociatedRenderFrameHostType::CURRENT);
    if (ShouldAvoidRedundantNavigationCancellations()) {
      // When avoiding redundant navigation cancellations, only delete the
      // speculative RFH if it is unused.
      DiscardSpeculativeRFHIfUnused(
          request->GetTypeForNavigationDiscardReason());
    } else {
      // When the flag is disabled, always delete the speculative RFH, even if
      // it means cancelling a pending commit navigation in that RFH.
      DiscardSpeculativeRFH(request->GetTypeForNavigationDiscardReason());
    }
  } else {
    // If the current RenderFrameHost cannot be used a speculative one is
    // created with the SiteInstance for the current URL. If a speculative
    // RenderFrameHost already exists we try as much as possible to reuse it and
    // its associated WebUI.

    // Check for cases that a speculative RenderFrameHost cannot be used and
    // create a new one if needed.
    if (!speculative_render_frame_host_ ||
        speculative_render_frame_host_->GetSiteInstance() !=
            dest_site_instance.get()) {
      AppendReason(reason, "GetFrameHostForNavigation / new-speculative-rfh");

      // Determine if the old speculative RFH and new speculative RFH will use
      // the same process.  If so, add a reference to that process so that
      // it won't get cleaned up when the old speculative RFH is discarded and
      // then immediately recreated for the new speculative RFH.
      bool should_keep_target_process_alive =
          speculative_render_frame_host_ && dest_site_instance->HasProcess() &&
          speculative_render_frame_host_->GetProcess() ==
              dest_site_instance->GetProcess();
      if (should_keep_target_process_alive) {
        dest_site_instance->GetProcess()->IncrementPendingReuseRefCount();
      }

      if (request->IsInPrerenderedMainFrame() &&
          speculative_render_frame_host_) {
        // For prerendered pages, the main frame is never in the provisional
        // state in the renderer. Discarding it now is going to lead to crashes
        // in the renderer, e.g. https://crbug.com/40063628 and
        // https://crbug.com/40076091. Try to gather some info now to help
        // diagnose the renderer-side crashes.
        SCOPED_CRASH_KEY_STRING256("Bug40076091", "reason",
                                   reason ? *reason : "n/a");
        SCOPED_CRASH_KEY_NUMBER(
            "Bug40076091", "bcg_swap_type",
            base::to_underlying(browsing_context_group_swap->type()));
        SCOPED_CRASH_KEY_NUMBER(
            "Bug40076091", "bi_swap_result",
            base::to_underlying(
                request->coop_status().browsing_instance_swap_result()));
        SCOPED_CRASH_KEY_STRING64(
            "Bug40076091", "currrent_si",
            current_site_instance->GetSiteInfo().GetDebugString());
        SCOPED_CRASH_KEY_STRING64(
            "Bug40076091", "dest_si",
            dest_site_instance->GetSiteInfo().GetDebugString());
        // When this happens, this is a bug. In tests, crash so the failure
        // isn't silent. But in the wild, crashing the user's browser from a
        // recoverable error is not useful, so don't crash there :)
#if DCHECK_IS_ON()
        DCHECK(false);
#else
        base::debug::DumpWithoutCrashing();
#endif
      }
      DiscardSpeculativeRFH(request->GetTypeForNavigationDiscardReason());
      bool success = CreateSpeculativeRenderFrameHost(
          current_site_instance, dest_site_instance.get(),
          recovering_without_early_commit);
      DCHECK(success);

      if (should_keep_target_process_alive) {
        dest_site_instance->GetProcess()->DecrementPendingReuseRefCount();
      }
    } else {
      AppendReason(reason,
                   "GetFrameHostForNavigation / existing-speculative-rfh");
    }
    DCHECK(speculative_render_frame_host_);

    navigation_rfh = speculative_render_frame_host_.get();
    request->SetAssociatedRFHType(
        NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE);

    // TODO(crbug.com/40276607): Move this early swap to happen after
    // DidStartNavigation, together with the back/forward early swap.
    PerformEarlyRenderFrameHostSwapIfNeeded(
        request, /*is_called_after_did_start_navigation=*/false);
  }

  DCHECK(navigation_rfh &&
         (navigation_rfh == render_frame_host_.get() ||
          navigation_rfh == speculative_render_frame_host_.get()));
  DCHECK(!navigation_rfh->must_be_replaced());

  // If the RenderFrame that needs to navigate is not live (its process was just
  // created), initialize it. This can only happen for the initial main frame of
  // a WebContents which starts non-live but non-crashed.
  //
  // A speculative RenderFrameHost is created in the live state. A crashed
  // RenderFrameHost is replaced by a new speculative RenderFrameHost. A
  // non-speculative RenderFrameHost that is being reused is already live. This
  // leaves only a non-speculative RenderFrameHost that has never been used
  // before.
  if (!navigation_rfh->IsRenderFrameLive()) {
    DCHECK(!frame_tree_node_->parent());
    SCOPED_CRASH_KEY_BOOL("Bug1404162", "is_main_frame",
                          frame_tree_node_->IsMainFrame());
    SCOPED_CRASH_KEY_BOOL("Bug1404162", "use_current_rfh", use_current_rfh);
    SCOPED_CRASH_KEY_BOOL("Bug1404162", "nav_rfh_is_current_rfh",
                          navigation_rfh == render_frame_host_.get());
    SCOPED_CRASH_KEY_BOOL("Bug1404162", "must_be_replaced",
                          navigation_rfh->must_be_replaced());
    SCOPED_CRASH_KEY_BOOL("Bug1404162", "rf_created",
                          navigation_rfh->is_render_frame_created());
    SCOPED_CRASH_KEY_BOOL(
        "Bug1404162", "process_live",
        navigation_rfh->GetProcess()->IsInitializedAndNotDead());
    SCOPED_CRASH_KEY_BOOL("Bug1404162", "without_early_commit",
                          recovering_without_early_commit);
    SCOPED_CRASH_KEY_STRING64("Bug1404162", "nav_rfh_lifecycle",
                              RenderFrameHostImpl::LifecycleStateImplToString(
                                  navigation_rfh->lifecycle_state()));

    if (!ReinitializeMainRenderFrame(navigation_rfh)) {
      return base::unexpected(
          GetFrameHostForNavigationFailed::kCouldNotReinitializeMainFrame);
    }

    notify_webui_of_rf_creation = true;

    if (navigation_rfh == render_frame_host_.get()) {
      EnsureRenderFrameHostPageFocusConsistent();
      // TODO(nasko): This is a very ugly hack. The Chrome extensions process
      // manager still uses NotificationService and expects to see a
      // RenderViewHost changed notification after WebContents and
      // RenderFrameHostManager are completely initialized. This should be
      // removed once the process manager moves away from NotificationService.
      // See https://crbug.com/462682.
      //
      // TODO(https://crbug.com/338233133): The extensions process manager does
      // not use NotificationService; clean this up.
      if (frame_tree_node_->IsMainFrame()) {
        delegate_->NotifyMainFrameSwappedFromRenderManager(
            nullptr, render_frame_host_.get());
      }
    }
  }

  if (request->HasWebUI()) {
    // If a WebUI has been created for the NavigationRequest, set it on the
    // RenderFrameHost picked for the navigation.
    navigation_rfh->SetWebUI(*request);
    CHECK(navigation_rfh->web_ui());
  }
  if (notify_webui_of_rf_creation && navigation_rfh->web_ui()) {
    CHECK(navigation_rfh->IsRenderFrameLive());
    // If a WebUI was created in a speculative RenderFrameHost, or a new
    // RenderFrame was created for an existing WebUI, then the WebUI never
    // interacted with the RenderFrame. Notify using WebUIRenderFrameCreated.
    navigation_rfh->web_ui()->WebUIRenderFrameCreated(navigation_rfh);
  }

  // The following call is here to make sure that explicit opt-out requests,
  // made while kOriginKeyedProcessByDefault is enabled, record the opt-out
  // status before CanAccessDataForOrigin is called below. It allows
  // CanAccessDataForOrigin to start by assuming default isolation (as stored in
  // the associated IsolationContext), knowing that it will be changed (during
  // the construction of the expected ProcessLock) to being explicit opt-out due
  // to the origin being tracked. The change occurs when we create a SiteInfo
  // for the ProcessLock and DetermineOriginAgentClusterIsolation is called.
  //
  // A similar call to the one below is made in
  // NavigationRequest::SelectFrameHostForOnResponseStarted() to handle
  // recording opt-outs when kOriginAgentClusterDefault is enabled, although in
  // that case process isolation isn't involved, and so the following call to
  // CanAccessDataForOrigin isn't a problem.
  // TODO(crbug.com/40613869): Remove the following block (and the
  // comments above) when the ProcessLock check below is removed.
  const IsolationContext& isolation_context =
      navigation_rfh->GetSiteInstance()->GetIsolationContext();
  request->AddOriginAgentClusterStateIfNecessary(isolation_context);

  // If this function picked an incompatible process for the origin that's about
  // to commit, except for allowed cases such as navigating to an error page
  // reusing the current process, capture a crash dump to diagnose why it is
  // occurring.
  // TODO(creis): Remove this check after we've gathered enough information to
  // debug issues with browser-side security checks. https://crbug.com/931895.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const auto process_lock = navigation_rfh->GetProcess()->GetProcessLock();
  if (!process_lock.is_error_page() &&
      request->common_params().url.IsStandard() &&
      !request->IsForMhtmlSubframe() &&
      request->ComputeErrorPageProcess() !=
          NavigationRequest::ErrorPageProcess::kCurrentProcess) {
    // Note that GetOriginToCommit() could return nullopt if the response is
    // received but does not need to be rendered, for example for a download.
    // However, that case should never need to pick a RenderFrameHost via
    // GetFrameHostForNavigation(), so getting here should imply that
    // GetOriginToCommit() always has a value.
    const url::Origin origin_to_commit =
        request->state() >= NavigationRequest::WILL_PROCESS_RESPONSE
            ? request->GetOriginToCommit().value()
            : request->GetTentativeOriginAtRequestTime();
    if (!policy->CanAccessOrigin(
            navigation_rfh->GetProcess()->GetID(), origin_to_commit,
            ChildProcessSecurityPolicyImpl::AccessType::kCanCommitNewOrigin)) {
      SCOPED_CRASH_KEY_STRING256("GetFrameHostForNav", "lock_url",
                                 process_lock.ToString());
      SCOPED_CRASH_KEY_STRING64(
          "GetFrameHostForNav", "commit_url_origin",
          request->common_params().url.DeprecatedGetOriginAsURL().spec());
      SCOPED_CRASH_KEY_STRING64("GetFrameHostForNav", "commit_origin",
                                origin_to_commit.GetDebugString());
      SCOPED_CRASH_KEY_BOOL("GetFrameHostForNav", "is_main_frame",
                            frame_tree_node_->IsMainFrame());
      SCOPED_CRASH_KEY_BOOL("GetFrameHostForNav", "use_current_rfh",
                            use_current_rfh);
      NOTREACHED_IN_MIGRATION()
          << "Picked an incompatible process for origin: "
          << process_lock.ToString() << " lock vs "
          << origin_to_commit.GetDebugString()
          << ", request_is_sandboxed = " << request->GetUrlInfo().is_sandboxed;
      base::debug::DumpWithoutCrashing();
    }
  }

  return navigation_rfh;
}

void RenderFrameHostManager::CreateWebUIForNavigationIfNeeded(
    NavigationRequest* request,
    SiteInstanceImpl* dest_site_instance,
    bool use_current_rfh) {
  if (request->HasWebUI()) {
    // It's possible for the navigation to already have a WebUI
    // associated with when it is called for the second time for the request,
    // e.g. from OnResponseStarted or OnStartChecksComplete.
    CHECK_GE(request->state(), NavigationRequest::WILL_START_REQUEST);
    CHECK(!request->web_ui()->HasRenderFrameHost());
    return;
  }

  BrowserContext* browser_context =
      render_frame_host_->GetSiteInstance()->GetBrowserContext();
  if (!NavigationRequestUsesWebUI(request, browser_context)) {
    return;
  }

  // If the navigation is to a WebUI URL, the WebUI needs to be created to
  // allow the navigation to be served correctly.
  if (use_current_rfh) {
    // If the navigation is to a WebUI and the current RenderFrameHost is
    // going to be used, there are only two possible ways to get here:
    // * The navigation is between two different documents belonging to the
    //   same WebUI or reloading the same document.
    // * Newly created window with a RenderFrameHost which hasn't committed a
    //   navigation yet.
    if (render_frame_host_->has_committed_any_navigation()) {
      // If |render_frame_host_| has committed at least one navigation and it
      // is in a WebUI SiteInstance, then it must have the exact same WebUI
      // type if it will be reused.
      CHECK_EQ(render_frame_host_->web_ui_type(),
               WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
                   browser_context, request->common_params().url))
          << "WebUI type mismatch for " << request->common_params().url;
    } else if (!render_frame_host_->web_ui()) {
      // It is possible to reuse a RenderFrameHost when going to a WebUI URL
      // and not have created a WebUI instance. An example is a WebUI main
      // frame that includes an iframe to URL that doesn't require WebUI but
      // stays in the parent frame SiteInstance (e.g. about:blank).  If that
      // frame is subsequently navigated to a URL in the same WebUI as the
      // parent frame, the RenderFrameHost will be reused and WebUI instance
      // for the child frame needs to be created.
      // During navigation, this method is called twice - at the beginning
      // and at ReadyToCommit time. The first call would have created the
      // WebUI instance and since the initial about:blank has not committed
      // a navigation, the else branch would be taken. Explicit check for
      // `web_ui()` is required, otherwise we will allocate a new instance
      // unnecessarily here.
      request->CreateWebUIIfNeeded(render_frame_host_.get());
    }
  } else if (speculative_render_frame_host_ &&
             speculative_render_frame_host_->GetSiteInstance() ==
                 dest_site_instance) {
    // The navigation will reuse the speculative RenderFrameHost. In this case,
    // a WebUI might have already been created in the speculative RFH, but it's
    // OK because `CreateWebUIIfNeeded()` won't create a new WebUI in that case
    // and this function will return false.
    request->CreateWebUIIfNeeded(speculative_render_frame_host_.get());
  } else {
    // The navigation will create a new speculative RenderFrameHost, so pass in
    // nullptr to `CreateWebUIIfNeeded()` as the RenderFrameHost is yet to be
    // created.
    request->CreateWebUIIfNeeded(nullptr);
  }
}

void RenderFrameHostManager::DiscardSpeculativeRFHIfUnused(
    NavigationDiscardReason reason) {
  // This is called when a renderer aborts a NavigationRequest
  // that was in the READY_TO_COMMIT state. The caller has already
  // disassociated the NavigationRequest from the RenderFrameHost,
  // which may or may not have been the speculative one. Either way,
  // if there are no remaining NavigationRequests associated with
  // |speculative_render_frame_host_|, then it is safe to call
  // DiscardSpeculativeRFH() to discard |speculative_render_frame_host_|.
  if (!speculative_render_frame_host_ ||
      speculative_render_frame_host_->HasPendingCommitNavigation()) {
    return;
  }
  NavigationRequest* navigation_request =
      frame_tree_node_->navigation_request();
  if (navigation_request &&
      navigation_request->GetAssociatedRFHType() ==
          NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE) {
    return;
  }
  DiscardSpeculativeRFH(reason);
}

void RenderFrameHostManager::DiscardSpeculativeRFH(
    NavigationDiscardReason reason) {
  TRACE_EVENT("navigation", "RenderFrameHostManager::DiscardSpeculativeRFH",
              ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);
  if (speculative_render_frame_host_) {
    bool was_loading = speculative_render_frame_host_->is_loading();
    SCOPED_CRASH_KEY_BOOL("Bug1450023", "is_main_frame",
                          frame_tree_node_->IsMainFrame());
    SCOPED_CRASH_KEY_NUMBER(
        "Bug1450023", "queueing_level",
        static_cast<int>(GetNavigationQueueingFeatureLevel()));
    SCOPED_CRASH_KEY_NUMBER(
        "Bug1450023", "current_rfh_si",
        static_cast<int>(current_frame_host()->GetSiteInstance()->GetId()));
    SCOPED_CRASH_KEY_NUMBER(
        "Bug1450023", "spec_rfh_si",
        static_cast<int>(
            speculative_render_frame_host_->GetSiteInstance()->GetId()));
    SCOPED_CRASH_KEY_STRING64(
        "Bug1450023", "spec_rfh_lifecycle",
        RenderFrameHostImpl::LifecycleStateImplToString(
            speculative_render_frame_host_->lifecycle_state()));

    if (NavigationRequest* navigation_request =
            frame_tree_node_->navigation_request()) {
      if (navigation_request->HasRenderFrameHost() &&
          navigation_request->GetRenderFrameHost() ==
              speculative_render_frame_host_.get()) {
        // Ensure that there are no ongoing NavigationRequest pointing to the
        // about-to-be-deleted speculative RFH. Note that NavigationRequests
        // that are associated with a non-speculative RFH and pending-commit
        // NavigationRequests that are already owned by a pending-commit RFH
        // will be deleted separately in the RenderFrameHost destructor.
        frame_tree_node_->ResetNavigationRequestButKeepState(reason);
      }
    }
    DiscardUnusedFrame(UnsetSpeculativeRenderFrameHost(reason));
    // If we were navigating away from a crashed main frame then we will have
    // set the RVH's main frame routing ID to MSG_ROUTING_NONE. We need to set
    // it back to the crashed frame to avoid having a situation where it's
    // pointing to nothing even though there is no pending commit.
    if (ShouldSkipEarlyCommitPendingForCrashedFrame() &&
        frame_tree_node_->IsMainFrame() &&
        !render_frame_host_->IsRenderFrameLive()) {
      render_frame_host_->render_view_host()->SetMainFrameRoutingId(
          render_frame_host_->GetRoutingID());
    }
    if (was_loading)
      frame_tree_node_->DidStopLoading();
  }
}

std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::UnsetSpeculativeRenderFrameHost(
    NavigationDiscardReason reason) {
  TRACE_EVENT("navigation",
              "RenderFrameHostManager::UnsetSpeculativeRenderFrameHost",
              ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);

  speculative_render_frame_host_->GetProcess()->RemovePendingView();
  if (speculative_render_frame_host_->lifecycle_state() ==
      LifecycleStateImpl::kSpeculative) {
    speculative_render_frame_host_->DeleteRenderFrame(
        frame_tree_node_->parent()
            ? mojom::FrameDeleteIntention::kNotMainFrame
            : mojom::FrameDeleteIntention::
                  kSpeculativeMainFrameForNavigationCancelled);
  } else {
    // TODO(dcheng): Upgrade this to a CHECK()?
    DCHECK_EQ(speculative_render_frame_host_->lifecycle_state(),
              LifecycleStateImpl::kPendingCommit);

    if (!ShouldQueueNavigationsWhenPendingCommitRFHExists()) {
      // The browser process already asked the renderer to commit the
      // navigation. The renderer is guaranteed to commit the navigation and
      // swap in the provisional `RenderFrame` to replace the current
      // `blink::RemoteFrame` unless the frame is detached: see
      // `AssertNavigationCommits` in `RenderFrameImpl` for more details about
      // this enforcement.
      //
      // Instead of simply deleting the `RenderFrame`, the browser process must
      // unwind the renderer's state by sending it another IPC to "undo" the
      // commit by immediately swapping it out for a proxy again.

      // The renderer hasn't acknowledged the `CommitNavigation()` yet so the
      // `RenderFrameProxyHost` should still be alive. Reuse it.
      RenderFrameProxyHost* proxy =
          speculative_render_frame_host_->browsing_context_state()
              ->GetRenderFrameProxyHost(
                  speculative_render_frame_host_->GetSiteInstance()->group());

      SCOPED_CRASH_KEY_BOOL("Bug1450023", "proxy_exists", !!proxy);
      DCHECK(proxy);
      // Note: this advances the RenderFrameHost's lifecycle state to
      // kReadyToBeDeleted.
      speculative_render_frame_host_->UndoCommitNavigation(
          *proxy, frame_tree_node_->IsLoading());
    } else {
      // A reasonable person might wonder: shouldn't a RenderFrameHostImpl in
      // kPendingCommit always have a... pending commit?
      //
      // The surprising answer is no! When the browser process handles the
      // renderer's commit navigation ack:
      // - the NavigationRequest is unconditionally removed from
      //   `RenderFrameHostImpl::navigation_requests_`.
      // - but if the IPC fails validation, the browser process reports a bad
      //   message (which kills the renderer process) and returns immediately.
      //
      // However, the kill is async and observing process termination (which is
      // what cleans up the speculative RenderFrameHostImpl) is also async.
      // Between reporting the bad message and the actual cleanup, the user can
      // begin a new navigation, which will discard any speculative RFHs rather
      // than blocking (since `HasPendingCommitForCrossDocumentNavigation()` now
      // returns `false`!) for a reason other than `kRenderProcessGone` or
      // `kWillRemoveFrame`.
      //
      // TODO(crbug.com/335790757): it might help make state easier to reason
      // about if the speculative RFH is proactively discarded rather than just
      // leaving it around to be asynchronously cleaned up.
      if (speculative_render_frame_host_
              ->HasPendingCommitForCrossDocumentNavigation()) {
        // With navigation queueing, pending commit navigations in speculative
        // RenderFrameHosts shouldn't get deleted, unless the FrameTreeNode or
        // renderer process is gone/will be gone soon.
        CHECK(reason == NavigationDiscardReason::kRenderProcessGone ||
              reason == NavigationDiscardReason::kWillRemoveFrame);
      }

      // TODO(dcheng): `CHECK(render_frame_host_->IsPendingDeletion())` would be
      // a nice precondition to enforce here. However, this turns out to be
      // Hard: `StartPendingDeletionOnSubtree()` performs its work in two
      // phases: it resets all navigation requests first (which might delete
      // speculative RFHs—even ones in pending commit), before doing a complex
      // dance to invoke `DeleteRenderFrame()` a minimal number of times. In the
      // future, it would be nice to refactor the code so this precondition can
      // be enforced.

      // A pending commit RFH is assumed/expected to have committed already in
      // the renderer process. If the FrameTreeNode is going away, explicitly
      // tear down the RenderFrame in the renderer process to keep the frame
      // tree in sync.
      if (frame_tree_node_->parent()) {
        speculative_render_frame_host_->DeleteRenderFrame(
            mojom::FrameDeleteIntention::kNotMainFrame);
      } else {
        // But for main frames, just advance the lifecycle state instead. In
        // Blink, a live WebView must always have a live main frame; violating
        // this invariant by destroying the already-committed (from the
        // perspective of the renderer process) frame with `DeleteRenderFrame()`
        // results in bugs like crbug.com/40091257.
        //
        // The main RenderFrame will be implicitly torn down later when the
        // corresponding RenderViewHost/WebView are torn down.
        speculative_render_frame_host_->SetLifecycleState(
            LifecycleStateImpl::kReadyToBeDeleted);
      }
    }
  }

  return std::move(speculative_render_frame_host_);
}

void RenderFrameHostManager::DiscardSpeculativeRenderFrameHostForShutdown() {
  TRACE_EVENT(
      "navigation",
      "RenderFrameHostManager::DiscardSpeculativeRenderFrameHostForShutdown",
      ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);

  DCHECK(speculative_render_frame_host_);

  speculative_render_frame_host_->GetProcess()->RemovePendingView();
  // No need to call `DeleteRenderFrame()`. When a RenderFrame or
  // `blink::RemoteFrame` is detached, it also detaches any associated
  // provisional RenderFrame, whether this due to a child frame being removed
  // from the frame tree or the entire `blink::WebView` being torn down.
  //
  // When the LifecycleStateImpl is kSpeculative, there is no need to transition
  // to kReadyToBeDeleted as speculative RenderFrameHosts don't run any unload
  // handlers but gets deleted by reset directly in kSpeculative state.
  if (speculative_render_frame_host_->lifecycle_state() ==
      LifecycleStateImpl::kPendingCommit) {
    speculative_render_frame_host_->SetLifecycleState(
        LifecycleStateImpl::kReadyToBeDeleted);
  }
  // TODO(dcheng): Figure out why `RenderFrameDeleted()` doesn't seem to be
  // called on child `RenderFrameHost`s at shutdown. This is currently limited
  // to main frame-only because that is how it has worked for some time:
  // `~WebContentsImpl()` calls `FrameTree::Shutdown()` which calls
  // `RenderFrameDeleted()` for main frame RenderFrameHosts only... Since
  // `FrameTree::Shutdown()` now delegates to this method to shutdown the
  // speculative RenderFrameHost, match the previous behavior.
  if (frame_tree_node_->IsMainFrame()) {
    speculative_render_frame_host_->RenderFrameDeleted();
  }
  speculative_render_frame_host_.reset();
}

void RenderFrameHostManager::OnDidChangeCollapsedState(bool collapsed) {
  // If we are a MPArch fenced frame root then ask the outer delegate node
  // to collapse the frame. Note `IsFencedFrameRoot` returns true for
  // ShadowDOM as well so we need to check the `FrameTree::Type` as well.
  if (frame_tree_node_->IsFencedFrameRoot() &&
      frame_tree_node_->IsInFencedFrameTree()) {
    if (GetProxyToOuterDelegate()->is_render_frame_proxy_live()) {
      GetProxyToOuterDelegate()->GetAssociatedRemoteFrame()->Collapse(
          collapsed);
    }
    return;
  }

  DCHECK(frame_tree_node_->parent());
  SiteInstanceGroup* parent_group =
      frame_tree_node_->parent()->GetSiteInstance()->group();

  // There will be no proxy to represent the pending or speculative RFHs in the
  // parent's SiteInstanceGroup until the navigation is committed, but the old
  // RFH is not unloaded before that happens either, so we can talk to the
  // FrameOwner in the parent via the child's current RenderFrame at any time.
  DCHECK(current_frame_host());
  if (current_frame_host()->GetSiteInstance()->group() == parent_group) {
    current_frame_host()->GetAssociatedLocalFrame()->Collapse(collapsed);
  } else {
    RenderFrameProxyHost* proxy_to_parent =
        frame_tree_node_->GetBrowsingContextStateForSubframe()
            ->GetRenderFrameProxyHost(parent_group);
    if (proxy_to_parent->is_render_frame_proxy_live())
      proxy_to_parent->GetAssociatedRemoteFrame()->Collapse(collapsed);
  }
}

void RenderFrameHostManager::OnDidUpdateFrameOwnerProperties(
    const blink::mojom::FrameOwnerProperties& properties) {
  // FrameOwnerProperties exist only for frames that have a parent.
  CHECK(frame_tree_node_->parent());
  SiteInstanceGroup* parent_group =
      frame_tree_node_->parent()->GetSiteInstance()->group();

  auto properties_for_local_frame = properties.Clone();

  // Notify the RenderFrame if it lives in a different process from its parent.
  if (render_frame_host_->GetSiteInstance()->group() != parent_group) {
    render_frame_host_->GetAssociatedLocalFrame()->SetFrameOwnerProperties(
        std::move(properties_for_local_frame));
  }

  render_frame_host_->browsing_context_state()->OnDidUpdateFrameOwnerProperties(
      properties);
}

RenderFrameHostManager::SiteInstanceDescriptor::SiteInstanceDescriptor(
    SiteInstanceImpl* site_instance)
    : existing_site_instance(site_instance),
      relation(SiteInstanceRelation::PREEXISTING) {}

RenderFrameHostManager::SiteInstanceDescriptor::SiteInstanceDescriptor(
    UrlInfo dest_url_info,
    SiteInstanceRelation relation_to_current)
    : existing_site_instance(nullptr),
      dest_url_info(dest_url_info),
      relation(relation_to_current) {
  DCHECK((relation_to_current == SiteInstanceRelation::RELATED) ||
         (relation_to_current == SiteInstanceRelation::RELATED_IN_COOP_GROUP) ||
         (relation_to_current == SiteInstanceRelation::UNRELATED));
}

void RenderFrameHostManager::CleanupSpeculativeRfhForRenderProcessGone() {
  CHECK(speculative_render_frame_host_);
  // TODO(crbug.com/41268960): This should just clean up the speculative
  // RFH without canceling the request.
  if (frame_tree_node_->navigation_request()) {
    // TODO(crbug.com/41268960): This might cancel an unrelated
    // NavigationRequest. Maybe check if the navigation request uses the
    // speculative RFH first?
    frame_tree_node_->navigation_request()->set_net_error(net::ERR_ABORTED);
    frame_tree_node_->ResetNavigationRequest(
        NavigationDiscardReason::kRenderProcessGone);
  }
  // It's possible that we are far enough into the navigation that
  // TransferNavigationRequestOwnership has already been called then the
  // FrameTreeNode no longer owns the NavigationRequest and we need to clean up.
  DiscardSpeculativeRFH(NavigationDiscardReason::kRenderProcessGone);
}

void RenderFrameHostManager::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {
  // Don't propagate user activations out of fenced frame trees.
  FrameTreeNode* root = frame_tree_node_->frame_tree().root();
  if (root->IsFencedFrameRoot()) {
    return;
  }

  for (const auto& pair :
       render_frame_host_->browsing_context_state()->proxy_hosts()) {
    RenderFrameProxyHost* proxy = pair.second.get();
    if (proxy->is_render_frame_proxy_live()) {
      proxy->GetAssociatedRemoteFrame()->UpdateUserActivationState(
          update_type, notification_type);
    }
  }

  // If any frame in an inner delegate is activated, then the FrameTreeNode that
  // embeds the inner delegate in the outer delegate should be activated as well
  // (crbug.com/1013447).
  //
  // TODO(mustaq): We should add activation consumption propagation from inner
  // to outer delegates, and also all state propagation from outer to inner
  // delegates. crbug.com/1026617.
  RenderFrameProxyHost* outer_delegate_proxy =
      root->render_manager()->GetProxyToOuterDelegate();
  if (outer_delegate_proxy &&
      outer_delegate_proxy->is_render_frame_proxy_live() &&
      update_type ==
          blink::mojom::UserActivationUpdateType::kNotifyActivation) {
    outer_delegate_proxy->GetAssociatedRemoteFrame()->UpdateUserActivationState(
        update_type, notification_type);
    GetOuterDelegateNode()->UpdateUserActivationState(update_type,
                                                      notification_type);
  }
}

BrowsingContextGroupSwap
RenderFrameHostManager::ShouldSwapBrowsingInstancesForNavigation(
    const GURL& current_effective_url,
    bool current_is_view_source_mode,
    SiteInstanceImpl* source_instance,
    SiteInstanceImpl* current_instance,
    SiteInstanceImpl* destination_instance,
    const UrlInfo& destination_url_info,
    bool destination_is_view_source_mode,
    ui::PageTransition transition,
    NavigationRequest::ErrorPageProcess error_page_process,
    bool is_reload,
    bool is_same_document,
    IsSameSiteGetter& is_same_site,
    CoopSwapResult coop_swap_result,
    bool was_server_redirect,
    bool should_replace_current_entry,
    bool has_rel_opener) {
  const GURL& destination_url = destination_url_info.url;
  // A subframe must stay in the same BrowsingInstance as its parent.
  bool is_main_frame = frame_tree_node_->IsMainFrame();
  if (!is_main_frame) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_NotMainFrame);
  }

  if (is_same_document) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_SameDocumentNavigation);
  }

  // Check for reasons to swap processes even if we are in a process model that
  // doesn't usually swap (e.g., process-per-tab).  Any time we return true,
  // the new URL will be rendered in a new SiteInstance AND BrowsingInstance.
  BrowserContext* browser_context =
      GetNavigationController().GetBrowserContext();
  const GURL& destination_effective_url =
      SiteInstanceImpl::GetEffectiveURL(browser_context, destination_url);
  // Don't force a new BrowsingInstance for URLs that are handled in the
  // renderer process, like javascript: or debug URLs like chrome://crash.
  if (blink::IsRendererDebugURL(destination_effective_url)) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_RendererDebugURL);
  }

  if (coop_swap_result == CoopSwapResult::kSwap) {
    return BrowsingContextGroupSwap::CreateCoopSwap();
  }

  // Transitions across BrowserContexts should always require a
  // BrowsingInstance swap. For example, this can happen if an extension in a
  // normal profile opens an incognito window with a web URL using
  // chrome.windows.create().
  //
  // TODO(alexmos): This check should've been enforced earlier in the
  // navigation, in chrome::Navigate().  Verify this, and then convert this to
  // a CHECK and remove the fallback.
  DCHECK_EQ(browser_context,
            render_frame_host_->GetSiteInstance()->GetBrowserContext());
  if (browser_context !=
      render_frame_host_->GetSiteInstance()->GetBrowserContext()) {
    return BrowsingContextGroupSwap::CreateSecuritySwap();
  }

  // For security, we should transition between processes when one is a Web UI
  // page and one isn't, or if the WebUI types differ.
  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          render_frame_host_->GetProcess()->GetID()) ||
      WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
          browser_context, current_effective_url)) {
    // If so, force a swap if destination is not an acceptable URL for Web UI.
    // Here, data URLs are never allowed.
    if (!WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            browser_context, destination_effective_url)) {
      return BrowsingContextGroupSwap::CreateSecuritySwap();
    }

    // Force swap if the current WebUI type differs from the one for the
    // destination.
    if (WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
            browser_context, current_effective_url) !=
        WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
            browser_context, destination_effective_url)) {
      return BrowsingContextGroupSwap::CreateSecuritySwap();
    }
  } else {
    // Force a swap if the current frame is not WebUI but the navigation is to a
    // Web UI URL. Exclude the case where the navigation starts from an initial
    // RenderFrameHost in an unassigned SiteInstance and unused process, since
    // in that case the WebUI navigation can safely reuse them.
    //
    // Subtle: using both !has_committed_any_navigation() and
    // is_initial_empty_document() to check for an initial RFH is intentional.
    // has_committed_any_navigation() becomes true when the first navigation
    // sends a CommitNavigation IPC, which avoids races where a WebUI navigation
    // incorrectly tries to reuse an initial RFH while another navigation in it
    // is pending commit. is_initial_empty_document() is additionally used to
    // avoid reusing an initial RFH after crashes and after document.open().
    // See https://crbug.com/1492076 and https://crbug.com/1485586.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, destination_effective_url)) {
      bool starts_from_initial_rfh =
          render_frame_host_->GetProcess()->IsUnused() &&
          !current_instance->HasSite() &&
          !render_frame_host_->has_committed_any_navigation() &&
          render_frame_host_->is_initial_empty_document();
      if (!starts_from_initial_rfh ||
          !base::FeatureList::IsEnabled(
              features::kReuseInitialRenderFrameHostForWebUI)) {
        return BrowsingContextGroupSwap::CreateSecuritySwap();
      }
    }
  }

  // Check with the content client as well.  Important to pass
  // current_effective_url here, which uses the SiteInstance's site if there is
  // no current_entry.
  if (GetContentClient()->browser()->ShouldSwapBrowsingInstancesForNavigation(
          render_frame_host_->GetSiteInstance(), current_effective_url,
          destination_effective_url)) {
    return BrowsingContextGroupSwap::CreateSecuritySwap();
  }

  // We can't switch a `blink::WebView` between view source and non-view source
  // mode without screwing up the session history sometimes (when navigating
  // between "view-source:http://foo.com/" and "http://foo.com/", Blink doesn't
  // treat it as a new navigation). So require a BrowsingInstance switch.
  if (current_is_view_source_mode != destination_is_view_source_mode)
    return BrowsingContextGroupSwap::CreateSecuritySwap();

  // If we haven't used the current SiteInstance but the destination is a
  // view-source URL, we should force a BrowsingInstance swap so that we won't
  // reuse the current SiteInstance.
  if (!current_instance->HasSite() && destination_is_view_source_mode)
    return BrowsingContextGroupSwap::CreateSecuritySwap();

  // If the target URL's origin was dynamically isolated, and the isolation
  // wouldn't apply in the current BrowsingInstance, see if this navigation can
  // safely swap to a new BrowsingInstance where this isolation would take
  // effect.  This helps protect sites that have just opted into process
  // isolation, ensuring that the next navigation (e.g., a form submission
  // after user has typed in a password) can utilize a dedicated process when
  // possible (e.g., when there are no existing script references).
  UrlInfo url_info_to_test = destination_url_info;
  url_info_to_test.url = destination_effective_url;
  if (ShouldSwapBrowsingInstancesForDynamicIsolation(render_frame_host_.get(),
                                                     url_info_to_test)) {
    return BrowsingContextGroupSwap::CreateSecuritySwap();
  }

  // If the navigation should end up in a different StoragePartition, create a
  // new BrowsingInstance, as we can only have one StoragePartition per
  // BrowsingInstance.
  if (DoesNavigationChangeStoragePartition(current_instance,
                                           destination_url_info)) {
    return BrowsingContextGroupSwap::CreateSecuritySwap();
  }

  // If the destination might have been a prefetch based on cross-site state, we
  // want to swap to make it more difficult to observe that the navigation
  // completes faster than normal.
  // https://crbug.com/1439246
  if (destination_url_info.is_prefetch_with_cross_site_contamination) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Preloading.PrefetchBCGSwap.RelatedActiveContents",
        base::saturated_cast<base::HistogramBase::Sample>(
            current_instance->GetRelatedActiveContentsCount()),
        51);
    if (base::FeatureList::IsEnabled(
            features::kPrefetchStateContaminationMitigation) &&
        features::kPrefetchStateContaminationSwapsBrowsingContextGroup.Get()) {
      return BrowsingContextGroupSwap::CreateSecuritySwap();
    }
  }

  // We've checked that we didn't need to do a hard BrowsingInstance swap. If
  // COOP: restrict-properties asks for it, do a BrowsingInstance swap that
  // preserves a reference to the previous BrowsingInstance. Such
  // BrowsingInstances are said to be "related".
  if (coop_swap_result == CoopSwapResult::kSwapRelated) {
    return BrowsingContextGroupSwap::CreateRelatedCoopSwap();
  }

  // When doing a history navigation, we cannot assume that the page will behave
  // in the same way as it did previously. It could change headers, lead to an
  // error page, etc. We only check the destination_instance once we're done
  // verifying that up-to-date security reasons do not require a
  // BrowsingInstance swap. On the other hand we should use the
  // destination_instance if suitable instead of swapping to a new
  // BrowsingInstance. This is why this block is after security checks, but
  // before proactive BrowsingInstance swap.
  if (destination_instance) {
    if (!destination_instance->IsCoopRelatedSiteInstance(current_instance)) {
      return BrowsingContextGroupSwap::CreateSecuritySwap();
    }
    if (!destination_instance->IsRelatedSiteInstance(current_instance)) {
      return BrowsingContextGroupSwap::CreateRelatedCoopSwap();
    }
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_AlreadyHasMatchingBrowsingInstance);
  }

  // If this is a cross-site navigation, we may be able to force a
  // BrowsingInstance swap to avoid unneeded process sharing. This is done for
  // certain main frame browser-initiated navigations where we can't use
  // |source_instance| and we don't need to preserve scripting
  // relationship for it (for isolated error pages).
  // See https://crbug.com/803367.
  // TODO(crbug.com/40239885): This should probably be considered a
  // a speculative BrowsingInstance swap. It is not required for security and
  // needs to be treated after the history navigation block
  bool is_for_isolated_error_page =
      (error_page_process ==
       NavigationRequest::ErrorPageProcess::kIsolatedProcess);
  if (current_instance->HasSite() &&
      !is_same_site.Get(*render_frame_host_, destination_url_info) &&
      !CanUseSourceSiteInstance(destination_url_info, source_instance,
                                was_server_redirect, error_page_process) &&
      !is_for_isolated_error_page &&
      IsBrowsingInstanceSwapAllowedForPageTransition(transition,
                                                     destination_url) &&
      render_frame_host_->has_committed_any_navigation()) {
    return BrowsingContextGroupSwap::CreateSecuritySwap();
  }

  // Experimental mode to swap BrowsingInstances on most navigations when there
  // are no other windows in the BrowsingInstance.
  return ShouldProactivelySwapBrowsingInstance(
      destination_url_info, is_reload, is_same_site,
      should_replace_current_entry, has_rel_opener);
}

BrowsingContextGroupSwap
RenderFrameHostManager::ShouldProactivelySwapBrowsingInstance(
    const UrlInfo& destination_url_info,
    bool is_reload,
    IsSameSiteGetter& is_same_site,
    bool should_replace_current_entry,
    bool has_rel_opener) {
  // If we've disabled proactive BrowsingInstance swap for this RenderFrameHost,
  // we should not try to do a proactive swap.
  // TODO(crbug.com/333743493): After
  // `blink::features::kRelOpenerBcgDependencyHint` ships, we could replace
  // usage of this test specific code path with the use of `has_rel_opener`.
  if (render_frame_host_->HasTestDisabledProactiveBrowsingInstanceSwap()) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_ProactiveSwapDisabled);
  }
  // We should only do proactive swap if it's needed for
  // the back-forward cache (and the bfcache flag is enabled).
  if (!IsBackForwardCacheEnabled()) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_ProactiveSwapDisabled);
  }

  // Only primary main frames are eligible to swap BrowsingInstances.
  if (frame_tree_node_->GetFrameType() != FrameType::kPrimaryMainFrame) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_NotPrimaryMainFrame);
  }

  // If the frame has not committed any navigation yet, we should not try to do
  // a proactive swap.
  if (!render_frame_host_->has_committed_any_navigation()) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_HasNotComittedAnyNavigation);
  }

  // Skip cases when there are other windows that might script this one.
  SiteInstanceImpl* current_instance = render_frame_host_->GetSiteInstance();
  if (current_instance->GetRelatedActiveContentsCount() > 1u) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents);
  }

  // Even if there are currently no other windows, the destination page may open
  // a window, then if the user navigates back, the previous page may expect to
  // be able to script the opened window. A proactive swap for the first
  // navigation would break scripting in this case. See crbug.com/40281878 for
  // an example. For pages that could be affected by this, the intended
  // mechanism to opt-out of proactive swaps is to use an explicit "opener" rel,
  // which signals that interactions with the opener are expected.
  if (has_rel_opener) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_InitiatorRequestedNoProactiveSwap);
  }

  // "about:blank" and chrome-native-URL do not "use" a SiteInstance. This
  // allows the SiteInstance to be reused cross-site. Starting a new
  // BrowsingInstance would prevent the SiteInstance to be reused, that's why
  // this case is excluded here.
  if (!current_instance->HasSite()) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite);
  }

  // Do not do a proactive BrowsingInstance swap when the previous document's
  // scheme is not HTTP/HTTPS, since only HTTP/HTTPS documents are eligible for
  // back-forward cache.
  const GURL& current_url = render_frame_host_->GetLastCommittedURL();
  if (!current_url.SchemeIsHTTPOrHTTPS()) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_SourceURLSchemeIsNotHTTPOrHTTPS);
  }

  // WebView guests currently need to stay in the same SiteInstance and
  // BrowsingInstance.
  if (current_instance->IsGuest()) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_Guest);
  }

  // We should check whether the new page will result in adding a new history
  // entry or not. If not, we should not do a proactive BrowsingInstance swap,
  // because these navigations are not interesting for bfcache (the old page
  // will not get into the bfcache). Cases include:
  // 1) When we know we're going to replace the history entry.
  if (should_replace_current_entry) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_WillReplaceEntry);
  }
  // Navigations where we will reuse the history entry:
  // 2) Different-document but same URL navigations. These navigations are
  // not classified as same-document (which got filtered earlier) so they will
  // use a different document, but they will reuse the history entry in
  // RendererDidNavigateToExistingEntry. They will usually be converted to a
  // reload (and would be handled below), but not always (e.g., POSTs to the
  // same URL use the same entry but aren't considered reloads).
  bool is_same_url = current_url.EqualsIgnoringRef(destination_url_info.url);
  if (is_same_url) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_SameUrlNavigation);
  }
  // 3) Reloads. Note that most reloads will not actually reach this part, as
  // ShouldSwapBrowsingInstancesForNavigation will return early if the reload
  // has a destination SiteInstance. Reloads that don't have a destination
  // SiteInstance include: doing reload after a replaceState call, reloading a
  // URL for which we've just installed a hosted app, and duplicating a tab.
  if (is_reload) {
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_Reload);
  }

  bool same_site = is_same_site.Get(*render_frame_host_, destination_url_info);

  auto bfcache_eligibility = GetNavigationController()
                                 .GetBackForwardCache()
                                 .GetFutureBackForwardCacheEligibilityPotential(
                                     render_frame_host_.get());
  if (bfcache_eligibility.CanStore()) {
    return BrowsingContextGroupSwap::CreateProactiveSwap(
        same_site ? ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap
                  : ShouldSwapBrowsingInstance::kYes_CrossSiteProactiveSwap);
  } else {
    BackForwardCacheMetrics* back_forward_cache_metrics =
        render_frame_host_->GetBackForwardCacheMetrics();
    if (back_forward_cache_metrics) {
      // Reasons set in the metrics object will be used for DevTools and
      // NotRestoredReasons API. We should include non-sticky reasons as well
      // here for better debugging, though non-sticky features might get cleaned
      // in pagehide handlers.
      BackForwardCacheCanStoreDocumentResultWithTree
          eligibility_including_non_sticky =
              GetNavigationController()
                  .GetBackForwardCache()
                  .GetCompleteBackForwardCacheEligibilityForReporting(
                      render_frame_host_.get());
      back_forward_cache_metrics->SetNotRestoredReasons(
          eligibility_including_non_sticky);
    }
    return BrowsingContextGroupSwap::CreateNoSwap(
        ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache);
  }
}

scoped_refptr<SiteInstanceImpl>
RenderFrameHostManager::GetSiteInstanceForNavigation(
    const UrlInfo& dest_url_info,
    SiteInstanceImpl* source_instance,
    SiteInstanceImpl* dest_instance,
    SiteInstanceImpl* candidate_instance,
    ui::PageTransition transition,
    NavigationRequest::ErrorPageProcess error_page_process,
    bool is_reload,
    bool is_same_document,
    IsSameSiteGetter& is_same_site,
    bool dest_is_view_source_mode,
    bool was_server_redirect,
    CoopSwapResult coop_swap_result,
    bool should_replace_current_entry,
    bool force_new_browsing_instance,
    bool has_rel_opener,
    BrowsingContextGroupSwap* should_swap_result,
    std::string* reason) {
  // On renderer-initiated navigations, when the frame initiating the navigation
  // and the frame being navigated differ, |source_instance| is set to the
  // SiteInstance of the initiating frame. |dest_instance| is present on session
  // history navigations. The two cannot be set simultaneously.
  DCHECK(!source_instance || !dest_instance);

  SiteInstanceImpl* current_instance = render_frame_host_->GetSiteInstance();

  // Determine if we need a new BrowsingInstance for this entry.  If true, this
  // implies that it will get a new SiteInstance (and likely process), and that
  // other tabs in the current BrowsingInstance will be unable to script it.
  // This is used for cases that require a process swap even in the
  // process-per-tab model, such as WebUI pages.

  // First determine the effective URL of the current RenderFrameHost. This is
  // the last URL it successfully committed. If it has yet to commit a URL, this
  // falls back to the Site URL of its SiteInstance.
  // Note: the effective URL of the current RenderFrameHost may differ from the
  // URL of the last committed NavigationEntry, which cannot be used to decide
  // whether to use a new SiteInstance. This happens when navigating a subframe,
  // or when a new RenderFrameHost has been swapped in at the beginning of a
  // navigation to replace a crashed RenderFrameHost.
  BrowserContext* browser_context =
      GetNavigationController().GetBrowserContext();
  const GURL& current_effective_url =
      !render_frame_host_->last_successful_url().is_empty()
          ? SiteInstanceImpl::GetEffectiveURL(
                browser_context, render_frame_host_->last_successful_url())
          : render_frame_host_->GetSiteInstance()->GetSiteInfo().site_url();

  // Determine if the current RenderFrameHost is in view source mode.
  // TODO(clamy): If the current_effective_url doesn't match the last committed
  // NavigationEntry's URL, current_is_view_source_mode should not be computed
  // using the NavigationEntry. This can happen when a tab crashed, and a new
  // RenderFrameHost was swapped in at the beginning of the navigation. See
  // https://crbug.com/766630.
  NavigationEntry* current_entry =
      GetNavigationController().GetLastCommittedEntry();
  bool current_is_view_source_mode = (!current_entry->IsInitialEntry())
                                         ? current_entry->IsViewSourceMode()
                                         : dest_is_view_source_mode;
  *should_swap_result =
      force_new_browsing_instance
          ? BrowsingContextGroupSwap::CreateProactiveSwap(
                ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap)
          : ShouldSwapBrowsingInstancesForNavigation(
                current_effective_url, current_is_view_source_mode,
                source_instance, current_instance, dest_instance, dest_url_info,
                dest_is_view_source_mode, transition, error_page_process,
                is_reload, is_same_document, is_same_site, coop_swap_result,
                was_server_redirect, should_replace_current_entry,
                has_rel_opener);

  TraceShouldSwapBrowsingInstanceResult(frame_tree_node_->frame_tree_node_id(),
                                        should_swap_result->reason());

  if (frame_tree_node_->IsMainFrame()) {
    if (BackForwardCacheMetrics* back_forward_cache_metrics =
            render_frame_host_->GetBackForwardCacheMetrics()) {
      back_forward_cache_metrics->SetBrowsingInstanceSwapResult(
          should_swap_result->reason(), render_frame_host_.get());
    }
  }

  SiteInstanceDescriptor new_instance_descriptor = DetermineSiteInstanceForURL(
      dest_url_info, source_instance, current_instance, dest_instance,
      transition, error_page_process, is_same_site, *should_swap_result,
      was_server_redirect, reason);

  scoped_refptr<SiteInstanceImpl> new_instance =
      ConvertToSiteInstance(new_instance_descriptor, candidate_instance);
  DCHECK(IsSiteInstanceCompatibleWithWebExposedIsolation(
      new_instance.get(), dest_url_info.web_exposed_isolation_info));
  CHECK(!new_instance->GetSiteInfo().agent_cluster_key() ||
        new_instance->GetSiteInfo()
                .agent_cluster_key()
                ->GetCrossOriginIsolationKey() ==
            dest_url_info.cross_origin_isolation_key);

  // If `should_swap_result.ShouldSwap()` is true, we must use a different
  // SiteInstance in a different BrowsingInstance as the current one.
  if (should_swap_result->ShouldSwap()) {
    CHECK_NE(new_instance, current_instance);
    CHECK(!new_instance->IsRelatedSiteInstance(current_instance));
  }

  if (new_instance == current_instance) {
    // If we're navigating to the same site instance, we won't need to use the
    // current spare RenderProcessHost.
    RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedSiteInstance(
        new_instance.get());
  }

  // Double-check that the new SiteInstance is associated with the right
  // BrowserContext.
  DCHECK_EQ(new_instance->GetBrowserContext(), browser_context);

  // If |new_instance| is a new SiteInstance for a subframe or a fenced frame
  // that require a dedicated process, set its process reuse policy so that such
  // subframes and fenced frames are consolidated into existing processes for
  // that site. Avoid aggressive process reuse for PDF content frames.
  // TODO(crbug.com/40230422): The model described in fenced frames process
  // isolation explainer is still in the design stage. Determining correctness
  // here will also involve resolving on the FF process model plan (see
  // https://github.com/WICG/fenced-
  // frame/blob/master/explainer/process_isolation.md).
  if (!frame_tree_node_->IsOutermostMainFrame() &&
      !new_instance->HasProcess() && new_instance->RequiresDedicatedProcess() &&
      !new_instance->IsPdf()) {
    // Also give the embedder and user-specifiable feature a chance to override
    // this decision. Certain frames have different enough workloads so that
    // it's better to avoid placing a subframe into an existing process for
    // better performance isolation.  See https://crbug.com/899418.
    if (!base::FeatureList::IsEnabled(features::kDisableProcessReuse) &&
        GetContentClient()
            ->browser()
            ->ShouldEmbeddedFramesTryToReuseExistingProcess(
                frame_tree_node_->GetParentOrOuterDocument()
                    ->GetOutermostMainFrame())) {
      new_instance->set_process_reuse_policy(
          ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME);
    }
  }

  UpdateProcessReusePolicyForProcessPerSiteWithMainFrameThreshold(
      new_instance.get(), frame_tree_node_);

  bool is_same_site_proactive_swap =
      (should_swap_result->reason() ==
       ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap);

  // Decide whether `new_instance` could reuse an existing process from either
  // the current or the candidate SiteInstance. These heuristics help avoid
  // swapping processes unnecessarily, which might cause extra latency. Note
  // that this needs to be balanced carefully with creating a clean slate, as
  // certain scenarios like opening noopener popups do expect a process swap.
  //
  // Note: process reuse might not be possible in some cases, e.g. for
  // cross-site navigations when the current SiteInstance needs a dedicated
  // process.  This will be enforced by the checks inside
  // ReuseExistingProcessIfPossible().
  RenderProcessHost* process_to_reuse = nullptr;

  // Process-reuse cases include:
  // 1) When BackForwardCache is enabled and we did a same-site proactive
  // BrowsingInstance swap.
  // Note 1: When BackForwardCache is disabled, we typically reuse processes on
  // same-site navigations. This follows that behavior.
  // See crbug.com/1122974 for further details.
  if (IsBackForwardCacheEnabled() && is_same_site_proactive_swap) {
    process_to_reuse = current_instance->GetProcess();
  }

  // 2) When we're doing a same-site history navigation with different
  // BrowsingInstances. We typically do not swap BrowsingInstances on same-site
  // navigations. This might indicate that the original navigation did a
  // proactive BrowsingInstance swap (and process-reuse) before, so we should
  // try to reuse the current process.
  bool is_history_navigation = !!dest_instance;
  bool swapped_browsing_instance =
      !new_instance->IsRelatedSiteInstance(current_instance);
  bool is_same_site_proactive_swap_enabled =
      IsBackForwardCacheEnabled();
  if (is_same_site_proactive_swap_enabled && is_history_navigation &&
      swapped_browsing_instance &&
      is_same_site.Get(*render_frame_host_, dest_url_info)) {
    process_to_reuse = current_instance->GetProcess();
  }

  // 3) When we're swapping BrowsingInstances due to a COOP mismatch, and we
  // have an existing process that's suitable for the new SiteInstance. This
  // has three cases:
  //
  //   - If there's a candidate SiteInstance that differs from the target
  //     SiteInstance, try to reuse the candidate SiteInstance's
  //     process. This typically happens on cross-site navigations when we've
  //     created a speculative RenderFrameHost and learned about the COOP
  //     mismatch at response time. While we will have to recreate a
  //     speculative RenderFrameHost in a new SiteInstance and
  //     BrowsingInstance, we can try to reuse the (already warmed up) process
  //     from the old speculative RenderFrameHost if its SiteInstance is
  //     compatible with the new one.
  //
  //   - If the navigation is same-site, we can try to reuse the
  //     current SiteInstance's process, but only if there is just one
  //     WebContents in the current BrowsingInstance.  In this case, we can be
  //     reasonably sure that the old page will be replaced by the new page in
  //     the current process, and there's less of a need for clean slate.
  //     Having more than one WebContents indicates that a page may be opening
  //     a COOP popup, which should use a fresh process to get a clean slate
  //     similarly to noopener popups.
  //
  //   - If the navigation is prerender initial navigation, we can also try to
  //     reuse the current SiteInstance's process. This is due to the fact that,
  //     at the time of the creation of PrerenderHost to start prerender initial
  //     navigation, a new FrameTree is initialized with new BrowsingInstance /
  //     SiteInstance, and a new unused process will be assigned to it
  //     accordingly.
  //     TODO(crbug.com/41492112): Note that it is a short term-fix. Ideally we
  //     could try to stay in the unassigned SiteInstance / BrowsingInstance in
  //     this scenario, rather than swapping to a new BrowsingInstance and
  //     reusing the process. Additionally, it could cover other navigations
  //     similar to prerender, which are started from unassigned SiteInstance
  //     and unlocked processes.
  //
  // TODO(alexmos): Study if this kind of reuse might be useful in other cases
  // beyond COOP.
  if (should_swap_result->type() == BrowsingContextGroupSwapType::kCoopSwap ||
      should_swap_result->type() ==
          BrowsingContextGroupSwapType::kRelatedCoopSwap) {
    if (candidate_instance && candidate_instance != new_instance &&
        candidate_instance->GetSiteInfo() == new_instance->GetSiteInfo()) {
      process_to_reuse = candidate_instance->GetProcess();
    } else if (is_same_site.Get(*render_frame_host_, dest_url_info) &&
               current_instance->GetRelatedActiveContentsCount() == 1) {
      process_to_reuse = current_instance->GetProcess();
    } else if (base::FeatureList::IsEnabled(
                   features::kProcessReuseOnPrerenderCOOPSwap) &&
               frame_tree_node_->frame_tree().is_prerendering()) {
      process_to_reuse = current_instance->GetProcess();
    }
  }

  if (process_to_reuse) {
    DCHECK(frame_tree_node_->IsMainFrame());
    new_instance->ReuseExistingProcessIfPossible(process_to_reuse);
  }

  // We want fenced frame BrowsingInstances to share the same default
  // process with their embedding BrowsingInstance. The code below forces
  // SiteInstances in the embedder and fenced frame BrowsingInstances to
  // share the same default process when they don't need a dedicated process.
  // With sites that do require a dedicated process, we reuse processes via the
  // subframe reuse policy (we set the reuse policy to
  // REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME).
  if (!current_frame_host()->IsOutermostMainFrame() &&
      !new_instance->HasProcess() &&
      !new_instance->RequiresDedicatedProcess()) {
    ReuseDefaultProcessFromDifferentBrowsingInstanceIfPossible(
        new_instance, current_frame_host());
  }

  return new_instance;
}

bool RenderFrameHostManager::InitializeMainRenderFrameForImmediateUse() {
  // TODO(jam): this copies some logic inside GetFrameHostForNavigation, which
  // also duplicates logic in Navigate. They should all use this method, but
  // that involves slight reordering.
  // http://crbug.com/794229
  DCHECK(frame_tree_node_->IsMainFrame());
  if (render_frame_host_->IsRenderFrameLive())
    return true;

  render_frame_host_->reset_must_be_replaced();

  // If the render frame was previously deleted, this is a signal that the
  // RenderFrameHost is being reused after a crash.
  if (render_frame_host_->is_render_frame_deleted()) {
    // The DocumentAssociatedData needs to be reinitialized now to ensure that
    // the render frame is created with a new DocumentToken. Note that this
    // needs to remain in sync with `RenderFrameHostImpl::RenderFrameCreated()`,
    // which dispatches the actual notification about a new Page object for this
    // case.
    render_frame_host_->ReinitializeDocumentAssociatedDataForReuseAfterCrash(
        /* passkey */ {});

    // Since it's possible for the now reinitialized main frame to create new
    // sub-frames/windows we need to also reinitialize the
    // RuntimeFeatureStateDocumentData, since those new frames/windows will
    // query it on their creation.

    RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
        render_frame_host_.get(), blink::RuntimeFeatureStateContext());
  }

  if (!ReinitializeMainRenderFrame(render_frame_host_.get())) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  EnsureRenderFrameHostPageFocusConsistent();

  // TODO(nasko): This is a very ugly hack. The Chrome extensions process
  // manager still uses NotificationService and expects to see a
  // RenderViewHost changed notification after WebContents and
  // RenderFrameHostManager are completely initialized. This should be
  // removed once the process manager moves away from NotificationService.
  // See https://crbug.com/462682.
  //
  // TODO(https://crbug.com/338233133): The extensions process manager does
  // not use NotificationService; clean this up.
  delegate_->NotifyMainFrameSwappedFromRenderManager(nullptr,
                                                     render_frame_host_.get());
  return true;
}

void RenderFrameHostManager::PrepareForInnerDelegateAttach(
    RenderFrameHost::PrepareForInnerWebContentsAttachCallback callback) {
  CHECK(frame_tree_node_->parent());
  attach_inner_delegate_callback_ = std::move(callback);
  DCHECK_EQ(attach_to_inner_delegate_state_, AttachToInnerDelegateState::NONE);
  attach_to_inner_delegate_state_ = AttachToInnerDelegateState::PREPARE_FRAME;

  // TODO(crbug.com/40249634): Some of these may no longer be necessary
  // now that MimeHandlerView's embedded case uses the same code path as the
  // full page case.
  if (current_frame_host()->ShouldDispatchBeforeUnload(
          false /* check_subframes_only */)) {
    // If there are beforeunload handlers in the frame or a nested subframe we
    // should first dispatch the event and wait for the ACK form the renderer
    // before proceeding with CreateNewFrameForInnerDelegateAttachIfNecessary.
    current_frame_host()->DispatchBeforeUnload(
        RenderFrameHostImpl::BeforeUnloadType::INNER_DELEGATE_ATTACH, false);
    return;
  }
  CreateNewFrameForInnerDelegateAttachIfNecessary();
}

RenderFrameHostManager::SiteInstanceDescriptor
RenderFrameHostManager::DetermineSiteInstanceForURL(
    const UrlInfo& dest_url_info,
    SiteInstanceImpl* source_instance,
    SiteInstanceImpl* current_instance,
    SiteInstanceImpl* dest_instance,
    ui::PageTransition transition,
    NavigationRequest::ErrorPageProcess error_page_process,
    IsSameSiteGetter& is_same_site,
    BrowsingContextGroupSwap browsing_context_group_swap,
    bool was_server_redirect,
    std::string* reason) {
  // Note that this function should return a SiteInstanceDescriptor with
  // SiteInstanceRelation::UNRELATED or
  // SiteInstanceRelation::RELATED_IN_COOP_GROUP relations to `current_instance`
  // iff `browsing_context_group_swap.ShouldSwap()` is true.

  // === Error page handling ===
  // Note that these must be the first checks to avoid picking the destination
  // instance or other instances.
  if (error_page_process ==
      NavigationRequest::ErrorPageProcess::kCurrentProcess) {
    // If this is an error page that must reuse the current process, ensure that
    // `current_instance` is used.
    AppendReason(reason,
                 "DetermineSiteInstanceForURL => error-current-instance");
    return SiteInstanceDescriptor(current_instance);
  } else if (error_page_process ==
             NavigationRequest::ErrorPageProcess::kIsolatedProcess) {
    // If error page navigations should be isolated, ensure a dedicated
    // SiteInstance is used for them.
    CHECK(frame_tree_node_->IsErrorPageIsolationEnabled());
    // If the target URL requires a BrowsingInstance swap, put the error page
    // in a new BrowsingInstance, since the scripting relationships would
    // have been broken anyway if there were no error. Otherwise, we keep it
    // in the same BrowsingInstance to preserve scripting relationships after
    // reloads. In UrlInfo below we use kNone for OriginIsolationRequest since
    // error pages cannot request origin isolation: this is done implicitly in
    // the UrlInfoInit constructor.
    AppendReason(reason,
                 "DetermineSiteInstanceForURL => error-isolated-instance");

    // Top level frames ending up as error pages should use COOP: unsafe-none.
    // They should therefore be non isolated. Note that it is possible for a
    // top-level error page to have a nullopt WebExposedIsolationInfo, in
    // certain post-commit error pages on top of about:blank scenarios.
    DCHECK(!frame_tree_node_->IsOutermostMainFrame() ||
           !dest_url_info.web_exposed_isolation_info.has_value() ||
           dest_url_info.web_exposed_isolation_info.value() ==
               WebExposedIsolationInfo::CreateNonIsolated());

    UrlInfo computed_url_info(
        UrlInfoInit(GURL(kUnreachableWebDataURL))
            .WithWebExposedIsolationInfo(
                dest_url_info.web_exposed_isolation_info));
    if (!browsing_context_group_swap.ShouldSwap()) {
      return SiteInstanceDescriptor(computed_url_info,
                                    SiteInstanceRelation::RELATED);
    }

    if (browsing_context_group_swap.type() ==
        BrowsingContextGroupSwapType::kRelatedCoopSwap) {
      // If we're dealing with COOP: restrict-properties, we need to stay in the
      // same CoopRelatedGroup, so that further navigations get a
      // chance to preserve their scriptability.
      return SiteInstanceDescriptor(
          computed_url_info, SiteInstanceRelation::RELATED_IN_COOP_GROUP);
    }

    return SiteInstanceDescriptor(computed_url_info,
                                  SiteInstanceRelation::UNRELATED);
  }

  // If the entry has an instance already we should usually use it, unless it is
  // no longer suitable.
  if (dest_instance &&
      CanUseDestinationInstance(dest_url_info, current_instance, dest_instance,
                                error_page_process, browsing_context_group_swap,
                                was_server_redirect)) {
    AppendReason(reason, "DetermineSiteInstanceForURL => dest_instance");
    return SiteInstanceDescriptor(dest_instance);
  }

  // COOP: restrict-properties requires that we swap BrowsingInstance, but
  // preserve a relation to the previous BrowsingInstance.
  bool can_use_source_instance =
      CanUseSourceSiteInstance(dest_url_info, source_instance,
                               was_server_redirect, error_page_process, reason);
  if (browsing_context_group_swap.type() ==
      BrowsingContextGroupSwapType::kRelatedCoopSwap) {
    // We typically expect `source_instance` to be in the same BrowsingInstance
    // as `current_instance`. However when extensions use the chrome.tabs.update
    // API to navigate to about:blank, `source_instance` is set to the
    // extension's SiteInstance, which should be in a different
    // BrowsingInstance. In that case, `source_instance` should not be in a
    // different BrowsingInstance in the same CoopRelatedGroup as
    // `current_instance`, but use its own extension's CoopRelatedGroup. Note
    // that it can be in another BrowsingInstance in another CoopRelatedGroup,
    // which we have to consider for the kSwap case below.
    // TODO(crbug.com/40186710): Add a test verifying that we cannot end
    // up in that situation using chrome.tabs.update. This could be the case if
    // an extension use that API to navigate from a COOP: restrict-properties
    // page to about:blank.
    CHECK(!can_use_source_instance ||
          source_instance->IsRelatedSiteInstance(current_instance) ||
          !source_instance->IsCoopRelatedSiteInstance(current_instance));
    AppendReason(reason,
                 "DetermineSiteInstanceForURL => related_in_COOP_group");
    return SiteInstanceDescriptor(dest_url_info,
                                  SiteInstanceRelation::RELATED_IN_COOP_GROUP);
  }

  // If a swap is required, we need to force the SiteInstance AND
  // BrowsingInstance to be different ones, using CreateForURL.
  if (browsing_context_group_swap.ShouldSwap()) {
    // In rare cases, `source_instance` maybe be already in another
    // BrowsingInstance from `current_instance` (e.g. see how the
    // ExtensionApiTabTest.HostPermission test uses chrome.tabs.update API to
    // navigate from "chrome://new-tab-page/" to "about:blank").  In such cases,
    // using `source_instance` will 1) effectively force browsing instance swap
    // and 2) use a process compatible with "about:blank"'s origin (unlike a
    // new, unrelated SiteInstance that might use an unlocked process even
    // when the origin requires a locked process).
    if (can_use_source_instance &&
        !source_instance->IsRelatedSiteInstance(current_instance)) {
      AppendReason(reason,
                   "DetermineSiteInstanceForURL => source_instance"
                   " (browsing-instance-swap)");
      return SiteInstanceDescriptor(source_instance);
    }

    // Force browsing instance_swap by asking for a new, unrelated SiteInstance.
    AppendReason(reason,
                 "DetermineSiteInstanceForURL / browsing-instance-swap");
    return SiteInstanceDescriptor(dest_url_info,
                                  SiteInstanceRelation::UNRELATED);
  }

  // TODO(crbug.com/40447789): Don't create OOPIFs on the NTP.  Remove
  // this when the NTP supports OOPIFs or is otherwise omitted from site
  // isolation policy.
  if (!frame_tree_node_->IsMainFrame()) {
    SiteInstanceImpl* parent_site_instance =
        frame_tree_node_->parent()->GetSiteInstance();
    if (GetContentClient()->browser()->ShouldStayInParentProcessForNTP(
            dest_url_info.url, parent_site_instance->GetSiteURL())) {
      // NTP is considered non-isolated.
      DCHECK(!dest_url_info.IsIsolated());
      AppendReason(reason,
                   "DetermineSiteInstanceForURL => parent_site_instance");
      return SiteInstanceDescriptor(parent_site_instance);
    }
  }

  // Check if we should use `source_instance`, such as for about:blank and data:
  // URLs.  Preferring `source_instance` over a site-less `current_instance` is
  // important in session restore scenarios which should commit in the
  // SiteInstance based on FrameNavigationEntry's initiator_origin.
  if (can_use_source_instance) {
    AppendReason(reason, "DetermineSiteInstanceForURL => source_instance");
    return SiteInstanceDescriptor(source_instance);
  }

  DCHECK_EQ(GetNavigationController().GetBrowserContext(),
            current_instance->GetBrowserContext());

  // If we haven't used our SiteInstance yet, then we can use it for this
  // navigation.  We won't commit the SiteInstance to this site until the
  // response is received (in OnResponseStarted).
  // TODO(crbug.com/40276947): In theory we should be able to go for an
  // unused SiteInstance with the same web exposed isolation status.
  if (!current_instance->HasSite() && !dest_url_info.IsIsolated() &&
      !current_instance->IsCrossOriginIsolated()) {
    // If we've already created a SiteInstance for our destination, we don't
    // want to use this unused SiteInstance; use the existing one.  (We don't
    // do this check if the current_instance has a site, because for now, we
    // want to compare against the current URL and not the SiteInstance's site.
    // In this case, there is no current URL, so comparing against the site is
    // ok.  See additional comments below.)
    const SiteInfo dest_site_info =
        current_instance->DeriveSiteInfo(dest_url_info);
    if (current_instance->HasRelatedSiteInstance(dest_site_info)) {
      AppendReason(reason,
                   "DetermineSiteInstanceForURL / !current->HasSite / "
                   "has-related-site-instance");
      return SiteInstanceDescriptor(dest_url_info,
                                    SiteInstanceRelation::RELATED);
    }

    // If the URL's site should use process-per-site mode and there is an
    // existing process for the site, we should use it.  We can call
    // GetRelatedSiteInstance() for this, which will eagerly set the site and
    // thus use the correct process.
    bool use_process_per_site =
        dest_site_info.ShouldUseProcessPerSite(
            current_instance->GetBrowserContext()) &&
        RenderProcessHostImpl::GetSoleProcessHostForSite(
            current_instance->GetIsolationContext(), dest_site_info);
    if (use_process_per_site) {
      AppendReason(reason,
                   "DetermineSiteInstanceForURL / !current->HasSite / "
                   "process-per-site");
      return SiteInstanceDescriptor(dest_url_info,
                                    SiteInstanceRelation::RELATED);
    }

    // For extensions and apps we do not want to use the `current_instance` if
    // it has no site, since it will have a non-privileged
    // RenderProcessHost. Create a new SiteInstance for this URL instead (with
    // the correct process type).
    if (!current_instance->IsSuitableForUrlInfo(dest_url_info)) {
      AppendReason(reason,
                   "DetermineSiteInstanceForURL / !current->HasSite / "
                   "!current_instance->IsSuitable");
      return SiteInstanceDescriptor(dest_url_info,
                                    SiteInstanceRelation::RELATED);
    }

    AppendReason(reason, "DetermineSiteInstanceForURL => current_instance");
    return SiteInstanceDescriptor(current_instance);
  }

  // Use the current SiteInstance for same site navigations.
  if (is_same_site.Get(*render_frame_host_, dest_url_info)) {
    AppendReason(reason, "DetermineSiteInstanceForURL / same-site-navigation");
    DCHECK_EQ(current_instance, render_frame_host_->GetSiteInstance());
    return SiteInstanceDescriptor(current_instance);
  }

  // Shortcut some common cases for reusing an existing frame's SiteInstance.
  // There are several reasons for this:
  // - with hosted apps, this allows same-site, non-app subframes to be kept
  //   inside the hosted app process.
  // - this avoids putting same-site iframes into different processes after
  //   navigations from isolated origins.  This matters for some OAuth flows;
  //   see https://crbug.com/796912.
  //
  // TODO(alexmos): Ideally, the right SiteInstance for these cases should be
  // found later, as part of creating a new related SiteInstance from
  // BrowsingInstance::GetSiteInstanceForURL().  However, the lookup there (1)
  // does not properly deal with hosted apps (see https://crbug.com/718516),
  // and (2) does not yet deal with cases where a SiteInstance is shared by
  // several sites that don't require a dedicated process (see
  // https://crbug.com/787576).
  if (!frame_tree_node_->IsMainFrame()) {
    RenderFrameHostImpl* main_frame =
        frame_tree_node_->frame_tree().root()->current_frame_host();
    if (IsCandidateSameSite(main_frame, dest_url_info)) {
      AppendReason(reason,
                   "DetermineSiteInstanceForURL / subframe-reuse => "
                   "main-frame-instance");
      return SiteInstanceDescriptor(main_frame->GetSiteInstance());
    }
    RenderFrameHostImpl* parent = frame_tree_node_->parent();
    if (IsCandidateSameSite(parent, dest_url_info)) {
      AppendReason(reason,
                   "DetermineSiteInstanceForURL / subframe-reuse => "
                   "parent-instance");
      return SiteInstanceDescriptor(parent->GetSiteInstance());
    }
  }
  if (frame_tree_node_->opener()) {
    RenderFrameHostImpl* opener_frame =
        frame_tree_node_->opener()->current_frame_host();
    if (IsCandidateSameSite(opener_frame, dest_url_info)) {
      AppendReason(reason, "DetermineSiteInstanceForURL => opener-instance");
      return SiteInstanceDescriptor(opener_frame->GetSiteInstance());
    }
  }

  // Keep subframes in the parent's SiteInstance unless a dedicated process is
  // required for either the parent or the subframe's destination URL. Although
  // this consolidation is usually handled by default SiteInstances, there are
  // some corner cases in which default SiteInstances cannot currently be used,
  // such as file: URLs.  This logic prevents unneeded OOPIFs in those cases.
  // This turns out to be important for correctness on Android Webview, which
  // does not yet support OOPIFs (https://crbug.com/1101214).
  // TODO(crbug.com/40704573): Remove this block when default
  // SiteInstances support file: URLs.
  //
  // Also if kProcessSharingWithStrictSiteInstances is enabled, don't lump the
  // subframe into the same SiteInstance as the parent. These separate
  // SiteInstances can get assigned to the same process later.
  if (!base::FeatureList::IsEnabled(
          features::kProcessSharingWithStrictSiteInstances)) {
    if (!frame_tree_node_->IsMainFrame()) {
      RenderFrameHostImpl* parent = frame_tree_node_->parent();
      auto& parent_isolation_context =
          parent->GetSiteInstance()->GetIsolationContext();

      auto site_info =
          SiteInfo::Create(parent_isolation_context, dest_url_info);
      if (!parent->GetSiteInstance()->RequiresDedicatedProcess() &&
          !site_info.RequiresDedicatedProcess(parent_isolation_context)) {
        AppendReason(reason,
                     "DetermineSiteInstanceForURL => parent-instance"
                     " (no-strict-site-instances)");
        return SiteInstanceDescriptor(parent->GetSiteInstance());
      }
    }
  }

  // Start the new renderer in a new SiteInstance, but in the current
  // BrowsingInstance, unless the destination URL's web-exposed isolated state
  // cannot be hosted by it.
  if (IsSiteInstanceCompatibleWithWebExposedIsolation(
          current_instance, dest_url_info.web_exposed_isolation_info)) {
    AppendReason(reason,
                 "DetermineSiteInstanceForURL / fallback / coop-compatible");
    return SiteInstanceDescriptor(dest_url_info, SiteInstanceRelation::RELATED);
  } else {
    AppendReason(
        reason, "DetermineSiteInstanceForURL / fallback / not-coop-compatible");
    return SiteInstanceDescriptor(dest_url_info,
                                  SiteInstanceRelation::UNRELATED);
  }
}

bool RenderFrameHostManager::CanUseDestinationInstance(
    const UrlInfo& dest_url_info,
    SiteInstanceImpl* current_instance,
    SiteInstanceImpl* dest_instance,
    NavigationRequest::ErrorPageProcess error_page_process,
    const BrowsingContextGroupSwap& browsing_context_group_swap,
    bool was_server_redirect) {
  // Start by verifying that the dest_instance is compatible with the browsing
  // context group swap decision.
  if (browsing_context_group_swap.ShouldSwap()) {
    // 1. If we've decided that the target SiteInstance cannot be in the same
    // BrowsingInstance, and that the dest_instance is, we should not reuse it.
    if (dest_instance->IsRelatedSiteInstance(current_instance)) {
      return false;
    }

    // 2. If we aren't looking for a SiteInstance in the same CoopRelatedGroup,
    // then don't use a dest_instance in that group.
    if (browsing_context_group_swap.type() !=
            BrowsingContextGroupSwapType::kRelatedCoopSwap &&
        dest_instance->IsCoopRelatedSiteInstance(current_instance)) {
      return false;
    }
  }

  // Note: The later call to IsSuitableForUrlInfo does not have context
  // about error page navigations, so we cannot rely on it to return correct
  // value when error pages are involved.
  if (!IsSiteInstanceCompatibleWithErrorIsolation(
          dest_instance, *frame_tree_node_, error_page_process)) {
    return false;
  }

  if (dest_instance->GetSiteInfo().agent_cluster_key() &&
      dest_instance->GetSiteInfo()
              .agent_cluster_key()
              ->GetCrossOriginIsolationKey() !=
          dest_url_info.cross_origin_isolation_key) {
    return false;
  }

  if (!IsSiteInstanceCompatibleWithWebExposedIsolation(
          dest_instance, dest_url_info.web_exposed_isolation_info)) {
    return false;
  }

  // TODO(nasko,creis): The check whether data: or about: URLs are
  // allowed to commit in the current process should be in
  // IsSuitableForUrlInfo. However, making this change has further
  // implications and needs more investigation of what behavior changes.
  // For now, use a conservative approach and explicitly check before
  // calling IsSuitableForUrlInfo.
  // Make sure that if the destination frame is sandboxed that we don't
  // skip the IsSuitableForUrlInfo() check. Note that it's impossible to
  // have a sandboxed parent but unsandboxed child.
  bool is_data_or_about_and_not_sandboxed =
      (dest_url_info.url.SchemeIs(url::kDataScheme) ||
       IsAbout(dest_url_info.url)) &&
      !dest_url_info.is_sandboxed;
  if (is_data_or_about_and_not_sandboxed) {
    // Server redirects to data: and about: URLs can only be done by
    // extensions. In this case, we are doing a history navigation to a URL
    // that wasn't redirected by extensions before, but got redirected to a
    // data: or about: URL when doing a history traversal back to it. Since the
    // redirect isn't related to the original page at all, don't use the saved
    // SiteInstance.
    // See also https://crbug.com/1440543, https://crbug.com/1454273, and the
    // comment about a similar case for non-history navigations in
    // `CanUseSourceSiteInstance()`.
    // TODO(crbug.com/40266169): Make `IsSuitableForUrlInfo()` handle
    // this case instead.
    return !was_server_redirect;
  }

  return dest_instance->IsSuitableForUrlInfo(dest_url_info);
}

bool RenderFrameHostManager::IsBrowsingInstanceSwapAllowedForPageTransition(
    ui::PageTransition transition,
    const GURL& dest_url) {
  // Disallow BrowsingInstance swaps for subframes.
  if (!frame_tree_node_->IsMainFrame())
    return false;

  // Skip data: and file: URLs, as some tests rely on browser-initiated
  // navigations to those URLs to stay in the same process.  Swapping
  // BrowsingInstances for those URLs may not carry much benefit anyway, since
  // they're likely less common.
  //
  // Note that such URLs are not considered same-site, but since their
  // SiteInstance site URL is based only on scheme (e.g., all data URLs use a
  // site URL of "data:"), a browser-initiated navigation from one such URL to
  // another will still stay in the same SiteInstance, due to the matching site
  // URL.
  if (dest_url.SchemeIsFile() || dest_url.SchemeIs(url::kDataScheme))
    return false;

  // Allow page transitions corresponding to certain browser-initiated
  // navigations: typing in the URL, using a bookmark, or using search.
  switch (ui::PageTransitionStripQualifier(transition)) {
    case ui::PAGE_TRANSITION_TYPED:
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
    case ui::PAGE_TRANSITION_GENERATED:
    case ui::PAGE_TRANSITION_KEYWORD:
      return true;
    // TODO(alexmos): PAGE_TRANSITION_AUTO_TOPLEVEL is not included due to a
    // bug that would cause unneeded BrowsingInstance swaps for DevTools,
    // https://crbug.com/733767.  Once that bug is fixed, consider adding this
    // transition here.
    default:
      return false;
  }
}

scoped_refptr<SiteInstanceImpl> RenderFrameHostManager::ConvertToSiteInstance(
    const SiteInstanceDescriptor& descriptor,
    SiteInstanceImpl* candidate_instance) {
  SiteInstanceImpl* current_instance = render_frame_host_->GetSiteInstance();

  // If we are asked to return a related SiteInstance but the BrowsingInstance
  // has a different cross_origin_isolated state, something went wrong.
  SCOPED_CRASH_KEY_BOOL("Bug1503252", "is_main_frame",
                        frame_tree_node_->IsOutermostMainFrame());
  SCOPED_CRASH_KEY_BOOL(
      "Bug1503252", "current_is_isolated",
      current_instance->GetWebExposedIsolationInfo().is_isolated());
  SCOPED_CRASH_KEY_BOOL(
      "Bug1503252", "current_is_isolated_app",
      current_instance->GetWebExposedIsolationInfo().is_isolated_application());
  SCOPED_CRASH_KEY_STRING256("Bug1503252", "current_instance_site_info",
                             current_instance->GetSiteInfo().GetDebugString());
  bool descriptor_is_isolated =
      descriptor.dest_url_info.web_exposed_isolation_info
          ? descriptor.dest_url_info.web_exposed_isolation_info->is_isolated()
          : false;
  bool descriptor_is_isolated_application =
      descriptor.dest_url_info.web_exposed_isolation_info
          ? descriptor.dest_url_info.web_exposed_isolation_info
                ->is_isolated_application()
          : false;
  SCOPED_CRASH_KEY_BOOL("Bug1503252", "descriptor_is_isolated",
                        descriptor_is_isolated);
  SCOPED_CRASH_KEY_BOOL("Bug1503252", "descriptor_is_isolated_app",
                        descriptor_is_isolated_application);
  bool origins_match = false;
  if (descriptor_is_isolated &&
      current_instance->GetWebExposedIsolationInfo().is_isolated()) {
    SCOPED_CRASH_KEY_STRING256("Bug1503252", "current_weii_origin",
                               current_instance->GetWebExposedIsolationInfo()
                                   .origin()
                                   .GetDebugString());
    SCOPED_CRASH_KEY_STRING256(
        "Bug1503252", "descriptor_weii_origin",
        descriptor.dest_url_info.web_exposed_isolation_info->origin()
            .GetDebugString());
    origins_match =
        current_instance->GetWebExposedIsolationInfo().origin() ==
        descriptor.dest_url_info.web_exposed_isolation_info->origin();
  }
  SCOPED_CRASH_KEY_BOOL("Bug1503252", "origins_match", origins_match);
  CHECK(descriptor.relation != SiteInstanceRelation::RELATED ||
        WebExposedIsolationInfo::AreCompatible(
            current_instance->GetWebExposedIsolationInfo(),
            descriptor.dest_url_info.web_exposed_isolation_info));

  // Note: If the `candidate_instance` matches the descriptor, it will already
  // be set to `descriptor.existing_site_instance`.
  if (descriptor.existing_site_instance) {
    DCHECK_EQ(descriptor.relation, SiteInstanceRelation::PREEXISTING);
    return descriptor.existing_site_instance.get();
  } else {
    DCHECK_NE(descriptor.relation, SiteInstanceRelation::PREEXISTING);
  }

  // Note: If the `candidate_instance` matches the descriptor,
  // GetRelatedSiteInstance will return it.
  // Note that by the time we get here, we've already ensured that this
  // BrowsingInstance has a compatible cross-origin isolated state, so we are
  // guaranteed to return a SiteInstance that will be compatible with
  // |descriptor.web_exposed_isolation_info|."
  if (descriptor.relation == SiteInstanceRelation::RELATED) {
    return current_instance->GetRelatedSiteInstanceImpl(
        descriptor.dest_url_info);
  }

  if (descriptor.relation == SiteInstanceRelation::RELATED_IN_COOP_GROUP) {
    return current_instance->GetCoopRelatedSiteInstanceImpl(
        descriptor.dest_url_info);
  }

  // At this point we know an unrelated site instance must be returned.

  // If the current SiteInstance has fixed storage partition (e.g. <webview>
  // tags), the new unrelated SiteInstance must also stay in the same
  // StoragePartition.
  UrlInfo dest_url_info = descriptor.dest_url_info;
  if (current_instance->IsFixedStoragePartition()) {
    dest_url_info.storage_partition_config =
        current_instance->GetSiteInfo().storage_partition_config();
  }

  // First check if the candidate SiteInstance matches.  For example, we get
  // here when we recompute the SiteInstance after receiving a response, and
  // `candidate_instance` is the SiteInstance that was created at request start
  // time.
  if (candidate_instance &&
      !current_instance->IsCoopRelatedSiteInstance(candidate_instance) &&
      candidate_instance->DoesSiteInfoForURLMatch(dest_url_info)) {
    return candidate_instance;
  }

  // Otherwise return a new SiteInstance in a new BrowsingInstance.
  return SiteInstanceImpl::CreateForUrlInfo(
      GetNavigationController().GetBrowserContext(), dest_url_info,
      current_instance->IsGuest(),
      current_instance->GetIsolationContext().is_fenced(),
      current_instance->IsFixedStoragePartition());
}

bool RenderFrameHostManager::CanUseSourceSiteInstance(
    const UrlInfo& dest_url_info,
    SiteInstanceImpl* source_instance,
    bool was_server_redirect,
    NavigationRequest::ErrorPageProcess error_page_process,
    std::string* reason) {
  if (!source_instance) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(invalid-source-instance)");
    return false;
  }

  // We use the source SiteInstance in case of data URLs, about:srcdoc pages and
  // about:blank pages because the content is then controlled and/or scriptable
  // by the initiator and therefore needs to stay in the `source_instance`.
  if (!dest_url_info.url.SchemeIs(url::kDataScheme) &&
      !IsAbout(dest_url_info.url)) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(not-data-url-or-about-srcdoc)");
    return false;
  }

  // If `dest_url_info` is sandboxed, then we can't assign it to a SiteInstance
  // that isn't sandboxed. But if the `source_instance` is also sandboxed, then
  // it's possible (e.g. a sandboxed child frame in a sandboxed parent frame).
  auto& source_site_info = source_instance->GetSiteInfo();
  if (dest_url_info.is_sandboxed != source_site_info.is_sandboxed()) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(is-sandboxed-mismatched)");
    return false;
  }
  if (dest_url_info.is_sandboxed &&
      dest_url_info.unique_sandbox_id != source_site_info.unique_sandbox_id()) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(sandbox-id-mismatched)");
    return false;
  }

  // One exception (where data URLs, about:srcdoc or about:blank pages are *not*
  // controlled by the initiator) is when these URLs are reached via a server
  // redirect.
  //
  // Normally, redirects to data: or about: URLs are disallowed as
  // net::ERR_UNSAFE_REDIRECT, but extensions can still redirect arbitrary
  // requests to those URLs using webRequest or declarativeWebRequest API (for
  // an example, see NavigationInitiatedByCrossSiteSubframeRedirectedTo... test
  // cases in the ChromeNavigationBrowserTest test suite.  For such data: URL
  // redirects, the content is controlled by the extension (rather than by the
  // `source_instance`), so we don't use the `source_instance` for data: URLs if
  // there was a server redirect.
  if (was_server_redirect && dest_url_info.url.SchemeIs(url::kDataScheme)) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(server-redirect-data-url)");
    return false;
  }

  // Make sure that error isolation is taken into account.  See also
  // ChromeNavigationBrowserTest.RedirectErrorPageReloadToAboutBlank.
  if (!IsSiteInstanceCompatibleWithErrorIsolation(
          source_instance, *frame_tree_node_, error_page_process)) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(error-isolation)");
    return false;
  }

  if (!IsSiteInstanceCompatibleWithWebExposedIsolation(
          source_instance, dest_url_info.web_exposed_isolation_info)) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(web-exposed-isolation)");
    return false;
  }

  if (source_instance->GetSiteInfo().agent_cluster_key() &&
      source_instance->GetSiteInfo()
              .agent_cluster_key()
              ->GetCrossOriginIsolationKey() !=
          dest_url_info.cross_origin_isolation_key) {
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(cross-origin-isolation-key)");
    return false;
  }

  // PDF content should never share a SiteInstance with non-PDF content. In
  // practice, this prevents the PDF viewer extension from incorrectly sharing
  // a process with PDF content that was loaded from a data URL.
  if (dest_url_info.is_pdf) {
    DCHECK(!source_instance->GetProcess()->IsPdf());
    AppendReason(reason,
                 "CanUseSourceSiteInstance => false "
                 "(pdf-content)");
    return false;
  }

  // Okay to use `source_instance`.
  AppendReason(reason, "CanUseSourceSiteInstance => true");
  return true;
}

bool RenderFrameHostManager::IsCandidateSameSite(RenderFrameHostImpl* candidate,
                                                 const UrlInfo& dest_url_info) {
  DCHECK_EQ(GetNavigationController().GetBrowserContext(),
            candidate->GetSiteInstance()->GetBrowserContext());
  if (!WebExposedIsolationInfo::AreCompatible(
          candidate->GetSiteInstance()->GetWebExposedIsolationInfo(),
          dest_url_info.web_exposed_isolation_info)) {
    return false;
  }

  if (candidate->GetSiteInstance()->GetSiteInfo().agent_cluster_key() &&
      candidate->GetSiteInstance()
              ->GetSiteInfo()
              .agent_cluster_key()
              ->GetCrossOriginIsolationKey() !=
          dest_url_info.cross_origin_isolation_key) {
    return false;
  }

  // Note: We are mixing the frame_tree_node_->IsOutermostMainFrame() status of
  // this object with the URL & origin of `candidate`. This is to determine if
  // `dest_url_info` would be considered "same site" if `candidate` occupied the
  // position of this object in the frame tree.
  return candidate->GetSiteInstance()->IsNavigationSameSite(
      candidate->last_successful_url(), candidate->GetLastCommittedOrigin(),
      frame_tree_node_->IsOutermostMainFrame(), dest_url_info);
}

void RenderFrameHostManager::CreateProxiesForNewRenderFrameHost(
    SiteInstanceGroup* old_group,
    SiteInstanceGroup* new_group,
    bool recovering_without_early_commit,
    const scoped_refptr<BrowsingContextState>& browsing_context_state) {
  // Only create opener proxies if they are in the same CoopRelatedGroup.
  if (new_group->IsCoopRelatedSiteInstanceGroup(old_group)) {
    CreateOpenerProxies(new_group, frame_tree_node_, browsing_context_state);
  } else {
    // Ensure that the frame tree has RenderFrameProxyHosts for the
    // new SiteInstanceGroup in all necessary nodes.  We do this for all frames
    // in the tree, whether they are in the same BrowsingInstance or not.  If
    // |new_group| is in the same BrowsingInstance as |old_group|, this
    // will be done as part of CreateOpenerProxies above; otherwise, we do this
    // here.  We will still check whether two frames are in the same
    // BrowsingInstance before we allow them to interact (e.g., postMessage).
    frame_tree_node_->frame_tree().CreateProxiesForSiteInstanceGroup(
        frame_tree_node_, new_group, browsing_context_state);
  }

  // When navigating same-site and recovering from a crash, create a proxy
  // in the new process. This will be swapped for a frame if we commit.
  // TODO(https://crbug.com/40052076): Consider handling this case in
  // FrameTree::CreateProxiesForSiteInstanceGroup.
  if (recovering_without_early_commit &&
      render_frame_host_->GetSiteInstance()->group() == new_group) {
    if (frame_tree_node_->IsMainFrame()) {
      frame_tree_node_->frame_tree()
          .GetRenderViewHost(new_group)
          ->SetMainFrameRoutingId(MSG_ROUTING_NONE);
    }

    // As there is an explicit check for |render_frame_host_|'s SiteInstance
    // being the same as the "new" RenderFrameHost,
    // |render_frame_host_->browsing_context_state()| is the right
    // BrowsingContextState to use.
    CreateRenderFrameProxy(new_group,
                           render_frame_host_->browsing_context_state());
  }
}

void RenderFrameHostManager::CreateProxiesForNewNamedFrame(
    const scoped_refptr<BrowsingContextState>& browsing_context_state) {
  DCHECK(!frame_tree_node_->frame_name().empty());

  // If this is a top-level frame, create proxies for this node in the
  // SiteInstanceGroups of its opener's ancestors, which are allowed to discover
  // this frame by name (see https://crbug.com/511474 and part 4 of
  // https://html.spec.whatwg.org/C/#the-rules-for-choosing-a-browsing-context-given-a-browsing-context-name
  // ).
  FrameTreeNode* opener = frame_tree_node_->opener();
  if (!opener || !frame_tree_node_->IsMainFrame())
    return;
  SiteInstanceGroup* current_group =
      render_frame_host_->GetSiteInstance()->group();

  // Return immediately if the opener and the openee are not in the same
  // BrowsingInstance. Named targeting should not resolve for frames in other
  // BrowsingInstances, even if they are in the same CoopRelatedGroup. In that
  // case we do not need proxies and do not want to expose more than what is
  // strictly required to the renderer.
  // TODO(crbug.com/40276662): this will likely need to change once we
  // implement a more robust approach to named targeting, using per-
  // BrowsingInstance names. In that case, we'll need to create proxies across
  // BrowsingInstances to support named targeting.
  if (!current_group->IsRelatedSiteInstanceGroup(
          opener->current_frame_host()->GetSiteInstance()->group())) {
    return;
  }

  // Start from opener's parent.  There's no need to create a proxy in the
  // opener's SiteInstance's group, since new windows are always first opened in
  // the same SiteInstanceGroup as their opener, and if the new window navigates
  // cross-site, that proxy would be created as part of unloading.
  for (RenderFrameHostImpl* ancestor = opener->parent(); ancestor;
       ancestor = ancestor->GetParent()) {
    if (ancestor->GetSiteInstance()->group() != current_group) {
      CreateRenderFrameProxy(ancestor->GetSiteInstance()->group(),
                             browsing_context_state);
    }
  }
}

std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::CreateRenderFrameHost(
    CreateFrameCase create_frame_case,
    SiteInstanceImpl* site_instance,
    int32_t frame_routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    const blink::LocalFrameToken& frame_token,
    const blink::DocumentToken& document_token,
    base::UnguessableToken devtools_frame_token,
    bool renderer_initiated_creation,
    scoped_refptr<BrowsingContextState> browsing_context_state) {
  FrameTree& frame_tree = frame_tree_node_->frame_tree();

  // Only the kInitChild case passes in a frame routing id.
  DCHECK_EQ(create_frame_case != CreateFrameCase::kInitChild,
            frame_routing_id == MSG_ROUTING_NONE);
  if (frame_routing_id == MSG_ROUTING_NONE) {
    frame_routing_id = site_instance->GetProcess()->GetNextRoutingID();
  }

  // Check to see if a speculative RenderViewHost is needed. It is needed for
  // cross-page same-SiteInstanceGroup navigations when the feature is enabled.
  // TODO(yangsharon, rakina, crbug.com/1336305): Handle the
  // cross-SiteInstanceGroup  and crashed frame cases.
  CreateRenderViewHostCase create_rvh_case =
      (render_frame_host_ &&
       render_frame_host_->ShouldChangeRenderFrameHostOnSameSiteNavigation() &&
       create_frame_case == CreateFrameCase::kCreateSpeculative &&
       static_cast<SiteInstanceImpl*>(site_instance)->group() ==
           render_frame_host_->GetSiteInstance()->group() &&
       frame_tree_node_->IsMainFrame() &&
       !render_frame_host_->must_be_replaced())
          ? CreateRenderViewHostCase::kSpeculative
          : CreateRenderViewHostCase::kDefault;

  scoped_refptr<RenderViewHostImpl> render_view_host = nullptr;
  std::optional<viz::FrameSinkId> frame_sink_id;
  // In the case a speculative RenderViewHost will be created, we don't need to
  // check if there's an existing RenderViewHost. Otherwise, get the appropriate
  // RenderViewHost.
  if (create_rvh_case == CreateRenderViewHostCase::kDefault) {
    render_view_host = frame_tree.GetRenderViewHost(site_instance->group());
  } else if (current_frame_host()->ShouldReuseCompositing(*site_instance)) {
    frame_sink_id =
        current_frame_host()->GetRenderWidgetHost()->GetFrameSinkId();
  }

  switch (create_frame_case) {
    case CreateFrameCase::kInitChild:
      DCHECK(!frame_tree_node_->IsMainFrame());
      // The first RenderFrameHost for a child FrameTreeNode is always in the
      // same SiteInstance as its parent.
      DCHECK_EQ(frame_tree_node_->parent()->GetSiteInstance(), site_instance);
      // The RenderViewHost must already exist for the parent's SiteInstance.
      DCHECK(render_view_host);
      // Only main frames can be marked as renderer-initiated, as it refers to
      // a renderer-created window.
      DCHECK(!renderer_initiated_creation);
      break;
    case CreateFrameCase::kInitRoot:
      DCHECK(frame_tree_node_->IsMainFrame());
      // The view should not already exist when we are initializing the frame
      // tree.
      DCHECK(!render_view_host);
      break;
    case CreateFrameCase::kCreateSpeculative:
      // We create speculative frames both for main frame and subframe
      // navigations. The view might exist already if the SiteInstance already
      // has frames hosted in the target process. So we don't check the view.
      //
      // A speculative frame should be replacing an existing frame.
      DCHECK(render_frame_host_);
      // Only the initial main frame can be marked as renderer-initiated, as it
      // refers to a renderer-created window. A speculative frame is always
      // created later by the browser.
      DCHECK(!renderer_initiated_creation);
      break;
  }

  if (!render_view_host) {
    render_view_host = frame_tree.CreateRenderViewHost(
        site_instance->group(), frame_routing_id, renderer_initiated_creation,
        features::GetBrowsingContextMode() ==
                features::BrowsingContextStateImplementationType::
                    kSwapForCrossBrowsingInstanceNavigations
            ? browsing_context_state
            : nullptr,
        create_rvh_case, frame_sink_id);
  }
  CHECK(render_view_host);

  // LifecycleStateImpl of newly created RenderFrameHost.
  LifecycleStateImpl lifecycle_state;

  if (create_frame_case == CreateFrameCase::kCreateSpeculative) {
    lifecycle_state = LifecycleStateImpl::kSpeculative;
  } else {
    // For the creation of initial documents:
    // - We create RenderFrameHost in kPrerendering state in case of
    // prerendering frame tree.
    // - We create RenderFrameHost in kActive state in all other cases.
    lifecycle_state = frame_tree.is_prerendering()
                          ? LifecycleStateImpl::kPrerendering
                          : LifecycleStateImpl::kActive;
  }

  return RenderFrameHostFactory::Create(
      site_instance, std::move(render_view_host),
      frame_tree.render_frame_delegate(), &frame_tree, frame_tree_node_,
      frame_routing_id, std::move(frame_remote), frame_token, document_token,
      devtools_frame_token, renderer_initiated_creation, lifecycle_state,
      std::move(browsing_context_state));
}

bool RenderFrameHostManager::CreateSpeculativeRenderFrameHost(
    SiteInstanceImpl* old_instance,
    SiteInstanceImpl* new_instance,
    bool recovering_without_early_commit) {
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.CreateSpeculativeRFH");
  CHECK(new_instance);
  // This DCHECK is going to be fully removed as part of RenderDocument [1].
  //
  // With RenderDocument for sub frames or main frames: cross-document
  // navigation creates a new RenderFrameHost. The navigation is potentially
  // same-SiteInstance.
  //
  // With RenderDocument for crashed frames: navigations from a crashed
  // RenderFrameHost creates a new RenderFrameHost. The navigation is
  // potentially same-SiteInstance.
  //
  // [1] http://crbug.com/936696
  DCHECK(old_instance != new_instance ||
         render_frame_host_->ShouldChangeRenderFrameHostOnSameSiteNavigation());

  // The process for the new SiteInstance may (if we're sharing a process with
  // another host that already initialized it) or may not (we have our own
  // process or the existing process crashed) have been initialized. Calling
  // Init() multiple times will be ignored, so this is safe.
  if (!new_instance->GetProcess()->Init())
    return false;

  scoped_refptr<BrowsingContextState> browsing_context_state;
  if (features::GetBrowsingContextMode() ==
      features::BrowsingContextStateImplementationType::
          kLegacyOneToOneWithFrameTreeNode) {
    browsing_context_state = render_frame_host_->browsing_context_state();
  } else {
    // For speculative frame hosts, we will need to create a new
    // BrowsingContextState when we have a cross-BrowsingInstance navigation,
    // as the browsing context + BrowsingInstance combination changes. An
    // exception is when the RenderViewHost for the speculative
    // RenderFrameHost's SiteInstance is still around, e.g. on history
    // navigations.
    // TODO(crbug.com/40169570): FrameReplicationState is a mix of things that
    // are per-frame, per-browsing context and per-document. Currently, we pass
    // the entire FrameReplicationState to match the old behaviour of storing
    // FrameReplicationState on FrameTreeNode. We should consider splitting
    // FrameReplicationState into multiple structs with different lifetimes.
    // TODO(crbug.com/40205442): conditionally avoid copying the frame name here
    // if DidChangeName arrives after DidCommitNavigation.
    if (render_frame_host_->GetSiteInstance()->IsRelatedSiteInstance(
            new_instance)) {
      // We're reusing the current BrowsingInstance, so also reuse the
      // BrowsingContextState.
      browsing_context_state = render_frame_host_->browsing_context_state();
    } else {
      // TODO(crbug.com/936696, rakina, yangsharon): Once RenderDocument is
      // implemented, there will never be an existing RenderViewHost, so getting
      // the RenderViewHost and checking if there's a value can be removed.
      scoped_refptr<RenderViewHostImpl> render_view_host =
          frame_tree_node_->frame_tree().GetRenderViewHost(
              new_instance->group());
      if (render_view_host) {
        // If we reuse a RenderViewHost for a main-frame cross-BrowsingInstance
        // navigation, we need to reuse the RenderFrameProxyHost representing
        // its main frame and BrowsingContextState associated with this proxy.
        // This is possible when we are performing a history navigation (which
        // reuses existing SiteInstance associated with the corresponding
        // FrameNavigationEntry) and there is a pending deletion RenderViewHost
        // associated with the same SiteInstance, and we are creating a new
        // BrowsingContextState. Both proxies and RenderViewHosts are keyed by
        // SiteInstance(Group), and we don't want to have two different proxies
        // in the same frame belonging to the same RenderViewHost due to these
        // proxies belonging to different BrowsingContextStates. Since
        // RenderViewHost is also keyed by SiteInstance, when there is an
        // existing RenderViewHost, we want to use the correct corresponding
        // proxy when unloading a frame and committing a navigation.
        // TODO(crbug.com/40216896): Migrate storage of SiteInstance(Group) =>
        // RenderViewHost to BrowsingContextState to eliminate this branch.
        browsing_context_state = scoped_refptr<BrowsingContextState>(
            &*(render_view_host->main_browsing_context_state().value()));
        CHECK(frame_tree_node_->IsMainFrame());
      } else {
        browsing_context_state = base::MakeRefCounted<BrowsingContextState>(
            render_frame_host_->browsing_context_state()
                ->current_replication_state()
                .Clone(),
            frame_tree_node_->parent(), new_instance->GetBrowsingInstanceId(),
            new_instance->coop_related_group_token());

        // Add a proxy to the outer delegate if one exists, as this is not
        // copied over to the new BrowsingContextState otherwise.
        FrameTreeNode* outer_contents_frame_tree_node = GetOuterDelegateNode();
        if (outer_contents_frame_tree_node) {
          DCHECK(outer_contents_frame_tree_node->parent());
          browsing_context_state->CreateOuterDelegateProxy(
              outer_contents_frame_tree_node->parent()
                  ->GetSiteInstance()
                  ->group(),
              frame_tree_node_, blink::RemoteFrameToken());
        }
      }
    }
  }

  CreateProxiesForNewRenderFrameHost(
      old_instance->group(), new_instance->group(),
      recovering_without_early_commit, browsing_context_state);

  speculative_render_frame_host_ = CreateSpeculativeRenderFrame(
      new_instance, recovering_without_early_commit, browsing_context_state);
  return !!speculative_render_frame_host_;
}

std::unique_ptr<RenderFrameHostImpl>
RenderFrameHostManager::CreateSpeculativeRenderFrame(
    SiteInstanceImpl* instance,
    bool recovering_without_early_commit,
    const scoped_refptr<BrowsingContextState>& browsing_context_state) {
  TRACE_EVENT("navigation",
              "RenderFrameHostManager::CreateSpeculativeRenderFrame",
              ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);

  CHECK(instance);
  // This DCHECK is going to be fully removed as part of RenderDocument [1].
  //
  // With RenderDocument for sub frames or main frames: cross-document
  // navigation creates a new RenderFrameHost. The navigation is potentially
  // same-SiteInstance.
  //
  // With RenderDocument for crashed frames: navigations from a crashed
  // RenderFrameHost creates a new RenderFrameHost. The navigation is
  // potentially same-SiteInstance.
  //
  // [1] http://crbug.com/936696
  DCHECK(render_frame_host_->GetSiteInstance() != instance ||
         render_frame_host_->ShouldChangeRenderFrameHostOnSameSiteNavigation());

  std::unique_ptr<RenderFrameHostImpl> new_render_frame_host =
      CreateRenderFrameHost(CreateFrameCase::kCreateSpeculative, instance,
                            /*frame_routing_id=*/MSG_ROUTING_NONE,
                            mojo::PendingAssociatedRemote<mojom::Frame>(),
                            blink::LocalFrameToken(), blink::DocumentToken(),
                            render_frame_host_->devtools_frame_token(),
                            /*renderer_initiated_creation=*/false,
                            browsing_context_state);
  DCHECK_EQ(new_render_frame_host->GetSiteInstance(), instance);

  // Prevent the process from exiting while we're trying to navigate in it.
  new_render_frame_host->GetProcess()->AddPendingView();

  RenderViewHostImpl* render_view_host =
      new_render_frame_host->render_view_host();
  if (frame_tree_node_->IsMainFrame()) {
    if (render_view_host == render_frame_host_->render_view_host()) {
      // We are replacing the main frame's host with |new_render_frame_host|.
      // RenderViewHost is reused after a crash and in order for InitRenderView
      // to find |new_render_frame_host| as the new main frame, we set the
      // routing ID now. This is safe to do as we will call CommitPending() in
      // GetFrameHostForNavigation() before yielding to other tasks.
      render_view_host->SetMainFrameRoutingId(
          new_render_frame_host->GetRoutingID());
    }

    SiteInstanceGroup* site_instance_group = instance->group();
    if (!InitRenderView(site_instance_group, render_view_host,
                        browsing_context_state->GetRenderFrameProxyHost(
                            site_instance_group))) {
      return nullptr;
    }

    // If we are reusing the RenderViewHost and it doesn't already have a
    // RenderWidgetHostView, we need to create one if this is the main frame.
    if (!render_view_host->GetWidget()->GetView()) {
      // TODO(crbug.com/40162510): The RenderWidgetHostView should be created
      // *before* we create the renderer-side objects through InitRenderView().
      // Then we should remove the null-check for the RenderWidgetHostView in
      // RenderWidgetHostImpl::RendererWidgetCreated().
      delegate_->CreateRenderWidgetHostViewForRenderManager(render_view_host);
      // If we are recovering a crashed frame in the same SiteInstanceGroup and
      // we are not skipping early commit then we will create a proxy and that
      // will prevent the regular outer delegate reattach path in
      // CreateRenderViewForRenderManager() from working.
      if (recovering_without_early_commit &&
          render_frame_host_->GetSiteInstance()->group() == instance->group()) {
        delegate_->ReattachOuterDelegateIfNeeded();
      }
    }
    // And since we are reusing the RenderViewHost make sure it is hidden, like
    // a new RenderViewHost would be, until navigation commits.
    render_view_host->GetWidget()->GetView()->Hide();
  }

  DCHECK(render_view_host->IsRenderViewLive());
  // RenderViewHost for |instance| might exist prior to calling
  // CreateRenderFrame. In such a case, InitRenderView will not create the
  // RenderFrame in the renderer process and it needs to be done
  // explicitly.
  if (!InitRenderFrame(new_render_frame_host.get()))
    return nullptr;

  return new_render_frame_host;
}

void RenderFrameHostManager::CreateRenderFrameProxy(
    SiteInstanceGroup* group,
    const scoped_refptr<BrowsingContextState>& browsing_context_state,
    BatchedProxyIPCSender* batched_proxy_ipc_sender) {
  CHECK(group);
  TRACE_EVENT_INSTANT("navigation.debug",
                      "RenderFrameHostManager::CreateRenderFrameProxy",
                      ChromeTrackEvent::kSiteInstanceGroup, *group,
                      ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);
  // If we are creating a proxy to recover from a crash and skipping the early
  // CommitPending then it could be in the same SiteInstanceGroup. In all other
  // cases we should be creating it in a different one.
  if (ShouldSkipEarlyCommitPendingForCrashedFrame()) {
    // TODO(fergal): We cannot put a CHECK in the else of this if because we do
    // not have enough information about who is calling this. If we knew it was
    // navigating then we could CHECK_EQ and CHECK_NE otherwise.
    if (!render_frame_host_->must_be_replaced())
      CHECK_NE(group, render_frame_host_->GetSiteInstance()->group());
  } else {
    // If policy allows early commit, a RenderFrameProxyHost should never be
    // created in the same SiteInstanceGroup as the current RFH.
    CHECK_NE(group, render_frame_host_->GetSiteInstance()->group());
  }

  // If a proxy already exists and is alive, nothing needs to be done.
  RenderFrameProxyHost* proxy =
      browsing_context_state->GetRenderFrameProxyHost(group);
  if (proxy && proxy->is_render_frame_proxy_live())
    return;

  // At this point we know that we either have to 1) create a new
  // RenderFrameProxyHost or 2) revive an existing, but no longer alive
  // RenderFrameProxyHost.
  if (!proxy) {
    // The RenderViewHost creates the page level structure in Blink. The first
    // object to depend on it is necessarily a main frame one.
    scoped_refptr<RenderViewHostImpl> render_view_host =
        frame_tree_node_->frame_tree().GetRenderViewHost(group);
    if (!frame_tree_node_->IsMainFrame()) {
      SCOPED_CRASH_KEY_BOOL("Bug1400009", "sig_exists", !!group);
      SCOPED_CRASH_KEY_STRING256("Bug1400009", "current_rfh_url",
                                 render_frame_host_->GetLastCommittedURL()
                                     .GetWithEmptyPath()
                                     .possibly_invalid_spec());
      SCOPED_CRASH_KEY_NUMBER("Bug1400009", "target_sig", (int)group->GetId());
      SCOPED_CRASH_KEY_NUMBER(
          "Bug1400009", "current_rfh_si",
          (int)render_frame_host_->GetSiteInstance()->GetId());
      SCOPED_CRASH_KEY_STRING64("Bug1400009", "current_lifecycle",
                                RenderFrameHostImpl::LifecycleStateImplToString(
                                    render_frame_host_->lifecycle_state()));
      RenderFrameHostImpl* parent_rfh = render_frame_host_->GetParent();
      SCOPED_CRASH_KEY_NUMBER("Bug1400009", "parent_si",
                              (int)parent_rfh->GetSiteInstance()->GetId());
      SCOPED_CRASH_KEY_BOOL("Bug1400009", "parent_rvh_exists",
                            !!frame_tree_node_->frame_tree().GetRenderViewHost(
                                parent_rfh->GetSiteInstance()->group()));
      SCOPED_CRASH_KEY_STRING64("Bug1400009", "parent_lifecycle",
                                RenderFrameHostImpl::LifecycleStateImplToString(
                                    parent_rfh->lifecycle_state()));
      CHECK(render_view_host);
    }
    if (!render_view_host) {
      // Before creating a new RenderFrameProxyHost, ensure a RenderViewHost
      // exists for |group|, as it creates the page level structure in Blink.
      render_view_host = frame_tree_node_->frame_tree().CreateRenderViewHost(
          group, /*main_frame_routing_id=*/MSG_ROUTING_NONE,
          /*renderer_initiated_creation=*/false,
          features::GetBrowsingContextMode() ==
                  features::BrowsingContextStateImplementationType::
                      kSwapForCrossBrowsingInstanceNavigations
              ? render_frame_host_->browsing_context_state()
              : nullptr,
          CreateRenderViewHostCase::kDefault, std::nullopt);
    } else {
      TRACE_EVENT_INSTANT("navigation",
                          "RenderFrameHostManager::CreateRenderFrameProxy_RVH",
                          ChromeTrackEvent::kRenderViewHost, *render_view_host);
    }

    proxy = browsing_context_state->CreateRenderFrameProxyHost(
        group, std::move(render_view_host), frame_tree_node_);
  }

  // Make sure that the `blink::RemoteFrame` is present in the renderer.
  if (frame_tree_node_->IsMainFrame() && proxy->GetRenderViewHost()) {
    InitRenderView(group, proxy->GetRenderViewHost(), proxy);
  } else {
    proxy->InitRenderFrameProxy(batched_proxy_ipc_sender);
  }
}

void RenderFrameHostManager::CreateRenderFrameProxyAndAncestorChainIfNeeded(
    SiteInstanceGroup* group) {
  SiteInstanceGroup* current_site_instance_group =
      current_frame_host()->GetSiteInstance()->group();
  CHECK(!group->IsRelatedSiteInstanceGroup(current_site_instance_group));
  CHECK(group->IsCoopRelatedSiteInstanceGroup(current_site_instance_group));

  // If the frame we need to create a proxy for is a subframe, we need to make
  // sure the entire ancestor chain exists as proxies as well, otherwise the
  // subframe proxy would be floating around. Note: we only need to create
  // ancestors in this frame tree, so we can use IsMainFrame().
  std::vector<FrameTreeNode*> ancestor_chain;
  FrameTreeNode* ancestor = frame_tree_node_;
  while (ancestor) {
    ancestor_chain.push_back(ancestor);
    if (ancestor->IsMainFrame()) {
      ancestor = nullptr;
    } else {
      ancestor = ancestor->parent()->frame_tree_node();
    }
  }

  // Create proxies, from the top-level frame down to the initially specified
  // subframe. TODO(crbug.com/40186710): Verify that the behavior is
  // correct if the frame is pending deletion.
  for (FrameTreeNode* node : base::Reversed(ancestor_chain)) {
    node->render_manager()->CreateRenderFrameProxy(
        group, node->current_frame_host()->browsing_context_state());
  }
}

void RenderFrameHostManager::CreateProxiesForChildFrame(FrameTreeNode* child) {
  TRACE_EVENT_INSTANT(
      "navigation", "RenderFrameHostManager::CreateProxiesForChildFrame_Parent",
      ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);
  TRACE_EVENT_INSTANT(
      "navigation", "RenderFrameHostManager::CreateProxiesForChildFrame_Child",
      ChromeTrackEvent::kFrameTreeNodeInfo, *child);
  RenderFrameProxyHost* outer_delegate_proxy =
      IsMainFrameForInnerDelegate() ? GetProxyToOuterDelegate() : nullptr;

  // Initial document in the child frame always belongs to the same SiteInstance
  // as its parent document, so we iterate over the proxies in the parent frame
  // to get a list of SiteInstances to create proxies in for in the child frame.
  DCHECK_EQ(render_frame_host_.get(), child->parent());

  for (const auto& pair :
       render_frame_host_->browsing_context_state()->proxy_hosts()) {
    TRACE_EVENT_INSTANT(
        "navigation",
        "RenderFrameHostManager::CreateProxiesForChildFrame_ProxyHost",
        ChromeTrackEvent::kRenderFrameProxyHost, *pair.second);
    // Do not create proxies for subframes in the outer delegate's process,
    // since the outer delegate does not need to interact with them.
    //
    // TODO(alexmos): This is potentially redundant with the
    // IsRelatedSiteInstanceGroup() check below.  Verify this and remove if so.
    if (pair.second.get() == outer_delegate_proxy)
      continue;

    // Do not create proxies for subframes for SiteInstances belonging to a
    // different BrowsingInstance. This may happen in several cases:
    // - When creating a frame in a BrowsingInstance that is in the same
    //   CoopRelatedGroup as another BrowsingInstance. In that case, other
    //   BrowsingInstances should not know about this frame until they
    //   absolutely need to.
    // - When a main frame is navigating across BrowsingInstances, and the
    //   current document adds a subframe after that navigation starts but
    //   before it commits.  In that time window, the main frame's FrameTreeNode
    //   would have a proxy in the destination SiteInstance, but the current
    //   document's subframes shouldn't create a proxy in the destination
    //   SiteInstance, since the new BrowsingInstance need not know about them.
    //   Not doing this used to trigger inconsistencies and crashes if the old
    //   document was stored in BackForwardCache and later restored (since this
    //   preserves all of the subframe FrameTreeNodes and proxies).  See
    //   https://crbug.com/1243541.
    if (!pair.second->site_instance_group()->IsRelatedSiteInstanceGroup(
            render_frame_host_->GetSiteInstance()->group())) {
      continue;
    }

    child->render_manager()->CreateRenderFrameProxy(
        pair.second->site_instance_group(),
        child->current_frame_host()->browsing_context_state());
  }
}

void RenderFrameHostManager::EnsureRenderViewInitialized(
    RenderViewHostImpl* render_view_host,
    SiteInstanceGroup* group) {
  DCHECK(frame_tree_node_->IsMainFrame());

  if (render_view_host->IsRenderViewLive())
    return;

  // If the proxy in `group` doesn't exist, this `blink::WebView` is not
  // swapped out and shouldn't be reinitialized here.
  RenderFrameProxyHost* proxy =
      render_frame_host_->browsing_context_state()->GetRenderFrameProxyHost(
          group);
  if (!proxy)
    return;

  InitRenderView(group, render_view_host, proxy);
}

void RenderFrameHostManager::SwapOuterDelegateFrame(
    RenderFrameHostImpl* render_frame_host,
    RenderFrameProxyHost* proxy) {
  // Swap the outer WebContents's frame with the proxy to inner WebContents.
  //
  // We are in the outer WebContents, and its FrameTree would never see
  // a load start for any of its inner WebContents. Eventually, that also makes
  // the FrameTree never see the matching load stop. Therefore, we always pass
  // false to |is_loading| below.
  // TODO(lazyboy): This |is_loading| behavior might not be what we want,
  // investigate and fix.
  DCHECK_EQ(render_frame_host->GetSiteInstance()->group(),
            proxy->site_instance_group());
  render_frame_host->SwapOuterDelegateFrame(proxy);
  proxy->SetRenderFrameProxyCreated(true);
}

void RenderFrameHostManager::SetRWHViewForInnerFrameTree(
    RenderWidgetHostViewChildFrame* child_rwhv) {
  DCHECK(IsMainFrameForInnerDelegate());
  DCHECK(GetProxyToOuterDelegate());
  GetProxyToOuterDelegate()->SetChildRWHView(child_rwhv, nullptr,
                                             /*allow_paint_holding=*/false);
}

bool RenderFrameHostManager::InitRenderView(
    SiteInstanceGroup* site_instance_group,
    RenderViewHostImpl* render_view_host,
    RenderFrameProxyHost* proxy) {
  // Ensure the renderer process is initialized before creating the
  // `blink::WebView`.
  if (!render_view_host->GetAgentSchedulingGroup().Init())
    return false;

  // We may have initialized this RenderViewHost for another RenderFrameHost.
  if (render_view_host->IsRenderViewLive())
    return true;

  auto opener_frame_token = GetOpenerFrameToken(site_instance_group);

  bool created = delegate_->CreateRenderViewForRenderManager(
      render_view_host, opener_frame_token, proxy);

  if (created && proxy) {
    proxy->SetRenderFrameProxyCreated(true);

    // If this main frame proxy was created for a frame that hasn't yet
    // finished loading, let the renderer know so it can also mark the proxy as
    // loading. See https://crbug.com/916137.
    if (frame_tree_node_->IsLoading())
      proxy->GetAssociatedRemoteFrame()->DidStartLoading();
  }

  return created;
}

scoped_refptr<SiteInstanceImpl>
RenderFrameHostManager::GetSiteInstanceForNavigationRequest(
    NavigationRequest* request,
    BrowsingContextGroupSwap* browsing_context_group_swap,
    std::string* reason) {
  IsSameSiteGetter is_same_site = IsSameSiteGetter();
  return GetSiteInstanceForNavigationRequest(
      request, is_same_site, browsing_context_group_swap, reason);
}

scoped_refptr<SiteInstanceImpl>
RenderFrameHostManager::GetSiteInstanceForNavigationRequest(
    NavigationRequest* request,
    IsSameSiteGetter& is_same_site,
    BrowsingContextGroupSwap* browsing_context_group_swap,
    std::string* reason) {
  SiteInstanceImpl* current_site_instance =
      render_frame_host_->GetSiteInstance();

  // All children of MHTML documents must be MHTML documents. They all live in
  // the same process.
  if (request->IsForMhtmlSubframe()) {
    AppendReason(reason,
                 "GetSiteInstanceForNavigationRequest => current_site_instance"
                 " (IsForMhtmlSubframe)");
    return base::WrapRefCounted(current_site_instance);
  }

  // Srcdoc documents are only in the same SiteInstance as their parent if they
  // both have the same value for is_sandboxed(). They load their content from
  // the "srcdoc" iframe attribute which lives in the parent's process. Using
  // `GetParent()` is correct here because we never share BrowsingInstance /
  // SiteInstance across inner and outer frame tree.
  RenderFrameHostImpl* parent = render_frame_host_->GetParent();
  if (parent && request->common_params().url.IsAboutSrcdoc()) {
    const UrlInfo& url_info = request->GetUrlInfo();
    if (url_info.is_sandboxed &&
        !parent->GetSiteInstance()->GetSiteInfo().is_sandboxed()) {
      // TODO(wjmaclean); For now, SiteInfo::is_sandboxed() and
      // UrlInfo::is_sandboxed both mean "origin-restricted sandbox", so this
      // simple comparison suffices. But when we extend sandbox isolation to
      // depend on other sandbox flags as well, we may want to do a more
      // detailed comparison to make sure everything is compatible. E.g. if both
      // the parent and child are sandboxed, but with different flags, then we
      // may need separate SiteInstances, but that will be left for future CL.
      AppendReason(reason,
                   "GetSiteInstanceForNavigationRequest => compatible "
                   "sandboxed instance (IsAboutSrcdoc)");
      // In all the non-srcdoc cases we have a value for src and hence a UrlInfo
      // from which to build a SiteInfo for the sandboxed frame. But in the case
      // of a srcdoc iframe, we're basically picking a SiteInstance that is the
      // same as the parent frame, but with the `is_sandbox` flag set. srcdoc
      // iframes are normally considered to have the same origin as their
      // parents, so this seems reasonable.
      return parent->GetSiteInstance()->GetCompatibleSandboxedSiteInstance(
          url_info, parent->GetLastCommittedOrigin());
    }
    AppendReason(reason,
                 "GetSiteInstanceForNavigationRequest => parent-instance"
                 " (IsAboutSrcdoc)");
    return base::WrapRefCounted(parent->GetSiteInstance());
  }

  // Compute the SiteInstance that the navigation should use, which will be
  // either the current SiteInstance or a new one.
  //
  // TODO(clamy): We should also consider as a candidate SiteInstance the
  // speculative SiteInstance that was computed on redirects.
  SiteInstanceImpl* candidate_site_instance =
      speculative_render_frame_host_
          ? speculative_render_frame_host_->GetSiteInstance()
          : nullptr;

  // Accounts for all types of reloads, including renderer-initiated reloads.
  bool is_reload =
      NavigationTypeUtils::IsReload(request->common_params().navigation_type);

  scoped_refptr<SiteInstanceImpl> dest_site_instance =
      GetSiteInstanceForNavigation(
          request->GetUrlInfo(), request->GetSourceSiteInstance(),
          request->dest_site_instance(), candidate_site_instance,
          ui::PageTransitionFromInt(request->common_params().transition),
          request->ComputeErrorPageProcess(), is_reload,
          request->IsSameDocument(), is_same_site,
          request->commit_params().is_view_source, request->WasServerRedirect(),
          request->coop_status().browsing_instance_swap_result(),
          request->common_params().should_replace_current_entry,
          request->force_new_browsing_instance(),
          request->begin_params().has_rel_opener, browsing_context_group_swap,
          reason);

  // If the NavigationRequest's dest_site_instance was present but incorrect,
  // then ensure no sensitive state is kept on the request. This can happen for
  // cross-process redirects, error pages, etc.
  if (request->dest_site_instance() &&
      request->dest_site_instance() != dest_site_instance) {
    request->ResetStateForSiteInstanceChange();
  }

  return dest_site_instance;
}

bool RenderFrameHostManager::InitRenderFrame(
    RenderFrameHostImpl* render_frame_host) {
  if (render_frame_host->IsRenderFrameLive()) {
    return true;
  }

  SiteInstanceGroup* site_instance_group =
      render_frame_host->GetSiteInstance()->group();

  std::optional<blink::FrameToken> opener_frame_token;
  if (frame_tree_node_->opener())
    opener_frame_token = GetOpenerFrameToken(site_instance_group);

  std::optional<blink::FrameToken> parent_frame_token;
  if (frame_tree_node_->parent()) {
    parent_frame_token =
        frame_tree_node_->parent()
            ->frame_tree_node()
            ->render_manager()
            ->GetFrameTokenForSiteInstanceGroup(site_instance_group);
    CHECK(parent_frame_token);
  }

  // At this point, all RenderFrameProxies for sibling frames have already been
  // created, including any proxies that come after this frame.  To preserve
  // correct order for indexed window access (e.g., window.frames[1]), pass the
  // previous sibling frame so that this frame is correctly inserted into the
  // frame tree on the renderer side.
  std::optional<blink::FrameToken> previous_sibling_frame_token;
  FrameTreeNode* previous_sibling =
      frame_tree_node_->current_frame_host()->PreviousSibling();
  if (previous_sibling) {
    previous_sibling_frame_token =
        previous_sibling->render_manager()->GetFrameTokenForSiteInstanceGroup(
            site_instance_group);
    CHECK(previous_sibling_frame_token);
  }

  RenderFrameProxyHost* existing_proxy =
      render_frame_host->browsing_context_state()->GetRenderFrameProxyHost(
          site_instance_group);
  if (existing_proxy && !existing_proxy->is_render_frame_proxy_live())
    existing_proxy->InitRenderFrameProxy();

  // Figure out the FrameToken of the frame or proxy that this frame will
  // replace. This usually will be `existing_proxy`'s FrameToken, but
  // with RenderDocument it might also be a RenderFrameHost's FrameToken.
  std::optional<blink::FrameToken> previous_frame_token =
      GetReplacementFrameToken(existing_proxy, render_frame_host);

  return render_frame_host->CreateRenderFrame(
      previous_frame_token, opener_frame_token, parent_frame_token,
      previous_sibling_frame_token);
}

std::optional<blink::FrameToken>
RenderFrameHostManager::GetReplacementFrameToken(
    RenderFrameProxyHost* existing_proxy,
    RenderFrameHostImpl* render_frame_host) const {
  // Check whether there is an existing proxy for this frame in this
  // SiteInstance. If there is, the new RenderFrame needs to be able to find
  // the proxy it is replacing, so that it can fully initialize itself.
  // NOTE: This is the only time that a RenderFrameProxyHost can be in the same
  // SiteInstance as its RenderFrameHost. This is only the case until the
  // RenderFrameHost commits, at which point it will replace and delete the
  // RenderFrameProxyHost.
  if (existing_proxy) {
    // We are navigating cross-SiteInstance in a main frame or subframe.
    return existing_proxy->GetFrameToken();
  } else {
    // No proxy means that this is one of:
    // - a same-SiteInstance subframe navigation
    // - a cross-SiteInstance navigation from a crashed subframe that will do an
    //   early commit and the SiteInstance is not already in the frame tree.
    // A main frame navigation with no proxy would have its RenderFrame init
    // handled by InitRenderView. This will change with RenderDocument for main
    // frames.
    DCHECK(frame_tree_node_->parent());
    if (current_frame_host()->IsRenderFrameLive()) {
      CHECK_EQ(render_frame_host->GetSiteInstance(),
               current_frame_host()->GetSiteInstance());
      // The new frame will replace an existing frame in the renderer. For now
      // this can only be when RenderDocument-subframe is enabled.
      DCHECK(render_frame_host_
                 ->ShouldChangeRenderFrameHostOnSameSiteNavigation());
      DCHECK_NE(render_frame_host, current_frame_host());
      return current_frame_host()->GetFrameToken();
    } else {
      // The renderer crashed and there is no previous proxy or previous frame
      // in the renderer to be replaced.
      DCHECK(current_frame_host()->must_be_replaced());
      DCHECK_NE(render_frame_host, current_frame_host());
      return std::nullopt;
    }
  }
}

bool RenderFrameHostManager::ReinitializeMainRenderFrame(
    RenderFrameHostImpl* render_frame_host) {
  CHECK(!frame_tree_node_->parent());

  // This should be used only when the RenderFrame is not live.
  DCHECK(!render_frame_host->IsRenderFrameLive());
  DCHECK(!render_frame_host->must_be_replaced());

  // Recreate the opener chain.
  CreateOpenerProxies(render_frame_host->GetSiteInstance()->group(),
                      frame_tree_node_,
                      render_frame_host_->browsing_context_state());

  // Main frames need both the `blink::WebView` and `RenderFrame` reinitialized,
  // so use `InitRenderView`.
  DCHECK(!render_frame_host->browsing_context_state()->GetRenderFrameProxyHost(
      render_frame_host->GetSiteInstance()->group()));
  if (!InitRenderView(render_frame_host->GetSiteInstance()->group(),
                      render_frame_host->render_view_host(), nullptr))
    return false;

  DCHECK(render_frame_host->IsRenderFrameLive());

  // The RenderWidgetHostView goes away with the render process. Initializing a
  // RenderFrame means we'll be creating (or reusing, https://crbug.com/419087)
  // a RenderWidgetHostView. The new RenderWidgetHostView should take its
  // visibility from the RenderWidgetHostImpl, but this call exists to handle
  // cases where it did not during a same-process navigation.
  // TODO(danakj): We now hide the widget unconditionally (treating main frame
  // and child frames alike) and show in DidFinishNavigation() always, so this
  // should be able to go away. Try to remove this.
  if (render_frame_host == render_frame_host_.get())
    EnsureRenderFrameHostVisibilityConsistent();

  return true;
}

int RenderFrameHostManager::GetRoutingIdForSiteInstanceGroup(
    SiteInstanceGroup* site_instance_group) {
  if (render_frame_host_->GetSiteInstance()->group() == site_instance_group)
    return render_frame_host_->GetRoutingID();

  RenderFrameProxyHost* proxy =
      render_frame_host_->browsing_context_state()->GetRenderFrameProxyHost(
          site_instance_group);
  if (proxy)
    return proxy->GetRoutingID();

  return MSG_ROUTING_NONE;
}

std::optional<blink::FrameToken>
RenderFrameHostManager::GetFrameTokenForSiteInstanceGroup(
    SiteInstanceGroup* site_instance_group) {
  // We want to ensure that we don't create proxies for the new speculative site
  // instance after a browsing instance swap, and we want to ensure that this
  // doesn't break anything, so we tie it to the GetBrowsingContextMode which
  // needs it and is disabled-by-default)
  if (features::GetBrowsingContextMode() ==
          features::BrowsingContextStateImplementationType::
              kSwapForCrossBrowsingInstanceNavigations &&
      !render_frame_host_->GetSiteInstance()
           ->group()
           ->IsRelatedSiteInstanceGroup(site_instance_group)) {
    return std::nullopt;
  }
  if (render_frame_host_->GetSiteInstance()->group() == site_instance_group)
    return render_frame_host_->GetFrameToken();

  RenderFrameProxyHost* proxy =
      render_frame_host_->browsing_context_state()->GetRenderFrameProxyHost(
          site_instance_group);
  if (proxy)
    return proxy->GetFrameToken();

  return std::nullopt;
}

void RenderFrameHostManager::CommitPending(
    std::unique_ptr<RenderFrameHostImpl> pending_rfh,
    std::unique_ptr<StoredPage> pending_stored_page,
    bool clear_proxies_on_commit,
    bool allow_paint_holding) {
  TRACE_EVENT1("navigation", "RenderFrameHostManager::CommitPending",
               "FrameTreeNode id", frame_tree_node_->frame_tree_node_id());
  CHECK(pending_rfh);
  // We either come here with a `pending_rfh` that is
  // 1) a speculative RenderFrameHost, which would have been deleted
  //    immediately upon renderer process exit, so it must still have a live
  //    connection to its renderer frame.
  // 2) a current RenderFrameHost which has just received a commit IPC from the
  //    renderer, so it must have a live connection to its renderer frame in
  //    order to receive the IPC.
  DCHECK(pending_rfh->IsRenderFrameLive());
  if (RenderWidgetHostImpl* rwh = pending_rfh->GetLocalRenderWidgetHost()) {
    if (rwh->compositor_metric_recorder()) {
      if (pending_rfh->lifecycle_state() == LifecycleStateImpl::kSpeculative ||
          pending_rfh->lifecycle_state() ==
              LifecycleStateImpl::kPendingCommit) {
        // The navigation swaps in a new RenderFrameHost with a new
        // RenderWidgetHost. Log the time when the RFH swap happens to record
        // compositor-related metrics.
        rwh->compositor_metric_recorder()->DidSwap();
      } else {
        // We're restoring a BFCached RenderFrameHost. Make sure that it won't
        // record compositor-related metrics, since it's intended to be recorded
        // only for navigations with a new RenderFrameHost. Note that this can't
        // be a prerendered RFH because we don't create recorders for
        // prerendered pages.
        CHECK_EQ(pending_rfh->lifecycle_state(),
                 LifecycleStateImpl::kInBackForwardCache);
        rwh->DisableCompositorMetricRecording();
      }
    }
  }
#if BUILDFLAG(IS_MAC)
  // The old RenderWidgetHostView will be hidden before the new
  // RenderWidgetHostView takes its contents. Ensure that Cocoa sees this as
  // a single transaction.
  // https://crbug.com/829523
  // TODO(ccameron): This can be removed when the RenderWidgetHostViewMac uses
  // the same ui::Compositor as MacViews.
  // https://crbug.com/331669
  gfx::ScopedCocoaDisableScreenUpdates disabler;
#endif  // BUILDFLAG(IS_MAC)

  RenderWidgetHostView* old_view = render_frame_host_->GetView();
  bool is_main_frame = frame_tree_node_->IsMainFrame();

  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view =
      old_view && old_view->HasFocus() &&
      render_frame_host_->GetMainFrame()->GetRenderWidgetHost()->is_focused();

  // Remove the current frame and its descendants from the set of fullscreen
  // frames immediately. They can stay in pending deletion for some time.
  // Removing them when they are deleted is too late.
  // This needs to be done before updating the frame tree structure, else it
  // will have trouble removing the descendants.
  frame_tree_node_->frame_tree()
      .render_frame_delegate()
      ->FullscreenStateChanged(current_frame_host(), false,
                               blink::mojom::FullscreenOptionsPtr());

  // If the removed frame was created by a script, then its history entry will
  // never be reused - we can save some memory by removing the history entry.
  // See also https://crbug.com/784356.
  // This is done in ~FrameTreeNode, but this is needed here as well. For
  // instance if the user navigates from A(B) to C and B is deleted after C
  // commits, then the last committed navigation entry wouldn't match anymore.
  NavigationEntryImpl* navigation_entry =
      GetNavigationController().GetLastCommittedEntry();
  if (navigation_entry) {
    frame_tree_node_->PruneChildFrameNavigationEntries(navigation_entry);
  }

  // If we navigate to an existing page (i.e. |pending_stored_page| is not
  // null), check that |pending_rfh|'s old lifecycle state supports that.
  RenderFrameHostImpl::LifecycleStateImpl prev_state =
      pending_rfh->lifecycle_state();
  DCHECK(!pending_stored_page ||
         prev_state == RenderFrameHostImpl::LifecycleStateImpl::kPrerendering ||
         prev_state ==
             RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);

  // Now close any modal dialogs that would prevent us from unloading the old
  // frame. This must be done separately from RenderFrameHost::Unload(), so that
  // the ScopedPageLoadDeferrer is no longer on the stack when we send the
  // mojo::FrameNavigationControl::Unload message. Note that this is
  // intentionally done before updating the RenderFrameHost below, as this may
  // trigger far-reaching code that updates UI in the embedder, which could end
  // up looking up properties of the current RenderFrameHost, and those
  // properties won't be fully initialized for `pending_rfh` until later, after
  // UnloadOldFrame(). See https://crbug.com/346386726.
  //
  // Prerendering pages cannot create modal dialogs, so unloading a prerendering
  // RFH should not cause existing dialogs to close. (Subtle: `pending_rfh` is
  // still in pending-commit state at this point, and its lifecycle would only
  // be updated to kPrerendering as part of SetRenderFrameHost() further below,
  // so the check for prerendering is done via the frame tree instead.) To
  // prevent the cancellation from being used as a channel from fenced frames to
  // the primary main frame, also don't cancel modal dialogs for fenced frame
  // navigations.
  //
  // TODO(crbug.com/40791259): Update CancelModalDialogsForRenderManager to take
  // a RFH/RPH and only clear relevant dialogs instead of all dialogs in the
  // WebContents.
  if (!frame_tree_node_->frame_tree().is_prerendering() &&
      !pending_rfh->IsNestedWithinFencedFrame()) {
    delegate_->CancelModalDialogsForRenderManager();
  }

  // Swap in the new frame and make it active. Also ensure the FrameTree
  // stays in sync.
  std::unique_ptr<RenderFrameHostImpl> old_render_frame_host;
  old_render_frame_host = SetRenderFrameHost(std::move(pending_rfh));

  // If a document is being restored from the BackForwardCache or is being
  // activated from Prerendering, restore all cached state now.
  if (pending_stored_page) {
    pending_stored_page->PrepareToRestore();

    // This is only implemented for the legacy mode of BrowsingContextState
    // because in the new implementation, proxies will be swapped/restored
    // whenever the RenderFrameHost (and internal BrowsingContextState) is
    // restored.
    if (features::GetBrowsingContextMode() ==
        features::BrowsingContextStateImplementationType::
            kLegacyOneToOneWithFrameTreeNode) {
      BrowsingContextState::RenderFrameProxyHostMap proxy_hosts_to_restore =
          pending_stored_page->TakeProxyHosts();
      for (auto& proxy : proxy_hosts_to_restore) {
        // We only cache pages when swapping BrowsingInstance, so we should
        // never be reusing SiteInstanceGroups.
        CHECK(!base::Contains(
            render_frame_host_->browsing_context_state()->proxy_hosts(),
            proxy.second->site_instance_group()->GetId()));
        proxy.second->site_instance_group()->AddObserver(
            render_frame_host_->browsing_context_state().get());
        TRACE_EVENT_INSTANT(
            "navigation", "RenderFrameHostManager::CommitPending_RestoreProxy",
            ChromeTrackEvent::kRenderFrameProxyHost, *proxy.second);
        render_frame_host_->browsing_context_state()->proxy_hosts().insert(
            std::move(proxy));
      }
    }

    StoredPage::RenderViewHostImplSafeRefSet render_view_hosts_to_restore =
        pending_stored_page->TakeRenderViewHosts();
    if (prev_state ==
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache) {
      for (const auto& rvh : render_view_hosts_to_restore) {
        CHECK_NE(&*rvh, old_render_frame_host->GetRenderViewHost());
        blink::mojom::PageRestoreParamsPtr page_restore_params =
            pending_stored_page->page_restore_params().Clone();
        // We only send view_transition_state to the main RenderViewHost.
        if (&*rvh == current_frame_host()->GetRenderViewHost()) {
          page_restore_params->view_transition_state =
              pending_stored_page->TakeViewTransitionState();
          if (page_restore_params->view_transition_state.has_value()) {
            PrepareViewTransitionForBFCacheActivation(current_frame_host());
          }
        }
        rvh->LeaveBackForwardCache(std::move(page_restore_params));
      }
    } else {
      DCHECK_EQ(prev_state,
                RenderFrameHostImpl::LifecycleStateImpl::kPrerendering);
      current_frame_host()->GetPage().Activate(
          PageImpl::ActivationType::kPrerendering, render_view_hosts_to_restore,
          pending_stored_page->TakeViewTransitionState(), base::DoNothing());
    }
  }

  // For all main frames, the RenderWidgetHost will not be destroyed when the
  // local frame is detached. https://crbug.com/419087
  //
  // The blink::WidgetBase in the renderer process has its lifetime connected to
  // a RenderWdigetHost, which is owned by a RenderFrameHost. While the host is
  // eligible for BFCache it will remain alive. The eligibility is decided in
  // UnloadOldFrame. If not eligible then the host will be added to
  // `pending_delete_host_` to be destroyed.
  //
  // The blink::WebFrameWidget is destroyed when the blink::WebLocalFrame goes
  // away.
  //
  // The RenderWidgetHost and RenderWidgetHostView are still kept alive, paired
  // to the blink::WidgetBase and blink::FrameWidget.
  //
  // We hide the browser side here, which will have side-effects from notifying
  // listeners. This will also have the side effect of hiding the
  // blink::WidgetBase, which is desired so that frame production stops, and we
  // can reclaim memory when we eventually evict it.
  //
  // Note the RenderWidgetHostView can be missing if the process for the old
  // RenderFrameHost crashed.
  //
  // We also hide all subframes that are a local root. As while in BFCache they
  // are not detached nor destroyed. This prevents them from continuing frame
  // production, and allows for memory to be reclaimed when they are evicted.
  //
  // TODO(crbug.com/40387047): This call to Hide() can go away when the main
  // frame's RenderWidgetHost is destroyed on frame detach. Note that calling
  // this on a subframe that is not a local root would be incorrect as it would
  // hide an ancestor local root's RenderWidget when that frame is not
  // necessarily navigating. Removing this Hide() has previously been attempted
  // without success in r426913 (https://crbug.com/658688) and r438516
  // (broke assumptions about RenderWidgetHosts not changing
  // RenderWidgetHostViews over time).
  //
  // |old_rvh| and |new_rvh| can be the same when navigating same-site from a
  // crashed RenderFrameHost. When RenderDocument will be implemented, this will
  // happen for each same-site navigation.
  RenderViewHostImpl* old_rvh = old_render_frame_host->render_view_host();
  RenderViewHostImpl* new_rvh = render_frame_host_->render_view_host();

  if (is_main_frame && old_view && old_rvh != new_rvh) {
    // Note that this hides the RenderWidget but does not hide the Page. If it
    // did hide the Page then making a new RenderFrameHost on another call to
    // here would need to make sure it showed the `blink::WebView` when the
    // RenderWidget was created as visible.
    //
    // TODO(crbug.com/40262486): In addition to the RenderWidgetHostView
    // visibility there is also the concept of PageVisibilityState. The
    // PageLifecycleStateManager will have the RenderViewHostImpl notify the
    // blink::Page of changes to the PageVisibilityState. This currently does
    // not affect the visibility of the blink::WidgetBase. We should unify these
    // two visibility states to prevent them from drifting.
    old_view->Hide();
    if (old_render_frame_host->child_count()) {
      old_render_frame_host->SetVisibilityForChildViews(false);
    }
  }

  RenderWidgetHostView* new_view = render_frame_host_->GetView();
  // Since the committing renderer frame is live, the RenderWidgetHostView must
  // also exist. For a local root frame, they share lifetimes exactly. For
  // another child frame, the RenderWidgetHostView comes from a parent, but if
  // this renderer frame is live its ancestors must be as well.
  DCHECK(new_view);

  if (focus_render_view) {
    if (is_main_frame) {
      // If the old page was focused, ensure the new one preserves
      // focus. This needs to be done differently depending on whether the main
      // frame is an outermost main frame or embedded in a nested FrameTree,
      // such as for a <webview> guest.  In the outermost case, focus the root
      // RenderWidgetHostView, which will also end up focusing the
      // RenderWidgetHost.  For the nested main frame case this won't work,
      // since the view will be a RenderWidgetHostViewChildFrame, and focusing
      // it would end up trying to focus the root view. Instead, we need to
      // focus the new main frame's RenderWidgetHost, which would set the new
      // widget as focused and also propagate page-level focus to the
      // corresponding renderer process.
      if (frame_tree_node_->GetParentOrOuterDocumentOrEmbedder()) {
        render_frame_host_->GetRenderWidgetHost()->Focus();
      } else {
        new_view->Focus();
      }
    } else {
      // The current WebContents has page-level focus, so we need to propagate
      // page-level focus to the subframe's renderer. Before doing that, also
      // tell the new renderer what the focused frame is if that frame is not
      // in its process, so that Blink's page-level focus logic won't try to
      // reset frame focus to the main frame.  See https://crbug.com/802156.
      FrameTreeNode* focused_frame =
          frame_tree_node_->frame_tree().GetFocusedFrame();
      SiteInstanceGroup* site_instance_group =
          render_frame_host_->GetSiteInstance()->group();
      if (focused_frame && !focused_frame->IsMainFrame() &&
          focused_frame->current_frame_host()->GetSiteInstance()->group() !=
              site_instance_group) {
        focused_frame->GetBrowsingContextStateForSubframe()
            ->GetRenderFrameProxyHost(site_instance_group)
            ->SetFocusedFrame();
      }
      frame_tree_node_->frame_tree().SetPageFocus(site_instance_group, true);
    }
  }

  // Notify that we have no `old_view` from which to TakeFallbackContentFrom.
  // This will clear the current Fallback Surface, which would be from a
  // previous Navigation. This way we do not display old content if this new
  // PendingCommit does not lead to a successful Navigation. This must be called
  // before NotifySwappedFromRenderManager, which will allocate a new
  // viz::LocalSurfaceId, which will allow the Renderer to submit new content.
  // TODO(crbug.com/40052076): Remove this once CommitPending has more explicit
  // shutdown, both for successful and failed navigations.
  if (!old_view) {
    delegate_->NotifySwappedFromRenderManagerWithoutFallbackContent(
        render_frame_host_.get());
  }

  bool should_take_fallback_content = false;
  // Make the new view show the contents of old view until it has something
  // useful to show. Note that we don't do this for BFCache entries with a
  // valid surface id, because it already has that surface embedded through
  // `RenderFrameHostImpl::WillLeaveBackForwardCache` and the timeout that
  // would be set here will clear that frame (incorrectly).
  if (is_main_frame && allow_paint_holding && old_view &&
      old_view != new_view) {
    // If allowed, we should take the fallback in any of the following cases:
    //  - We're not coming from BFCache
    //  - We don't have a valid surface id to display.
    auto* render_widget_host_view_base =
        static_cast<RenderWidgetHostViewBase*>(render_frame_host_->GetView());
    should_take_fallback_content =
        prev_state !=
            RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache ||
        !render_widget_host_view_base->GetLocalSurfaceId().is_valid() ||
        render_widget_host_view_base->is_evicted();
  }

  // Notify that we've swapped RenderFrameHosts. We do this before shutting down
  // the RFH so that we can clean up RendererResources related to the RFH first.
  delegate_->NotifySwappedFromRenderManager(old_render_frame_host.get(),
                                            render_frame_host_.get());

  if (should_take_fallback_content) {
    new_view->TakeFallbackContentFrom(old_view);
  }

  // The RenderViewHost keeps track of the main RenderFrameHost routing id.
  // If this is committing a main frame navigation, update it and set the
  // routing id in the RenderViewHost associated with the old RenderFrameHost
  // to MSG_ROUTING_NONE.
  if (is_main_frame) {
    // If the RenderViewHost is transitioning from an inactive to active state,
    // it was reused, so dispatch a RenderViewReady event. For example, this is
    // necessary to hide the sad tab if one is currently displayed. See
    // https://crbug.com/591984.
    //
    // Note that observers of RenderViewReady() will see the updated main frame
    // routing ID, since PostRenderViewReady() posts a task.
    //
    // TODO(alexmos): Remove this and move RenderViewReady consumers to use
    // the main frame's RenderFrameCreated instead.
    if (!new_rvh->is_active())
      new_rvh->PostRenderViewReady();

    new_rvh->SetMainFrameRoutingId(render_frame_host_->GetRoutingID());
    if (old_rvh != new_rvh)
      old_rvh->SetMainFrameRoutingId(MSG_ROUTING_NONE);
  }

  // Store the old_render_frame_host's current frame size so that it can be used
  // to initialize the child RWHV.
  std::optional<gfx::Size> old_size = old_render_frame_host->frame_size();

  // Store the old_render_frame_host's BrowsingContextState so that it can be
  // used to update/delete proxies.
  scoped_refptr<BrowsingContextState> old_browsing_context_state =
      old_render_frame_host->browsing_context_state();

  // Unload the old frame now that the new one is visible.
  // This will unload it and schedule it for deletion when the unload ack
  // arrives (or immediately if the process isn't live).
  UnloadOldFrame(std::move(old_render_frame_host));

  // Since the new RenderFrameHost is now committed, there must be no proxies
  // for its SiteInstance. Delete any existing ones.
  render_frame_host_->browsing_context_state()->DeleteRenderFrameProxyHost(
      render_frame_host_->GetSiteInstance()->group());

  // If this is a top-level frame, and COOP triggered a BrowsingInstance swap,
  // make sure all relationships with the previous BrowsingInstance are severed
  // by removing the opener, the openee's opener, and the proxies with unrelated
  // SiteInstances.
  // TODO(crbug.com/40205442): Make this a no-op for the non-legacy
  // implementation of BrowsingContextState.
  if (clear_proxies_on_commit) {
    TRACE_EVENT("navigation",
                "RenderFrameHostManager::CommitPending_ClearProxiesOnCommit");
    DCHECK(frame_tree_node_->IsMainFrame());

    // If this frame has opened popups, we need to clear the opened popup's
    // opener. This is done here on the browser side. A similar mechanism occurs
    // in the renderer process when the `blink::WebView` of this frame is
    // destroyed, via blink::OpenedFrameTracker.
    frame_tree_node_->ClearOpenerReferences();

    // We've just cleared other frames' "opener" referencing this frame, we now
    // clear this frame's "opener".
    if (frame_tree_node_->opener() &&
        !render_frame_host_->GetSiteInstance()->IsRelatedSiteInstance(
            frame_tree_node_->opener()
                ->current_frame_host()
                ->GetSiteInstance())) {
      frame_tree_node_->SetOpener(nullptr);
      // Note: It usually makes sense to notify the proxies of that frame that
      // the opener was removed. However since these proxies are destroyed right
      // after it is not necessary in this particuliar case.
    }

    // Now that opener references are gone in both direction, we can clear the
    // underlying proxies that were used for that purpose.
    std::vector<RenderFrameProxyHost*> removed_proxies;
    for (auto& it :
         render_frame_host_->browsing_context_state()->proxy_hosts()) {
      const auto& proxy = it.second;
      // The outer delegate proxy is *always* cross-browsing context group, but
      // it is the only proxy we must preserve.
      if (!render_frame_host_->GetSiteInstance()
               ->group()
               ->IsRelatedSiteInstanceGroup(proxy->site_instance_group()) &&
          proxy.get() != GetProxyToOuterDelegate()) {
        removed_proxies.push_back(proxy.get());
      }
    }

    TRACE_EVENT("navigation",
                "RenderFrameHostManager::CommitPending_"
                "DeleteProxiesFromOldBrowsingContextState",
                ChromeTrackEvent::kBrowsingContextState,
                old_browsing_context_state);
    for (auto* proxy : removed_proxies) {
      // After deleting the proxy we will not have either a proxy or
      // main frame associated with the RenderViewHost. Do not allow
      // it to be used for new navigations in this inconsistent state.
      proxy->GetRenderViewHost()->DisallowReuse();
      old_browsing_context_state->DeleteRenderFrameProxyHost(
          proxy->site_instance_group());
    }
  }

  // If this is a subframe or inner frame tree, it should have a
  // CrossProcessFrameConnector created already.  Use it to link the new RFH's
  // view to the proxy that belongs to the parent frame's SiteInstance. If this
  // navigation causes an out-of-process frame to return to the same process as
  // its parent, the proxy would have been removed from
  // render_frame_host_->browsing_context_state()->proxy_hosts() above.
  // Note: We do this after unloading the old RFH because that may create
  // the proxy we're looking for.
  RenderFrameProxyHost* proxy_to_parent_or_outer_delegate =
      GetProxyToParentOrOuterDelegate();
  if (proxy_to_parent_or_outer_delegate) {
    proxy_to_parent_or_outer_delegate->SetChildRWHView(
        static_cast<RenderWidgetHostViewChildFrame*>(new_view),
        old_size ? &*old_size : nullptr, allow_paint_holding);
  }

  if (render_frame_host_->is_local_root()) {
    // RenderFrames are created with a hidden RenderWidgetHost. When navigation
    // finishes, we show it if the delegate is shown.
    if (!frame_tree_node_->frame_tree().IsHidden()) {
      new_view->Show();
      if (render_frame_host_->child_count()) {
        render_frame_host_->SetVisibilityForChildViews(true);
      }
    }
  }

  // If we took fallback content, we need to start a timeout timer to clear it
  // in case the new renderer does not produce a timely frame.
  if (should_take_fallback_content) {
    static_cast<RenderWidgetHostImpl*>(new_view->GetRenderWidgetHost())
        ->StartNewContentRenderingTimeout();
  }

  // The process will no longer try to exit, so we can decrement the count.
  render_frame_host_->GetProcess()->RemovePendingView();

  // After all is done, there must never be a proxy in the list which has the
  // same SiteInstanceGroup as the current RenderFrameHost.
  CHECK(!render_frame_host_->browsing_context_state()->GetRenderFrameProxyHost(
      render_frame_host_->GetSiteInstance()->group()));
}

std::unique_ptr<RenderFrameHostImpl> RenderFrameHostManager::SetRenderFrameHost(
    std::unique_ptr<RenderFrameHostImpl> render_frame_host) {
  // Swap the two.
  std::unique_ptr<RenderFrameHostImpl> old_render_frame_host =
      std::move(render_frame_host_);
  render_frame_host_ = std::move(render_frame_host);

  FrameTree& frame_tree = frame_tree_node_->frame_tree();

  // If the feature is enabled, check if there is a corresponding speculative
  // RenderViewHost that also needs to be swapped in.
  if (render_frame_host_ && render_frame_host_->GetRenderViewHost() ==
                                frame_tree.speculative_render_view_host()) {
    CHECK(frame_tree_node_->IsMainFrame());
    frame_tree.MakeSpeculativeRVHCurrent();
  }

  // Swapping the current RenderFrameHost in a FrameTreeNode comes along with an
  // update to its LifecycleStateImpl.

  // The lifecycle state of the old RenderFrameHost is either:
  // - kActive: starts unloading or enters the BackForwardCache.
  // - kPrerendering: starts unloading.

  // The lifecycle state of the new RenderFrameHost is either:
  // - kSpeculative: for early-commit navigations (see
  //   https://crbug.com/1072817) and when attaching an inner delegate (when
  //   embedding one WebContents inside another).
  // - kPendingCommit: for regular cross-RenderFrameHost navigations.
  // - kBackForwardCache: for BackForwardCache restore navigation.
  // - kPrerendering: for a prerender activation navigation.
  // It should become kActive in the primary frame tree and kPrerendering for
  // navigations inside the prerendered frame tree.

  // Note that Prerender2 introduces the concept of a prerendered frame tree.
  // It also allows navigations within the prerendered tree to enable loading
  // and running pages while in the background. Here, the old RenderFrameHost's
  // state isn't kActive, but kPrerendering. The new RenderFrameHost doesn't
  // become kActive, but kPrerendering because documents in kPrerendering state
  // are considered current in the prerendered frame tree and invisible to the
  // user, unlike kActive state.
  if (render_frame_host_) {
    if (frame_tree.is_prerendering()) {
      // Prerendering pages do not currently support early commit, so
      // speculative RFHs for prerendering pages will always go through
      // kPendingCommit first.
      DCHECK_NE(render_frame_host_->lifecycle_state(),
                LifecycleStateImpl::kSpeculative);
      if (render_frame_host_->lifecycle_state() ==
          LifecycleStateImpl::kPendingCommit) {
        render_frame_host_->SetLifecycleState(
            LifecycleStateImpl::kPrerendering);
      }
    } else {
      if (render_frame_host_->lifecycle_state() != LifecycleStateImpl::kActive)
        render_frame_host_->SetLifecycleState(LifecycleStateImpl::kActive);
    }
  }

  // Note that we don't know yet what the next state will be, so it is
  // temporarily marked with SetHasPendingLifecycleStateUpdate().
  // TODO(crbug.com/40170710): Determine the next state earlier and
  // remove SetHasPendingLifecycleStateUpdate().
  if (old_render_frame_host && !old_render_frame_host->IsPendingDeletion()) {
    // After the old RenderFrameHost is no longer the current one, set the value
    // of |has_pending_lifecycle_state_update_| to true if it is not null.
    old_render_frame_host->SetHasPendingLifecycleStateUpdate(
        /*last_frame_type=*/frame_tree_node_->GetFrameType());
  }

  // Update the count of active documents using this SiteInstance, both for
  // active document tracking and related active contents tracking.
  if (render_frame_host_) {
    if (frame_tree_node_->IsMainFrame()) {
      render_frame_host_->GetSiteInstance()
          ->IncrementRelatedActiveContentsCount();
    }
  }
  if (old_render_frame_host) {
    if (frame_tree_node_->IsMainFrame()) {
      old_render_frame_host->GetSiteInstance()
          ->DecrementRelatedActiveContentsCount();
    }
  }

  // Update the owner of the new RenderFrameHost to point to the current frame.
  // Note that this is a no-op for pending commit RenderFrameHosts (which start
  // with owner pointing to the FrameTreeNode owning them) and prerendering
  // activations (where RenderFrameHost's owner has been updated in
  // PrerenderHost::Activate), but is necessary for RFHs restored from
  // back/forward cache.
  if (render_frame_host_)
    render_frame_host_->SetRenderFrameHostOwner(frame_tree_node_);

  if (old_render_frame_host)
    old_render_frame_host->SetRenderFrameHostOwner(nullptr);

  if (render_frame_host_) {
    // Speculative fix for https://crbug.com/354382462 where we're seeing a page
    // in BFCache sharing SiteInstances with a non-BFCached page. We're
    // suspecting that a navigation with a related SiteInstance is ongoing when
    // the page enters BFCache, and later that navigation commits. To prevent
    // confusion, evict any BFCached page that has a related SiteInstance as
    // this RenderFrameHost now.
    // TODO(https://crbug.com/354382462): Make this a proper fix with a repro
    // test and delete the debugging code below and elsewhere.
    GetNavigationController()
        .GetBackForwardCache()
        .EvictFramesInRelatedSiteInstances(
            render_frame_host_->GetSiteInstance());
    SiteInstanceGroupId sig_id =
        render_frame_host_->GetSiteInstance()->group()->GetId();
    bool rfh_in_bfcache =
        GetNavigationController()
            .GetBackForwardCache()
            .IsRenderFrameHostWithSIGInBackForwardCacheForDebugging(sig_id);
    bool rfph_in_bfcache =
        GetNavigationController()
            .GetBackForwardCache()
            .IsRenderFrameProxyHostWithSIGInBackForwardCacheForDebugging(
                sig_id);
    bool rvh_in_bfcache =
        GetNavigationController()
            .GetBackForwardCache()
            .IsRenderViewHostWithMapIdInBackForwardCacheForDebugging(
                *static_cast<RenderViewHostImpl*>(
                    render_frame_host_->GetRenderViewHost()));
    if (rfh_in_bfcache || rfph_in_bfcache || rvh_in_bfcache) {
      SCOPED_CRASH_KEY_BOOL("rvh-double", "rfh_in_bfcache", rfh_in_bfcache);
      SCOPED_CRASH_KEY_BOOL("rvh-double", "rfph_in_bfcache", rfph_in_bfcache);
      SCOPED_CRASH_KEY_BOOL("rvh-double", "rvh_in_bfcache", rvh_in_bfcache);
      SCOPED_CRASH_KEY_NUMBER("rvh-double", "related_active_contents",
                              render_frame_host_->GetSiteInstance()
                                  ->GetRelatedActiveContentsCount());
      base::debug::DumpWithoutCrashing();
    }
  }
  return old_render_frame_host;
}

void RenderFrameHostManager::CollectOpenerFrameTrees(
    SiteInstanceGroup* site_instance_group,
    std::vector<FrameTree*>* opener_frame_trees,
    std::unordered_set<FrameTreeNode*>* nodes_with_back_links,
    std::unordered_set<FrameTreeNode*>* cross_browsing_context_group_openers) {
  CHECK(opener_frame_trees);
  opener_frame_trees->push_back(&frame_tree_node_->frame_tree());

  // Add the FrameTree of the given node's opener to the list of
  // |opener_frame_trees| if it doesn't exist there already. |visited_index|
  // indicates which FrameTrees in |opener_frame_trees| have already been
  // visited (i.e., those at indices less than |visited_index|).
  // |nodes_with_back_links| collects FrameTreeNodes with openers in FrameTrees
  // that have already been visited (such as those with cycles).
  size_t visited_index = 0;
  while (visited_index < opener_frame_trees->size()) {
    FrameTree* frame_tree = (*opener_frame_trees)[visited_index];
    visited_index++;
    for (FrameTreeNode* node : frame_tree->Nodes()) {
      if (!node->opener())
        continue;

      // Do not iterate recursively on FrameTrees in different BrowsingInstances
      // in the same CoopRelatedGroup. Instead, simply record the direct opener
      // in `cross_browsing_context_group_openers`. We can end up here with
      // BrowsingInstance not in the same CoopRelatedGroup for rare cases
      // involving outer delegate proxies. For example when a chrome app webview
      // gets a new opener, we will iterate this opener tree and create proxies
      // for newly connected frames in the outer delegate SiteInstanceGroup. We
      // do not want to interact with these, so explicitly verify the
      // CoopRelatedGroups match.
      // TODO(crbug.com/40266207): It is not clear that this iteration is
      // actually useful for outer delegate proxies. See if this can be
      // prevented to simplify logic here.
      SiteInstanceGroup* opener_sig =
          node->opener()->current_frame_host()->GetSiteInstance()->group();
      if (site_instance_group &&
          !site_instance_group->IsRelatedSiteInstanceGroup(opener_sig) &&
          site_instance_group->IsCoopRelatedSiteInstanceGroup(opener_sig)) {
        cross_browsing_context_group_openers->insert(node->opener());
        continue;
      }

      FrameTree& opener_tree = node->opener()->frame_tree();
      const auto& existing_tree_it =
          base::ranges::find(*opener_frame_trees, &opener_tree);

      if (existing_tree_it == opener_frame_trees->end()) {
        // This is a new opener tree that we will need to process.
        opener_frame_trees->push_back(&opener_tree);
      } else {
        // If this tree is already on our processing list *and* we have visited
        // it,
        // then this node's opener is a back link.  This means the node will
        // need
        // special treatment to process its opener.
        size_t position =
            std::distance(opener_frame_trees->begin(), existing_tree_it);
        if (position < visited_index)
          nodes_with_back_links->insert(node);
      }
    }
  }
}

void RenderFrameHostManager::CreateOpenerProxies(
    SiteInstanceGroup* group,
    FrameTreeNode* skip_this_node,
    const scoped_refptr<BrowsingContextState>& browsing_context_state) {
  // TODO(crbug.com/40205442): Add a DCHECK verifying that |instance
  // is a related site instance to the site instance in |render_frame_host_|. At
  // the moment, this DCHECK fails due to a bug in choosing SiteInstance in
  // web_contents_impl.cc.
  std::vector<FrameTree*> opener_frame_trees;
  std::unordered_set<FrameTreeNode*> nodes_with_back_links;
  std::unordered_set<FrameTreeNode*> cross_browsing_context_group_openers;

  CollectOpenerFrameTrees(group, &opener_frame_trees, &nodes_with_back_links,
                          &cross_browsing_context_group_openers);

  // Create the proxies for openers outside of this BrowsingInstance. They are
  // created separately on purpose, because we do not want to create proxies for
  // their entire tree, only the single point of contact with this
  // BrowsingInstance (and for any necessary ancestor frames).
  for (auto* node : cross_browsing_context_group_openers) {
    node->render_manager()->CreateRenderFrameProxyAndAncestorChainIfNeeded(
        group);
  }

  // Create opener proxies for frame trees, processing furthest openers from
  // this node first and this node last.  In the common case without cycles,
  // this will ensure that each tree's openers are created before the tree's
  // nodes need to reference them.
  for (FrameTree* tree : base::Reversed(opener_frame_trees)) {
    tree->root()->render_manager()->CreateOpenerProxiesForFrameTree(
        group, skip_this_node, browsing_context_state);
  }

  // Set openers for nodes in |nodes_with_back_links| in a second pass.
  // The proxies created at these FrameTreeNodes in
  // CreateOpenerProxiesForFrameTree won't have their opener routing ID
  // available when created due to cycles or back links in the opener chain.
  // They must have their openers updated as a separate step after proxy
  // creation.
  for (auto* node : nodes_with_back_links) {
    RenderFrameProxyHost* proxy = node->render_manager()
                                      ->current_frame_host()
                                      ->browsing_context_state()
                                      ->GetRenderFrameProxyHost(group);
    // If there is no proxy, the cycle may involve nodes in the same process,
    // or, if this is a subframe, --site-per-process may be off.  Either way,
    // there's nothing more to do.
    if (!proxy || !proxy->is_render_frame_proxy_live())
      continue;

    auto opener_frame_token =
        node->render_manager()->GetOpenerFrameToken(group);
    DCHECK(opener_frame_token);
    proxy->GetAssociatedRemoteFrame()->UpdateOpener(opener_frame_token);
  }
}

void RenderFrameHostManager::CreateOpenerProxiesForFrameTree(
    SiteInstanceGroup* group,
    FrameTreeNode* skip_this_node,
    const scoped_refptr<BrowsingContextState>& browsing_context_state) {
  // Currently, this function is only called on main frames.  It should
  // actually work correctly for subframes as well, so if that need ever
  // arises, it should be sufficient to remove this DCHECK.
  DCHECK(frame_tree_node_->IsMainFrame());

  FrameTree& frame_tree = frame_tree_node_->frame_tree();

  // Ensure that all the nodes in the opener's FrameTree have
  // RenderFrameProxyHosts for the new SiteInstanceGroup.  Only pass the node to
  // be skipped if it's in the same FrameTree.
  if (skip_this_node && &skip_this_node->frame_tree() != &frame_tree) {
    skip_this_node = nullptr;
  }
  frame_tree.CreateProxiesForSiteInstanceGroup(skip_this_node, group,
                                               browsing_context_state);
}

std::optional<blink::FrameToken> RenderFrameHostManager::GetOpenerFrameToken(
    SiteInstanceGroup* group) {
  if (!frame_tree_node_->opener())
    return std::nullopt;

  return frame_tree_node_->opener()
      ->render_manager()
      ->GetFrameTokenForSiteInstanceGroup(group);
}

void RenderFrameHostManager::ExecutePageBroadcastMethod(
    PageBroadcastMethodCallback callback,
    SiteInstanceGroup* group_to_skip) {
  DCHECK(!frame_tree_node_->parent());

  // When calling a PageBroadcast Mojo method for an inner WebContents, we don't
  // want to also call it for the outer WebContent's frame as well.
  RenderFrameProxyHost* outer_delegate_proxy =
      IsMainFrameForInnerDelegate() ? GetProxyToOuterDelegate() : nullptr;
  for (const auto& pair :
       render_frame_host_->browsing_context_state()->proxy_hosts()) {
    if (outer_delegate_proxy == pair.second.get())
      continue;
    if (pair.second->site_instance_group() == group_to_skip) {
      continue;
    }
    callback.Run(pair.second->GetRenderViewHost());
  }

  if (speculative_render_frame_host_ &&
      speculative_render_frame_host_->GetSiteInstance()->group() !=
          group_to_skip) {
    callback.Run(speculative_render_frame_host_->render_view_host());
  }

  if (render_frame_host_->GetSiteInstance()->group() != group_to_skip) {
    callback.Run(render_frame_host_->render_view_host());
  }
}

void RenderFrameHostManager::ExecuteRemoteFramesBroadcastMethod(
    RemoteFramesBroadcastMethodCallback callback,
    SiteInstanceGroup* group_to_skip) {
  DCHECK(!frame_tree_node_->parent());

  // When calling a ExecuteRemoteFramesBroadcastMethod() for an inner
  // WebContents, we don't want to also call it for the outer WebContent's
  // frame as well.
  RenderFrameProxyHost* outer_delegate_proxy =
      IsMainFrameForInnerDelegate() ? GetProxyToOuterDelegate() : nullptr;
  render_frame_host_->browsing_context_state()
      ->ExecuteRemoteFramesBroadcastMethod(callback, group_to_skip,
                                           outer_delegate_proxy);
}

void RenderFrameHostManager::EnsureRenderFrameHostVisibilityConsistent() {
  RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view &&
      static_cast<RenderWidgetHostImpl*>(view->GetRenderWidgetHost())
              ->is_hidden() != frame_tree_node_->frame_tree().IsHidden()) {
    if (frame_tree_node_->frame_tree().IsHidden()) {
      view->Hide();
    } else {
      view->Show();
    }
  }
}

void RenderFrameHostManager::EnsureRenderFrameHostPageFocusConsistent() {
  frame_tree_node_->frame_tree().SetPageFocus(
      render_frame_host_->GetSiteInstance()->group(),
      frame_tree_node_->frame_tree()
          .root()
          ->current_frame_host()
          ->GetRenderWidgetHost()
          ->is_focused());
}

void RenderFrameHostManager::CreateNewFrameForInnerDelegateAttachIfNecessary() {
  TRACE_EVENT(
      "navigation",
      "RenderFrameHostManager::CreateNewFrameForInnerDelegateAttachIfNecessary",
      ChromeTrackEvent::kFrameTreeNodeInfo, *frame_tree_node_);
  DCHECK(is_attaching_inner_delegate());
  // There should be no navigations happening on the frame to attach the inner
  // delegate to. This is guaranteed by `is_attaching_inner_delegate()` state
  // checks, which will prevent NavigationRequests from being created on this
  // frame. Since that state will be set synchronously after we got the
  // RenderFrameCreated notification for this frame, no navigation should be
  // able to start on the frame.
  if (current_frame_host()->HasPendingCommitNavigation() ||
      frame_tree_node_->navigation_request() ||
      speculative_render_frame_host_) {
    NOTREACHED_IN_MIGRATION();
    base::debug::DumpWithoutCrashing();
    NotifyPrepareForInnerDelegateAttachComplete(false /* success */);
    return;
  }
  // Reset the loading state. Even though there should be no navigations in the
  // injected frame, it might not have received a DidStopLoading call.
  // See also https://crbug.com/1400157.
  current_frame_host()->ResetLoadingState();

  DCHECK(!current_frame_host()->is_main_frame());
  if (current_frame_host()->GetSiteInstance() ==
      current_frame_host()->GetParent()->GetSiteInstance()) {
    // At this point the beforeunload is dispatched and the result has been to
    // proceed with attaching. There are also no upcoming navigations which
    // would interfere with the upcoming attach. If the frame is in the same
    // SiteInstance as its parent it can be safely used for attaching an inner
    // Delegate.
    NotifyPrepareForInnerDelegateAttachComplete(true /* success */);
    return;
  }

  // TODO(crbug.com/40249634): Some of these may no longer be necessary
  // now that MimeHandlerView's embedded case uses the same code path as the
  // full page case.

  // We need a new RenderFrameHost in its parent's SiteInstance to be able to
  // safely use the WebContentsImpl attach API.
  DCHECK(!speculative_render_frame_host_);
  if (!CreateSpeculativeRenderFrameHost(
          current_frame_host()->GetSiteInstance(),
          current_frame_host()->GetParent()->GetSiteInstance(),
          /*recovering_without_early_commit=*/false)) {
    NotifyPrepareForInnerDelegateAttachComplete(false /* success */);
    return;
  }
  // Swap in the speculative frame. It will later be replaced when
  // WebContents::AttachToOuterWebContentsFrame is called.
  speculative_render_frame_host_->SwapIn();

  CommitPending(std::move(speculative_render_frame_host_),
                /*pending_stored_page=*/nullptr,
                /*clear_proxies_on_commit=*/false,
                /*allow_paint_holding=*/false);
  NotifyPrepareForInnerDelegateAttachComplete(true /* success */);
}

void RenderFrameHostManager::NotifyPrepareForInnerDelegateAttachComplete(
    bool success) {
  DCHECK(is_attaching_inner_delegate());
  int32_t process_id = success ? render_frame_host_->GetProcess()->GetID()
                               : ChildProcessHost::kInvalidUniqueID;
  int32_t routing_id =
      success ? render_frame_host_->GetRoutingID() : MSG_ROUTING_NONE;
  // Invoking the callback asynchronously to meet the APIs promise.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RenderFrameHost::PrepareForInnerWebContentsAttachCallback callback,
             int32_t process_id, int32_t routing_id) {
            std::move(callback).Run(
                RenderFrameHostImpl::FromID(process_id, routing_id));
          },
          std::move(attach_inner_delegate_callback_), process_id, routing_id));
}

NavigationControllerImpl& RenderFrameHostManager::GetNavigationController() {
  return frame_tree_node_->frame_tree().controller();
}

base::WeakPtr<RenderFrameHostManager> RenderFrameHostManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
