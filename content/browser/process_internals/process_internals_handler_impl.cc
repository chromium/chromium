// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_internals/process_internals_handler_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

namespace {

using IsolatedOriginSource = ChildProcessSecurityPolicy::IsolatedOriginSource;

::mojom::FrameInfoPtr RenderFrameHostToFrameInfo(RenderFrameHostImpl* frame,
                                                 bool is_bfcached) {
  auto frame_info = ::mojom::FrameInfo::New();

  frame_info->routing_id = frame->GetRoutingID();
  frame_info->process_id = frame->GetProcess()->GetID();
  frame_info->last_committed_url =
      frame->GetLastCommittedURL().is_valid()
          ? base::make_optional(frame->GetLastCommittedURL())
          : base::nullopt;
  frame_info->is_bfcached = is_bfcached;

  SiteInstanceImpl* site_instance =
      static_cast<SiteInstanceImpl*>(frame->GetSiteInstance());
  frame_info->site_instance = ::mojom::SiteInstanceInfo::New();
  frame_info->site_instance->id = site_instance->GetId();

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  frame_info->site_instance->locked =
      policy->GetProcessLock(site_instance->GetProcess()->GetID())
          .is_locked_to_site();

  frame_info->site_instance->site_url =
      site_instance->HasSite()
          ? base::make_optional(site_instance->GetSiteInfo().site_url())
          : base::nullopt;

  // Only send a process lock URL if it's different from the site URL.  In the
  // common case they are the same, so we avoid polluting the UI with two
  // identical URLs.
  bool should_show_lock_url = frame_info->site_instance->locked &&
                              site_instance->GetSiteInfo().process_lock_url() !=
                                  site_instance->GetSiteInfo().site_url();
  frame_info->site_instance->process_lock_url =
      should_show_lock_url
          ? base::make_optional(site_instance->GetSiteInfo().process_lock_url())
          : base::nullopt;

  frame_info->site_instance->is_origin_keyed =
      site_instance->GetSiteInfo().is_origin_keyed();

  for (size_t i = 0; i < frame->child_count(); ++i) {
    frame_info->subframes.push_back(RenderFrameHostToFrameInfo(
        frame->child_at(i)->current_frame_host(), is_bfcached));
  }

  return frame_info;
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
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

ProcessInternalsHandlerImpl::ProcessInternalsHandlerImpl(
    BrowserContext* browser_context,
    mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver)
    : browser_context_(browser_context), receiver_(this, std::move(receiver)) {}

ProcessInternalsHandlerImpl::~ProcessInternalsHandlerImpl() = default;

void ProcessInternalsHandlerImpl::GetIsolationMode(
    GetIsolationModeCallback callback) {
  std::vector<base::StringPiece> modes;
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    modes.push_back("Site Per Process");
  if (SiteIsolationPolicy::AreIsolatedOriginsEnabled())
    modes.push_back("Isolate Origins");
  if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled())
    modes.push_back("Strict Origin Isolation");

  // Retrieve any additional site isolation modes controlled by the embedder.
  std::vector<std::string> additional_modes =
      GetContentClient()->browser()->GetAdditionalSiteIsolationModes();
  std::move(additional_modes.begin(), additional_modes.end(),
            std::back_inserter(modes));

  std::move(callback).Run(modes.empty() ? "Disabled"
                                        : base::JoinString(modes, ", "));
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
    info->root_frame =
        RenderFrameHostToFrameInfo(web_contents->GetMainFrame(), false);

    // Retrieve all root frames from bfcache as well.
    NavigationControllerImpl& controller = web_contents->GetController();
    const auto& entries = controller.GetBackForwardCache().GetEntries();
    for (const auto& entry : entries) {
      info->bfcached_root_frames.push_back(
          RenderFrameHostToFrameInfo((*entry).render_frame_host.get(), true));
    }

    infos.push_back(std::move(info));
  }

  std::move(callback).Run(std::move(infos));
}

}  // namespace content
