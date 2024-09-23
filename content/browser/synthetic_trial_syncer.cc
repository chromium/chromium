// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/synthetic_trial_syncer.h"

#include "base/functional/bind.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/common/synthetic_trial_configuration.mojom.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"

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
  if (trials_updated.size() > 0) {
    synthetic_trial_configuration->AddOrUpdateSyntheticTrialGroups(
        ConvertTrialGroupsToMojo(trials_updated));
  }
  if (trials_removed.size() > 0) {
    synthetic_trial_configuration->RemoveSyntheticTrialGroups(
        ConvertTrialGroupsToMojo(trials_removed));
  }
}

ChildProcessHost* FindChildProcessHost(int unique_id) {
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id == unique_id) {
      return iter.GetHost();
    }
  }
  return nullptr;
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

void SyntheticTrialSyncer::OnDisconnected(int unique_child_process_id) {
  child_process_unique_id_to_mojo_connections_.erase(unique_child_process_id);
}

SyntheticTrialSyncer::SyntheticTrialSyncer(
    variations::SyntheticTrialRegistry* registry)
    : registry_(registry) {}

SyntheticTrialSyncer::~SyntheticTrialSyncer() {
  registry_->RemoveObserver(this);
  BrowserChildProcessObserver::Remove(this);

  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    if (RenderProcessHost* host = it.GetCurrentValue()) {
      host->RemoveObserver(this);
    }
  }
}

void SyntheticTrialSyncer::OnSyntheticTrialsChanged(
    const std::vector<variations::SyntheticTrialGroup>& trials_updated,
    const std::vector<variations::SyntheticTrialGroup>& trials_removed,
    const std::vector<variations::SyntheticTrialGroup>& groups) {
  for (auto& it : child_process_unique_id_to_mojo_connections_) {
    NotifyChildProcess(it.second, trials_updated, trials_removed);
  }
}

void SyntheticTrialSyncer::BrowserChildProcessLaunchedAndConnected(
    const ChildProcessData& data) {
  const int unique_id = data.id;
  ChildProcessHost* host = FindChildProcessHost(unique_id);
  if (host == nullptr) {
    return;
  }
  mojo::Remote<mojom::SyntheticTrialConfiguration>
      synthetic_trial_configuration;
  host->BindReceiver(
      synthetic_trial_configuration.BindNewPipeAndPassReceiver());
  // SyntheticTrialSyncer will be destroyed at PostMainMessageLoopRun() while
  // BrowserMainRunner's shutdown. So the disconnect handler task will not
  // be invoked after the destruction.
  synthetic_trial_configuration.set_disconnect_handler(
      base::BindOnce(&SyntheticTrialSyncer::OnDisconnected,
                     base::Unretained(this), unique_id));
  NotifyChildProcess(synthetic_trial_configuration,
                     registry_->GetSyntheticTrialGroups(), {});

  // Keep the mojo connection not to repeatedly create and destroy the child
  // process synthetic trial syncer.
  child_process_unique_id_to_mojo_connections_[unique_id] =
      std::move(synthetic_trial_configuration);
}

void SyntheticTrialSyncer::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  // Since data.GetProcess().IsValid() returns false, we cannot get the child
  // process' pid here.
  child_process_unique_id_to_mojo_connections_.erase(data.id);
}

void SyntheticTrialSyncer::OnRenderProcessHostCreated(RenderProcessHost* host) {
  host->AddObserver(this);
}

void SyntheticTrialSyncer::RenderProcessReady(RenderProcessHost* host) {
  const base::Process& process = host->GetProcess();
  if (!process.IsValid()) {
    return;
  }

  const int unique_id = host->GetID();
  mojo::Remote<mojom::SyntheticTrialConfiguration>
      synthetic_trial_configuration;
  host->BindReceiver(
      synthetic_trial_configuration.BindNewPipeAndPassReceiver());
  // SyntheticTrialSyncer will be destroyed at PostMainMessageLoopRun() while
  // BrowserMainRunner's shutdown. So the disconnect handler task will not
  // be invoked after the destruction.
  synthetic_trial_configuration.set_disconnect_handler(
      base::BindOnce(&SyntheticTrialSyncer::OnDisconnected,
                     base::Unretained(this), unique_id));
  NotifyChildProcess(synthetic_trial_configuration,
                     registry_->GetSyntheticTrialGroups(), {});

  // Keep the mojo connection not to repeatedly create and destroy the child
  // process synthetic trial syncer.
  child_process_unique_id_to_mojo_connections_[unique_id] =
      std::move(synthetic_trial_configuration);
}

void SyntheticTrialSyncer::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  child_process_unique_id_to_mojo_connections_.erase(host->GetID());

  // To ensure this is removed from the observer list, call RemoveObserver()
  // again.
  host->RemoveObserver(this);
}

void SyntheticTrialSyncer::RenderProcessHostDestroyed(RenderProcessHost* host) {
  child_process_unique_id_to_mojo_connections_.erase(host->GetID());

  // To ensure this is removed from the observer list, call RemoveObserver()
  // again.
  host->RemoveObserver(this);
}

}  // namespace content
