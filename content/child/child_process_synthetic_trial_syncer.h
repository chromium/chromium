// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_SYNTHETIC_TRIAL_SYNCER_H_
#define CONTENT_CHILD_CHILD_PROCESS_SYNTHETIC_TRIAL_SYNCER_H_

#include <string>
#include <vector>

#include "components/variations/synthetic_trials.h"
#include "content/common/content_export.h"
#include "content/common/synthetic_trial_configuration.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

// This class works in child processes and receives synthetic trial groups
// from SyntheticTrialSyncer running in the browser process via mojo.
//
// When receiving any message from SyntheticTrialSyncer, this class updates
// synthetic trial groups and updates crash keys with synthetic trials.
// This makes crash dumps from non-browser processes have synthetic trial
// information.
class CONTENT_EXPORT ChildProcessSyntheticTrialSyncer
    : public mojom::SyntheticTrialConfiguration {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::SyntheticTrialConfiguration> receiver);

  ChildProcessSyntheticTrialSyncer();
  ~ChildProcessSyntheticTrialSyncer() override;

  ChildProcessSyntheticTrialSyncer(const ChildProcessSyntheticTrialSyncer&) =
      delete;
  ChildProcessSyntheticTrialSyncer& operator=(
      const ChildProcessSyntheticTrialSyncer&) = delete;
  ChildProcessSyntheticTrialSyncer(ChildProcessSyntheticTrialSyncer&&) = delete;

 private:
  friend class ChildProcessSyntheticTrialSyncerTest;

  static std::unique_ptr<ChildProcessSyntheticTrialSyncer>
  CreateInstanceForTesting();

  // mojom::SyntheticTrialConfiguration:
  void AddOrUpdateSyntheticTrialGroups(
      std::vector<mojom::SyntheticTrialGroupPtr> trial_groups) override;
  void RemoveSyntheticTrialGroups(
      std::vector<mojom::SyntheticTrialGroupPtr> trial_groups) override;

  void AddOrUpdateTrialGroupInternal(const std::string& trial_name,
                                     const std::string& group_name);

  std::vector<variations::SyntheticTrialGroup> trials_;
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_SYNTHETIC_TRIAL_SYNCER_H_
