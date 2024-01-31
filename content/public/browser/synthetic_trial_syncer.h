// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SYNTHETIC_TRIAL_SYNCER_H_
#define CONTENT_PUBLIC_BROWSER_SYNTHETIC_TRIAL_SYNCER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/variations/synthetic_trials.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace variations {
class SyntheticTrialRegistry;
}  // namespace variations

namespace content {

namespace mojom {
class SyntheticTrialConfiguration;
}  // namespace mojom

// This class is used by the browser process to tell child processes
// what synthetic trial groups the browser process joins in.
//
// This class registers itself as an observer of SyntheticTrialObserver.
// SyntheticTrialRegistry notifies this class when a synthetic trial group
// is updated.
//
// This class also registers itself as BrowserChildProcessObserver,
// RenderProcessHostCreationObserver and RenderProcessHostObserver to
// tell the synthetic trial groups just after a child process is created.
// At that time, this class gets all joined synthetic groups by calling
// SyntheticTrialRegistry::GetSyntheticTrialGroups().
class CONTENT_EXPORT SyntheticTrialSyncer
    : public variations::SyntheticTrialObserver,
      public BrowserChildProcessObserver,
      public RenderProcessHostCreationObserver,
      public RenderProcessHostObserver {
 public:
  static std::unique_ptr<SyntheticTrialSyncer> Create(
      variations::SyntheticTrialRegistry* registry);

  explicit SyntheticTrialSyncer(variations::SyntheticTrialRegistry* registry);
  ~SyntheticTrialSyncer() override;

  SyntheticTrialSyncer(const SyntheticTrialSyncer&) = delete;
  SyntheticTrialSyncer(SyntheticTrialSyncer&&) = delete;
  SyntheticTrialSyncer& operator=(const SyntheticTrialSyncer&) = delete;

 private:
  void OnDisconnected(int unique_child_process_id);

  // variations::SyntheticTrialObserver:
  void OnSyntheticTrialsChanged(
      const std::vector<variations::SyntheticTrialGroup>& trials_updated,
      const std::vector<variations::SyntheticTrialGroup>& trials_removed,
      const std::vector<variations::SyntheticTrialGroup>& groups) override;

  // BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) override;

  // RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(RenderProcessHost* host) override;

  // RenderProcessHostObserver:
  void RenderProcessReady(RenderProcessHost* host) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  const raw_ptr<variations::SyntheticTrialRegistry> registry_;

  // A map: child process' unique id to the mojo connection.
  // The keys are unique id-s created by
  // ChildProcessHostImpl::GenerateChildProcessUniqueId().
  //
  // Since the number of child precesses is basically not large, use
  // base::flat_map for the map.
  base::flat_map<int, mojo::Remote<mojom::SyntheticTrialConfiguration>>
      child_process_unique_id_to_mojo_connections_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SYNTHETIC_TRIAL_SYNCER_H_
