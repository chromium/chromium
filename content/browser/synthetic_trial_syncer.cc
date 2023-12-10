// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/synthetic_trial_syncer.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/common/synthetic_trial_configuration.mojom.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

namespace {

std::vector<mojom::SyntheticTrialGroupPtr> ConvertTrialGroupsToMojo(
    const std::vector<variations::SyntheticTrialGroup>& trials) {
  std::vector<mojom::SyntheticTrialGroupPtr> groups;
  for (const auto& trial : trials) {
    std::string trial_name(trial.trial_name());
    std::string group_name(trial.group_name());
    groups.push_back(mojom::SyntheticTrialGroup::New(trial_name, group_name));
  }
  return groups;
}

void NotifyChildProcess(
    mojo::Remote<mojom::SyntheticTrialConfiguration>&
        synthetic_trial_configuration,
    const std::vector<variations::SyntheticTrialGroup>& trials_updated,
    const std::vector<variations::SyntheticTrialGroup>& trials_removed) {
  synthetic_trial_configuration->AddOrUpdateSyntheticTrialGroups(
      ConvertTrialGroupsToMojo(trials_updated));
  synthetic_trial_configuration->RemoveSyntheticTrialGroups(
      ConvertTrialGroupsToMojo(trials_removed));
}

class RenderProcessIterator {
 public:
  using HostType = RenderProcessHost;

  RenderProcessIterator() : iter_(RenderProcessHost::AllHostsIterator()) {}

  bool IsAtEnd() { return iter_.IsAtEnd(); }

  void Advance() { iter_.Advance(); }

  const base::Process& GetProcess() {
    return iter_.GetCurrentValue()->GetProcess();
  }

  HostType* GetHost() { return iter_.GetCurrentValue(); }

 private:
  RenderProcessHost::iterator iter_;
};

class NonRenderProcessIterator {
 public:
  using HostType = ChildProcessHost;

  NonRenderProcessIterator() = default;

  bool IsAtEnd() { return iter_.Done(); }

  void Advance() { ++iter_; }

  const base::Process& GetProcess() { return iter_.GetData().GetProcess(); }

  HostType* GetHost() { return iter_.GetHost(); }

 private:
  BrowserChildProcessHostIterator iter_;
};

template <typename Iterator>
void NotifySyntheticTrialsChange(
    base::ProcessId process_id,
    const std::vector<variations::SyntheticTrialGroup>& trials_updated,
    const std::vector<variations::SyntheticTrialGroup>& trials_removed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ProcessId current_pid = base::Process::Current().Pid();
  for (Iterator iter; !iter.IsAtEnd(); iter.Advance()) {
    const base::Process& process = iter.GetProcess();
    if (!process.IsValid() ||
        (process_id != base::kNullProcessId && process_id != process.Pid())) {
      continue;
    }

    // Skip if in-browser process mode, because the browser process
    // manages synthetic trial groups properly.
    if (process.Pid() == current_pid) {
      continue;
    }

    mojo::Remote<mojom::SyntheticTrialConfiguration>
        synthetic_trial_configuration;
    iter.GetHost()->BindReceiver(
        synthetic_trial_configuration.BindNewPipeAndPassReceiver());
    NotifyChildProcess(synthetic_trial_configuration, trials_updated,
                       trials_removed);
  }
}

}  // namespace

std::unique_ptr<SyntheticTrialSyncer> SyntheticTrialSyncer::Create(
    variations::SyntheticTrialRegistry* registry) {
  // Only 1 instance is allowed for the browser process.
  static bool s_called = false;
  CHECK(!s_called);
  std::unique_ptr<SyntheticTrialSyncer> instance =
      std::make_unique<SyntheticTrialSyncer>(registry);
  registry->AddObserver(instance.get());
  BrowserChildProcessObserver::Add(instance.get());
  s_called = true;
  return instance;
}

SyntheticTrialSyncer::SyntheticTrialSyncer(
    variations::SyntheticTrialRegistry* registry)
    : registry_(registry) {}

SyntheticTrialSyncer::~SyntheticTrialSyncer() {
  registry_->RemoveObserver(this);
  BrowserChildProcessObserver::Remove(this);

  for (RenderProcessIterator it; !it.IsAtEnd(); it.Advance()) {
    if (it.GetHost()) {
      it.GetHost()->RemoveObserver(this);
    }
  }
}

void SyntheticTrialSyncer::OnSyntheticTrialsChanged(
    const std::vector<variations::SyntheticTrialGroup>& trials_updated,
    const std::vector<variations::SyntheticTrialGroup>& trials_removed,
    const std::vector<variations::SyntheticTrialGroup>& groups) {
  NotifySyntheticTrialsChange<RenderProcessIterator>(
      base::kNullProcessId, trials_updated, trials_removed);
  NotifySyntheticTrialsChange<NonRenderProcessIterator>(
      base::kNullProcessId, trials_updated, trials_removed);
}

void SyntheticTrialSyncer::BrowserChildProcessLaunchedAndConnected(
    const ChildProcessData& data) {
  if (!data.GetProcess().IsValid()) {
    return;
  }

  NotifySyntheticTrialsChange<NonRenderProcessIterator>(
      data.GetProcess().Pid(), registry_->GetSyntheticTrialGroups(), {});
}

void SyntheticTrialSyncer::OnRenderProcessHostCreated(RenderProcessHost* host) {
  host->AddObserver(this);
}

void SyntheticTrialSyncer::RenderProcessReady(RenderProcessHost* host) {
  const base::Process& process = host->GetProcess();
  if (!process.IsValid()) {
    return;
  }

  NotifySyntheticTrialsChange<RenderProcessIterator>(
      process.Pid(), registry_->GetSyntheticTrialGroups(), {});
}

void SyntheticTrialSyncer::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  host->RemoveObserver(this);
}

void SyntheticTrialSyncer::RenderProcessHostDestroyed(RenderProcessHost* host) {
  // To ensure this is removed from the observer list, call RemoveObserver()
  // again.
  host->RemoveObserver(this);
}

}  // namespace content
