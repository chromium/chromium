// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/field_trial_synchronizer.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/strings/strcat.h"
#include "base/threading/thread.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/variations_client.h"
#include "content/common/renderer_variations_configuration.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

namespace {

FieldTrialSynchronizer* g_instance = nullptr;

// Notifies all renderer processes about the |group_name| that is finalized for
// the given field trail (|field_trial_name|). This is called on UI thread.
void NotifyAllRenderersOfFieldTrial(const std::string& field_trial_name,
                                    const std::string& group_name,
                                    bool is_low_anonymity,
                                    bool is_overridden) {
  // To iterate over RenderProcessHosts, or to send messages to the hosts, we
  // need to be on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Low anonymity or overridden field trials must not be written to persistent
  // data, otherwise they might end up being logged in metrics.
  //
  // TODO(crbug.com/40263398): split this out into a separate class that
  // registers using |FieldTrialList::AddObserver()| (and so doesn't get told
  // about low anonymity trials at all).
  if (!is_low_anonymity) {
    // Note this in the persistent profile as it will take a while for a new
    // "complete" profile to be generated.
    metrics::GlobalPersistentSystemProfile::GetInstance()->AddFieldTrial(
        field_trial_name,
        is_overridden ? base::StrCat({group_name, variations::kOverrideSuffix})
                      : group_name);
  }

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

}  // namespace

// static
void FieldTrialSynchronizer::CreateInstance() {
  // Only 1 instance is allowed per process.
  DCHECK(!g_instance);
  g_instance = new FieldTrialSynchronizer();
}

FieldTrialSynchronizer::FieldTrialSynchronizer() {
  // TODO(crbug.com/40263398): consider whether there is a need to exclude low
  // anonymity field trials from non-browser processes (or to plumb through the
  // anonymity property for more fine-grained access).
  bool success = base::FieldTrialListIncludingLowAnonymity::AddObserver(this);
  // Ensure the observer was actually registered.
  DCHECK(success);

  variations::VariationsIdsProvider::GetInstance()->AddObserver(this);
  NotifyAllRenderersOfVariationsHeader();
}

void FieldTrialSynchronizer::OnFieldTrialGroupFinalized(
    const base::FieldTrial& trial,
    const std::string& group_name) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    NotifyAllRenderersOfFieldTrial(trial.trial_name(), group_name,
                                   trial.is_low_anonymity(),
                                   trial.IsOverridden());
  } else {
    // Note that in some tests, `trial` may not be alive when the posted task is
    // called.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&NotifyAllRenderersOfFieldTrial, trial.trial_name(),
                       group_name, trial.is_low_anonymity(),
                       trial.IsOverridden()));
  }
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
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FieldTrialSynchronizer::NotifyAllRenderersOfVariationsHeader));
}

FieldTrialSynchronizer::~FieldTrialSynchronizer() {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace content
