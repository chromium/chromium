// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIELD_TRIAL_SYNCHRONIZER_H_
#define CONTENT_BROWSER_FIELD_TRIAL_SYNCHRONIZER_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "components/variations/variations_ids_provider.h"

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
class FieldTrialSynchronizer
    : public base::FieldTrialList::Observer,
      public variations::VariationsIdsProvider::Observer {
 public:
  // Creates the global FieldTrialSynchronizer instance for this process. After
  // this is invoked, renderers are notified whenever a field trial group is
  // finalized.
  static void CreateInstance();

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

  // Sends the current variations header to |host|'s renderer.
  static void UpdateRendererVariationsHeader(RenderProcessHost* host);

 private:
  FieldTrialSynchronizer();
  ~FieldTrialSynchronizer() override;

  static void NotifyAllRenderersOfVariationsHeader();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIELD_TRIAL_SYNCHRONIZER_H_
