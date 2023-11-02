// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/field_trial_recorder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/field_trial.h"
#include "content/common/field_trial_recorder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

FieldTrialRecorder::FieldTrialRecorder() = default;

FieldTrialRecorder::~FieldTrialRecorder() = default;

// static
void FieldTrialRecorder::Create(
    mojo::PendingReceiver<mojom::FieldTrialRecorder> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FieldTrialRecorder>(),
                              std::move(receiver));
}

void FieldTrialRecorder::FieldTrialActivated(const std::string& trial_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Activate the trial in the browser process to match its state in the
  // renderer. This is done by calling FindFullName which finalizes the group
  // and activates the trial.
  base::FieldTrialList::FindFullName(trial_name);
}

}  // namespace content
