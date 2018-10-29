// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_internals/process_internals_handler_impl.h"

#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

::mojom::FrameInfoPtr FrameTreeNodeToFrameInfo(FrameTreeNode* ftn) {
  RenderFrameHost* frame = ftn->current_frame_host();
  auto frame_info = ::mojom::FrameInfo::New();

  frame_info->routing_id = frame->GetRoutingID();
  frame_info->process_id = frame->GetProcess()->GetID();
  frame_info->last_committed_url =
      frame->GetLastCommittedURL().is_valid()
          ? base::make_optional(frame->GetLastCommittedURL())
          : base::nullopt;

  SiteInstanceImpl* site_instance =
      static_cast<SiteInstanceImpl*>(frame->GetSiteInstance());
  frame_info->site_instance = ::mojom::SiteInstanceInfo::New();
  frame_info->site_instance->id = site_instance->GetId();

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  frame_info->site_instance->locked =
      !policy->GetOriginLock(site_instance->GetProcess()->GetID()).is_empty();

  frame_info->site_instance->site_url =
      site_instance->HasSite()
          ? base::make_optional(site_instance->GetSiteURL())
          : base::nullopt;

  for (size_t i = 0; i < ftn->child_count(); ++i) {
    frame_info->subframes.push_back(FrameTreeNodeToFrameInfo(ftn->child_at(i)));
  }

  return frame_info;
}

}  // namespace

ProcessInternalsHandlerImpl::ProcessInternalsHandlerImpl(
    BrowserContext* browser_context,
    mojo::InterfaceRequest<::mojom::ProcessInternalsHandler> request)
    : browser_context_(browser_context), binding_(this, std::move(request)) {}

ProcessInternalsHandlerImpl::~ProcessInternalsHandlerImpl() = default;

void ProcessInternalsHandlerImpl::GetIsolationMode(
    GetIsolationModeCallback callback) {
  std::vector<base::StringPiece> modes;
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    modes.push_back("Site Per Process");
  if (SiteIsolationPolicy::AreIsolatedOriginsEnabled())
    modes.push_back("Isolate Origins");

  std::move(callback).Run(modes.empty() ? "Disabled"
                                        : base::JoinString(modes, ", "));
}

void ProcessInternalsHandlerImpl::GetIsolatedOriginsSize(
    GetIsolatedOriginsSizeCallback callback) {
  int size = SiteIsolationPolicy::GetIsolatedOrigins().size();
  std::move(callback).Run(size);
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
        FrameTreeNodeToFrameInfo(web_contents->GetFrameTree()->root());
    infos.push_back(std::move(info));
  }

  std::move(callback).Run(std::move(infos));
}

}  // namespace content
