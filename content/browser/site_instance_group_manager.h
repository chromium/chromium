// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_GROUP_MANAGER_H_
#define CONTENT_BROWSER_SITE_INSTANCE_GROUP_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

class RenderProcessHost;
class SiteInstanceImpl;

// Policy class that manages groups of SiteInstances and controls whether
// they share a process and/or need to use proxies to communicate with each
// other. This decouples the process model from the SiteInfo granularity used
// by SiteInstances. There are three supported modes:
// 1) SiteInstances placed in the same group will share a process and will not
//    use proxies to communicate with other members of the same group.
// 2) SiteInstances can be placed in different groups, but still share a
//    process. These SiteInstances will use proxies to communicate with frames
//    in the same process as well as cross process frames.
// 3) SiteInstances that require a dedicated process will always be placed in
//    their own group and given a process that is not shared with other
//    principals (i.e., SiteInfos).
//
// This policy object is owned by a BrowsingInstance and the groups it manages
// only contain SiteInstances associated with that BrowsingInstance.
//
// TODO: Update description to include details about when/how SiteInstances
// get assigned to groups. Currently this object only holds the logic for the
// 'default process' model which implements mode 2 mentioned above for any
// SiteInstance that does not require a dedicated process.
class SiteInstanceGroupManager final : private RenderProcessHostObserver {
 public:
  SiteInstanceGroupManager();
  ~SiteInstanceGroupManager() override;

  // Returns a process that can be assigned to `site_instance`. This may be
  // the process for an existing group the SiteInstance is assigned to, or
  // it could be the default process if that process mode is enabled and the
  // process is suitable.
  RenderProcessHost* GetExistingGroupProcess(SiteInstanceImpl* site_instance);

  // Called when the SiteInfo is set on `site_instance`. This is used to
  // discover new SiteInstances when they are assigned a specific security
  // principal so that they can be assigned to an existing group if appropriate.
  // `has_process` is set to true if the SiteInstance has a process assigned
  // to it already. This signal is used to determine if the process
  // assigned to the SiteInstance could potentially be used by other groups
  // with similar properties.
  void OnSiteInfoSet(SiteInstanceImpl* site_instance, bool has_process);

  // Called when a process gets assigned to a SiteInstance. This
  // is used to discover new processes that are created for a SiteInstance by
  // RenderProcessHostImpl. It provides a hook for discovering the process that
  // actually gets assigned to a specific group, and provides candidates for
  // selecting an appropriate default process.
  void OnProcessSet(SiteInstanceImpl* site_instance);

  RenderProcessHost* default_process() { return default_process_; }

 private:
  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Evaluates the process assigned to `site_instance` and determines if it is
  // suitable to be the default process. If suitable, it keeps a reference
  // in `default_process_` so it can be used for future GetProcess() calls.
  void MaybeSetDefaultProcess(SiteInstanceImpl* site_instance);

  // Removes observer for `default_process_` and sets the field to nullptr.
  void ClearDefaultProcess();

  // The process to use for any SiteInstance in this BrowsingInstance that
  // doesn't require a dedicated process.
  raw_ptr<RenderProcessHost> default_process_ = nullptr;
};
}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_GROUP_MANAGER_H_
