// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIELD_TRIAL_SYNCHRONIZER_H_
#define CONTENT_BROWSER_FIELD_TRIAL_SYNCHRONIZER_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/metrics/runtime_field_trial_overrides.h"
#include "components/variations/variations_ids_provider.h"
#include "content/common/content_export.h"

namespace base {
class PersistentMemoryAllocator;
}

namespace content {
class RenderProcessHost;

// This class is used by the browser process to communicate FieldTrial setting
// (field trial name and group) and Variation header to any previously started
// renderers.
//
// This class registers itself as an observer of FieldTrialList. FieldTrialList
// notifies this class by calling its OnFieldTrialGroupFinalized method when a
// group is selected (finalized) for a FieldTrial and OnFieldTrialGroupFinalized
// method sends the FieldTrial's name and the group to all renderer processes.
// Each renderer process creates the FieldTrial, and by using a 100% probability
// for the FieldTrial, forces the FieldTrial to have the same group string. This
// is mostly an optimization so that renderers don't send anything to the
// browser when they know that a trial is already active.
//
// This class also registers itself as a VariationsIdsProvider Observer and
// updates the renderers if the variations header changes.
class CONTENT_EXPORT FieldTrialSynchronizer
    : public base::FieldTrialList::Observer,
      public variations::VariationsIdsProvider::Observer,
      public base::RuntimeFieldTrialOverrides::Observer {
 public:
  // Creates the global FieldTrialSynchronizer instance for this process. After
  // this is invoked, renderers are notified whenever a field trial group is
  // finalized.
  static void CreateInstance();

  // Cleans up the global instance for testing.
  // This is required to unregister observers from global singletons like
  // FieldTrialList and RuntimeFieldTrialOverrides, preventing callbacks from
  // executing after the test's ScopedTaskEnvironment or BrowserTaskExecutor
  // is torn down, which otherwise crashes in debug builds.
  static void DeleteInstanceForTesting();

  // Test-only wrappers to access the local GlobalPersistentSystemProfile
  // instance inside the content component (libcontent.so). In component builds,
  // components/metrics is a source_set, causing the testing binary
  // (content_unittests) and libcontent.so to get separate, hidden copies of the
  // GlobalPersistentSystemProfile singleton. Working with the profile directly
  // inside the test class registers variables on the test copy, while the
  // synchronizer inside the library writes to the library copy (discarding the
  // data). Performing these actions via wrappers in content/browser context
  // ensures we target the correct copy.
  static void RegisterPersistentAllocatorForTesting(
      base::PersistentMemoryAllocator* memory_allocator);
  static void DeregisterPersistentAllocatorForTesting(
      base::PersistentMemoryAllocator* memory_allocator);
  static void SetSystemProfileForTesting(const std::string& serialized_profile,
                                         bool complete);

  FieldTrialSynchronizer(const FieldTrialSynchronizer&) = delete;
  FieldTrialSynchronizer& operator=(const FieldTrialSynchronizer&) = delete;

  // FieldTrialList::Observer methods:

  // This method is called by the FieldTrialList singleton when a trial's group
  // is finalized. This method contacts all renderers (by calling
  // NotifyAllRenderers) to create a FieldTrial that carries the randomly
  // selected state from the browser process into all the renderer processes.
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override;

  // VariationsIdsProvider::Observer methods:
  void VariationIdsHeaderUpdated() override;

  // RuntimeFieldTrialOverrides::Observer methods:
  void OnRuntimeFieldTrialOverride(
      const base::RuntimeFieldTrialOverrides::RuntimeOverrideInfo&
          override_info,
      std::string_view previous_override_trial_name) override;

  // Sends the current variations header to |host|'s renderer.
  static void UpdateRendererVariationsHeader(RenderProcessHost* host);

 private:
  FieldTrialSynchronizer();
  ~FieldTrialSynchronizer() override;

  static void NotifyAllRenderersOfVariationsHeader();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIELD_TRIAL_SYNCHRONIZER_H_
