// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_internals/process_internals_handler_impl.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

namespace {

using IsolatedOriginSource = ChildProcessSecurityPolicy::IsolatedOriginSource;

// Creates FrameInfoPtr but does not populate subframes.
::mojom::FrameInfoPtr RenderFrameHostToFrameInfoNoTraverse(
    RenderFrameHostImpl* frame,
    ::mojom::FrameInfo::Type type) {
  auto frame_info = ::mojom::FrameInfo::New();

  frame_info->routing_id = frame->GetRoutingID();
  frame_info->agent_scheduling_group_id =
      frame->GetAgentSchedulingGroup().id_for_debugging();
  frame_info->process_id = frame->GetProcess()->GetID();
  frame_info->last_committed_url =
      frame->GetLastCommittedURL().is_valid()
          ? std::make_optional(frame->GetLastCommittedURL())
          : std::nullopt;
  frame_info->type = type;

  SiteInstanceImpl* site_instance =
      static_cast<SiteInstanceImpl*>(frame->GetSiteInstance());
  frame_info->site_instance = ::mojom::SiteInstanceInfo::New();
  frame_info->site_instance->id = site_instance->GetId().value();
  frame_info->site_instance->locked =
      site_instance->GetProcess()->GetProcessLock().is_locked_to_site();
  frame_info->site_instance->site_url =
      site_instance->HasSite()
          ? std::make_optional(site_instance->GetSiteInfo().site_url())
          : std::nullopt;
  frame_info->site_instance->is_guest = site_instance->IsGuest();
  frame_info->site_instance->is_pdf = site_instance->IsPdf();
  frame_info->site_instance->is_sandbox_for_iframes =
      site_instance->GetSiteInfo().is_sandboxed();
  frame_info->site_instance->site_instance_group_id =
      site_instance->group() ? site_instance->group()->GetId().value() : 0;
  frame_info->site_instance->browsing_instance_id =
      site_instance->GetBrowsingInstanceId().value();

  // If the SiteInstance has a non-default StoragePartition, include a basic
  // string representation of it.  Skip cases where the StoragePartition is
  // already conveyed in the site URL to avoid redundancy.
  const auto& partition = site_instance->GetStoragePartitionConfig();
  if (!partition.is_default() &&
      site_instance->GetSiteInfo().site_url().spec().find(
          partition.partition_domain()) == std::string::npos) {
    std::string partition_description =
        base::StrCat({partition.partition_domain().c_str(), "/",
                      partition.partition_name().c_str(),
                      partition.in_memory() ? "" : "?persist"});
    frame_info->site_instance->storage_partition =
        std::make_optional(partition_description);
  }

  // Only send a process lock URL if it's different from the site URL.  In the
  // common case they are the same, so we avoid polluting the UI with two
  // identical URLs.
  bool should_show_lock_url = frame_info->site_instance->locked &&
                              site_instance->GetSiteInfo().process_lock_url() !=
                                  site_instance->GetSiteInfo().site_url();
  frame_info->site_instance->process_lock_url =
      should_show_lock_url
          ? std::make_optional(site_instance->GetSiteInfo().process_lock_url())
          : std::nullopt;

  frame_info->site_instance->requires_origin_keyed_process =
      site_instance->GetSiteInfo().requires_origin_keyed_process();

  return frame_info;
}

// Traverses over all subframes for the given |frame|.
::mojom::FrameInfoPtr RenderFrameHostToFrameInfo(
    WebContentsImpl* web_contents,
    RenderFrameHostImpl* frame,
    ::mojom::FrameInfo::Type type) {
  std::map<RenderFrameHostImpl*, ::mojom::FrameInfo*> all_frame_info;

  // Store the outermost frame info because we will need to return it and
  // |all_frame_info| does not retain ownership of the mojom::FrameInfo.
  ::mojom::FrameInfoPtr outermost_frame_info =
      RenderFrameHostToFrameInfoNoTraverse(frame, type);
  all_frame_info[frame] = outermost_frame_info.get();

  // Execute over all frames appending any frames encountered to the parent's
  // subframe data.
  frame->ForEachRenderFrameHostWithAction(
      [web_contents, outermost_frame = frame, type,
       &all_frame_info](RenderFrameHostImpl* rfh) {
        // We've already handled the outermost frame outside of this.
        if (rfh == outermost_frame)
          return RenderFrameHost::FrameIterationAction::kContinue;

        // If this is a nested WebContents skip it, it will be encountered
        // by the GetAllWebContents iteration.
        if (WebContents::FromRenderFrameHost(rfh) != web_contents)
          return RenderFrameHost::FrameIterationAction::kSkipChildren;

        ::mojom::FrameInfoPtr frame_info =
            RenderFrameHostToFrameInfoNoTraverse(rfh, type);
        all_frame_info[rfh] = frame_info.get();
        RenderFrameHostImpl* parent = rfh->GetParentOrOuterDocument();
        DCHECK(base::Contains(all_frame_info, parent));
        all_frame_info[parent]->subframes.push_back(std::move(frame_info));
        return RenderFrameHost::FrameIterationAction::kContinue;
      });

  return outermost_frame_info;
}

// Adds `host` to `out_frames` if it is a prerendered main frame.
RenderFrameHost::FrameIterationAction CollectPrerenders(
    WebContentsImpl* web_contents,
    RenderFrameHostImpl* host,
    std::vector<::mojom::FrameInfoPtr>& out_frames) {
  if (!host->GetParentOrOuterDocument()) {
    if (host->GetLifecycleState() ==
        RenderFrameHost::LifecycleState::kPrerendering) {
      out_frames.push_back(RenderFrameHostToFrameInfo(
          web_contents, host, ::mojom::FrameInfo::Type::kPrerender));
    }
    return RenderFrameHost::FrameIterationAction::kSkipChildren;
  }
  return RenderFrameHost::FrameIterationAction::kContinue;
}

std::string IsolatedOriginSourceToString(IsolatedOriginSource source) {
  switch (source) {
    case IsolatedOriginSource::BUILT_IN:
      return "Built-in";
    case IsolatedOriginSource::COMMAND_LINE:
      return "Command line";
    case IsolatedOriginSource::FIELD_TRIAL:
      return "Field trial";
    case IsolatedOriginSource::POLICY:
      return "Device policy";
    case IsolatedOriginSource::TEST:
      return "Test";
    case IsolatedOriginSource::USER_TRIGGERED:
      return "User-triggered";
    case IsolatedOriginSource::WEB_TRIGGERED:
      return "Web-triggered";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

}  // namespace

ProcessInternalsHandlerImpl::ProcessInternalsHandlerImpl(
    BrowserContext* browser_context,
    mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver)
    : browser_context_(browser_context), receiver_(this, std::move(receiver)) {}

ProcessInternalsHandlerImpl::~ProcessInternalsHandlerImpl() = default;

void ProcessInternalsHandlerImpl::GetProcessCountInfo(
    GetProcessCountInfoCallback callback) {
  ::mojom::ProcessCountInfoPtr info = ::mojom::ProcessCountInfo::New();
  info->renderer_process_count_total = RenderProcessHostImpl::GetProcessCount();
  info->renderer_process_count_for_limit =
      RenderProcessHostImpl::GetProcessCountForLimit();
  info->renderer_process_limit =
      RenderProcessHost::GetMaxRendererProcessCount();

  std::move(callback).Run(std::move(info));
}

void ProcessInternalsHandlerImpl::GetIsolationMode(
    GetIsolationModeCallback callback) {
  std::vector<std::string_view> modes;
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    modes.push_back("Site Per Process");
  if (SiteIsolationPolicy::AreIsolatedOriginsEnabled())
    modes.push_back("Isolate Origins");
  if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled())
    modes.push_back("Strict Origin Isolation");
  if (SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled())
    modes.push_back("COOP");

  // Retrieve any additional site isolation modes controlled by the embedder.
  std::vector<std::string> additional_modes =
      GetContentClient()->browser()->GetAdditionalSiteIsolationModes();
  std::move(additional_modes.begin(), additional_modes.end(),
            std::back_inserter(modes));

  std::move(callback).Run(modes.empty() ? "Disabled"
                                        : base::JoinString(modes, ", "));
}

void ProcessInternalsHandlerImpl::GetProcessPerSiteMode(
    GetProcessPerSiteModeCallback callback) {
  if (!GetContentClient()
           ->browser()
           ->ShouldAllowProcessPerSiteForMultipleMainFrames(browser_context_)) {
    std::move(callback).Run("off (ContentClient policy)");
    return;
  }
  if (!base::FeatureList::IsEnabled(
          features::kProcessPerSiteUpToMainFrameThreshold)) {
    std::move(callback).Run("off (feature setting)");
    return;
  }
  std::move(callback).Run(base::StringPrintf(
      "on (limit %d)", features::kProcessPerSiteMainFrameThreshold.Get()));
}

void ProcessInternalsHandlerImpl::GetUserTriggeredIsolatedOrigins(
    GetUserTriggeredIsolatedOriginsCallback callback) {
  // Retrieve serialized user-triggered isolated origins for the current
  // profile (i.e., profile from which chrome://process-internals is shown).
  // Note that this may differ from the list of stored user-triggered isolated
  // origins if the user clears browsing data.  Clearing browsing data clears
  // stored isolated origins right away, but the corresponding origins in
  // ChildProcessSecurityPolicy will stay active until next restart, and hence
  // they will still be present in this list.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  std::vector<std::string> serialized_origins;
  for (const auto& origin : policy->GetIsolatedOrigins(
           IsolatedOriginSource::USER_TRIGGERED, browser_context_)) {
    serialized_origins.push_back(origin.Serialize());
  }
  std::move(callback).Run(std::move(serialized_origins));
}

void ProcessInternalsHandlerImpl::GetWebTriggeredIsolatedOrigins(
    GetWebTriggeredIsolatedOriginsCallback callback) {
  // Retrieve serialized user-triggered isolated origins for the current
  // profile (i.e., profile from which chrome://process-internals is shown).
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  std::vector<std::string> serialized_origins;
  for (const auto& origin : policy->GetIsolatedOrigins(
           IsolatedOriginSource::WEB_TRIGGERED, browser_context_)) {
    serialized_origins.push_back(origin.Serialize());
  }
  std::move(callback).Run(std::move(serialized_origins));
}

void ProcessInternalsHandlerImpl::GetGloballyIsolatedOrigins(
    GetGloballyIsolatedOriginsCallback callback) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  std::vector<::mojom::IsolatedOriginInfoPtr> origins;

