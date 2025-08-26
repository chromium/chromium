// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/local_muter.h"

#include <utility>

#include "base/functional/bind.h"

namespace audio {

LocalMuter::LocalMuter(LoopbackCoordinator* coordinator,
                       const base::UnguessableToken& group_id)
    : loopback_group_observer_(
          LoopbackGroupObserver::CreateMatchingGroupObserver(coordinator,
                                                             group_id)),
      group_id_(group_id) {
  loopback_group_observer_->StartObserving(this);
  loopback_group_observer_->ForEachSource(
      base::BindRepeating(&LocalMuter::OnSourceAdded, base::Unretained(this)));

  receivers_.set_disconnect_handler(
      base::BindRepeating(&LocalMuter::OnBindingLost, base::Unretained(this)));
}

LocalMuter::~LocalMuter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Un-mute all members of the group this muter was responsible for.
  loopback_group_observer_->ForEachSource(base::BindRepeating(
      [](LoopbackSource* source) { source->StopMuting(); }));
  loopback_group_observer_->StopObserving();
}

void LocalMuter::SetAllBindingsLostCallback(base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  all_bindings_lost_callback_ = callback;
}

void LocalMuter::AddReceiver(
    mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver));
}

void LocalMuter::OnBindingLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!HasReceivers()) {
    all_bindings_lost_callback_.Run();
  }
}

void LocalMuter::OnSourceAdded(LoopbackSource* source) {
  // Start muting each new source added to the group.
  source->StartMuting();
}

void LocalMuter::OnSourceRemoved(LoopbackSource* source) {
  // This looks like a potential bug, but the existing behavior is to do
  // nothing when a source leaves the group. Probably because this
  // normally happens when the audio stream is being destroyed.
}

}  // namespace audio
