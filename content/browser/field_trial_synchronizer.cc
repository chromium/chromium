// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/field_trial_synchronizer.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/metrics/runtime_field_trial_overrides.h"
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
#include "content/public/browser/storage_partition.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

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
  base::RuntimeFieldTrialOverrides::GetInstance()->AddObserver(this);
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
  absl::flat_hash_set<BrowserContext*> browser_contexts;
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    UpdateRendererVariationsHeader(host);
    if (auto* context = host->GetBrowserContext()) {
      browser_contexts.insert(context);
    }
  }

  // Also update the variations headers for all storage partitions, as this is
  // needed for attaching the variations header in the reporting API requests.
  for (BrowserContext* context : browser_contexts) {
    variations::VariationsClient* client = context->GetVariationsClient();
    if (!client || client->IsOffTheRecord()) {
      continue;
    }
    context->ForEachLoadedStoragePartition([&](StoragePartition* partition) {
      partition->GetNetworkContext()->SetVariationsHeaders(
          client->GetVariationsHeaders());
    });
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

void FieldTrialSynchronizer::OnRuntimeFieldTrialOverride(
    const base::RuntimeFieldTrialOverrides::RuntimeOverrideInfo& override_info,
    std::string_view previous_override_trial_name) {
  // Runtime FieldTrial Overrides only happen on the main/UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // When an override is applied, the overridden trial and/or the previous
  // override should be removed from the persistent data.
  if (override_info.overridden_trial) {
    metrics::GlobalPersistentSystemProfile::GetInstance()->RemoveFieldTrial(
        override_info.overridden_trial->trial_name());
  }
  if (!previous_override_trial_name.empty()) {
    // Note that if a `previous_override_trial_name` is specified, we don't need
    // to remove `overridden_trial` above since it should have been removed
    // already, so the above removal is technically redundant.
    metrics::GlobalPersistentSystemProfile::GetInstance()->RemoveFieldTrial(
        previous_override_trial_name);
  }

  // Add the new override to the persistent data. This must be done after the
  // previous override is removed, since the `previous_override_trial_name` may
  // be the same name as the new override (in which case, if the order was
  // reversed, the new override would be removed immediately after being added).
  metrics::GlobalPersistentSystemProfile::GetInstance()->AddFieldTrial(
      override_info.trial_name, override_info.group_name);

  // TODO(crbug.com/482449878): Notify renderers of the new override for
  // reporting purposes (e.g. crash keys).
}

// static
void FieldTrialSynchronizer::DeleteInstanceForTesting() {
  if (g_instance) {
    delete g_instance;
  }
}

// static
void FieldTrialSynchronizer::RegisterPersistentAllocatorForTesting(
    base::PersistentMemoryAllocator* memory_allocator) {
  metrics::GlobalPersistentSystemProfile::GetInstance()
      ->RegisterPersistentAllocator(memory_allocator);
}

// static
void FieldTrialSynchronizer::DeregisterPersistentAllocatorForTesting(
    base::PersistentMemoryAllocator* memory_allocator) {
  metrics::GlobalPersistentSystemProfile::GetInstance()
      ->DeregisterPersistentAllocator(memory_allocator);
}

// static
void FieldTrialSynchronizer::SetSystemProfileForTesting(
    const std::string& serialized_profile,
    bool complete) {
  metrics::GlobalPersistentSystemProfile::GetInstance()->SetSystemProfile(
      serialized_profile, complete);
}

FieldTrialSynchronizer::~FieldTrialSynchronizer() {
  base::FieldTrialListIncludingLowAnonymity::RemoveObserver(this);
  variations::VariationsIdsProvider::GetInstance()->RemoveObserver(this);
  base::RuntimeFieldTrialOverrides::GetInstance()->RemoveObserver(this);
  g_instance = nullptr;
}

}  // namespace content