  // The following global isolated origin sources are safe to show to the user.
  // Any new sources should only be added here if they are ok to be shown on
  // chrome://process-internals.
  for (IsolatedOriginSource source :
       {IsolatedOriginSource::BUILT_IN, IsolatedOriginSource::COMMAND_LINE,
        IsolatedOriginSource::FIELD_TRIAL, IsolatedOriginSource::POLICY,
        IsolatedOriginSource::TEST}) {
    for (const auto& origin : policy->GetIsolatedOrigins(source)) {
      auto info = ::mojom::IsolatedOriginInfo::New();
      info->origin = origin.Serialize();
      info->source = IsolatedOriginSourceToString(source);
      origins.push_back(std::move(info));
    }
  }

  std::move(callback).Run(std::move(origins));
}

void ProcessInternalsHandlerImpl::GetAllWebContentsInfo(
    GetAllWebContentsInfoCallback callback) {
  std::vector<::mojom::WebContentsInfoPtr> infos;
  std::vector<WebContentsImpl*> all_contents =
      WebContentsImpl::GetAllWebContents();

  for (WebContentsImpl* web_contents : all_contents) {
    // Do not return WebContents that don't belong to the current
    // BrowserContext to avoid leaking data between contexts.
    if (web_contents->GetBrowserContext() != browser_context_)
      continue;

    auto info = ::mojom::WebContentsInfo::New();
    info->title = base::UTF16ToUTF8(web_contents->GetTitle());
    info->root_frame = RenderFrameHostToFrameInfo(
        web_contents, web_contents->GetPrimaryMainFrame(),
        ::mojom::FrameInfo::Type::kActive);

    // Retrieve all root frames from bfcache as well.
    NavigationControllerImpl& controller = web_contents->GetController();
    const auto& entries = controller.GetBackForwardCache().GetEntries();
    for (const auto& entry : entries) {
      info->bfcached_root_frames.push_back(RenderFrameHostToFrameInfo(
          web_contents, (*entry).render_frame_host(),
          ::mojom::FrameInfo::Type::kBackForwardCache));
    }

    // Retrieve prerendering root frames.
    web_contents->ForEachRenderFrameHost(
        [web_contents, &prerender_root_frames = info->prerender_root_frames](
            RenderFrameHostImpl* rfh) {
          CollectPrerenders(web_contents, rfh, prerender_root_frames);
        });

    infos.push_back(std::move(info));
  }

  std::move(callback).Run(std::move(infos));
}

}  // namespace content
