// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_group_manager.h"

#include "base/feature_list.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/features.h"
#include "content/public/browser/render_process_host.h"

namespace content {

SiteInstanceGroupManager::SiteInstanceGroupManager() = default;
SiteInstanceGroupManager::~SiteInstanceGroupManager() {
  ClearDefaultProcess();
}

RenderProcessHost* SiteInstanceGroupManager::GetExistingGroupProcess(
    SiteInstanceImpl* site_instance) {
  if (!base::FeatureList::IsEnabled(
          features::kProcessSharingWithStrictSiteInstances) ||
      !default_process_) {
    return nullptr;
  }

  if (site_instance->RequiresDedicatedProcess() ||
      !RenderProcessHostImpl::MayReuseAndIsSuitable(default_process_,
                                                    site_instance)) {
    return nullptr;
  }

  return default_process_;
}

void SiteInstanceGroupManager::OnSiteInfoSet(SiteInstanceImpl* site_instance,
                                             bool has_process) {
  if (!default_process_ && has_process)
    MaybeSetDefaultProcess(site_instance);
}

void SiteInstanceGroupManager::OnProcessSet(SiteInstanceImpl* site_instance) {
  if (default_process_) {
    if (RenderProcessHostImpl::MayReuseAndIsSuitable(default_process_,
                                                     site_instance)) {
      // Make sure the default process was actually used if it is appropriate
      // for this SiteInstance.
      DCHECK_EQ(site_instance->GetProcess(), default_process_);
    }
    return;
  }

  MaybeSetDefaultProcess(site_instance);
}

void SiteInstanceGroupManager::MaybeSetDefaultProcess(
    SiteInstanceImpl* site_instance) {
  if (!base::FeatureList::IsEnabled(
          features::kProcessSharingWithStrictSiteInstances)) {
    return;
  }

  // Wait until this SiteInstance both has a site and a process
  // assigned, so that we can be sure that RequiresDedicatedProcess()
  // is accurate and we actually have a process to set.
  DCHECK(site_instance->HasProcess());
  if (!site_instance->HasSite() || site_instance->RequiresDedicatedProcess())
    return;

  DCHECK(!default_process_);
  default_process_ = site_instance->GetProcess();
  default_process_->AddObserver(this);
}

void SiteInstanceGroupManager::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_EQ(default_process_, host);
  // Only clear the default process if the RenderProcessHost object goes away,
  // not if the renderer process goes away while the RenderProcessHost remains.
  ClearDefaultProcess();
}

void SiteInstanceGroupManager::ClearDefaultProcess() {
  if (!default_process_)
    return;

  default_process_->RemoveObserver(this);
  default_process_ = nullptr;
}

}  // namespace content
