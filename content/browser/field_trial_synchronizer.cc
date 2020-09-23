// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/field_trial_synchronizer.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/variations_client.h"
#include "content/common/renderer_variations_configuration.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

namespace {

void AddFieldTrialToPersistentSystemProfile(const std::string& field_trial_name,
                                            const std::string& group_name) {
  // Note this in the persistent profile as it will take a while for a new
  // "complete" profile to be generated.
  metrics::GlobalPersistentSystemProfile::GetInstance()->AddFieldTrial(
      field_trial_name, group_name);
}

}  // namespace

FieldTrialSynchronizer::FieldTrialSynchronizer() {
  bool success = base::FieldTrialList::AddObserver(this);
  // Ensure the observer was actually registered.
  DCHECK(success);

  variations::VariationsIdsProvider::GetInstance()->AddObserver(this);
  NotifyAllRenderersOfVariationsHeader();
}

void FieldTrialSynchronizer::NotifyAllRenderersOfFieldTrial(
    const std::string& field_trial_name,
    const std::string& group_name) {
  // To iterate over RenderProcessHosts, or to send messages to the hosts, we
  // need to be on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  AddFieldTrialToPersistentSystemProfile(field_trial_name, group_name);

  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    auto* host = it.GetCurrentValue();
    IPC::ChannelProxy* channel = host->GetChannel();
    // channel might be null in tests.
    if (host->IsInitializedAndNotDead() && channel) {
      mojo::AssociatedRemote<mojom::RendererVariationsConfiguration>
          renderer_variations_configuration;
      channel->GetRemoteAssociatedInterface(&renderer_variations_configuration);
      renderer_variations_configuration->SetFieldTrialGroup(field_trial_name,
                                                            group_name);
    }
  }
}

void FieldTrialSynchronizer::OnFieldTrialGroupFinalized(
    const std::string& field_trial_name,
    const std::string& group_name) {
  // The FieldTrialSynchronizer may have been created before any BrowserThread
  // is created, so we don't need to synchronize with child processes in which
  // case there are no child processes to notify yet. But we want to update the
  // persistent system profile, thus the histogram data recorded in the reduced
  // mode will be tagged to its corresponding field trial experiment.
  if (!BrowserThread::IsThreadInitialized(BrowserThread::UI)) {
    AddFieldTrialToPersistentSystemProfile(field_trial_name, group_name);
    return;
  }

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&FieldTrialSynchronizer::NotifyAllRenderersOfFieldTrial,
                     this, field_trial_name, group_name));
}

// static
void FieldTrialSynchronizer::NotifyAllRenderersOfVariationsHeader() {
  // To iterate over RenderProcessHosts, or to send messages to the hosts, we
  // need to be on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    UpdateRendererVariationsHeader(it.GetCurrentValue());
  }
}

// static
void FieldTrialSynchronizer::UpdateRendererVariationsHeader(
    RenderProcessHost* host) {
  if (!host->IsInitializedAndNotDead())
    return;

  IPC::ChannelProxy* channel = host->GetChannel();

  // |channel| might be null in tests.
  if (!channel)
    return;

  variations::VariationsClient* client =
      host->GetBrowserContext()->GetVariationsClient();

  // |client| might be null in tests.
  if (!client || client->IsOffTheRecord())
    return;

  mojo::AssociatedRemote<mojom::RendererVariationsConfiguration>
      renderer_variations_configuration;
  channel->GetRemoteAssociatedInterface(&renderer_variations_configuration);
  renderer_variations_configuration->SetVariationsHeaders(
      client->GetVariationsHeaders());
}

void FieldTrialSynchronizer::VariationIdsHeaderUpdated() {
  // PostTask to avoid recursive lock.
  base::PostTask(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &FieldTrialSynchronizer::NotifyAllRenderersOfVariationsHeader));
}

FieldTrialSynchronizer::~FieldTrialSynchronizer() {
  base::FieldTrialList::RemoveObserver(this);
  variations::VariationsIdsProvider::GetInstance()->RemoveObserver(this);
}

}  // namespace content
