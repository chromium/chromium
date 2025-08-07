// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/local_muter.h"

#include <utility>

#include "base/functional/bind.h"

namespace audio {

namespace {

void StartMutingSource(LoopbackSource* source) {
  source->StartMuting();
}

void StopMutingSource(LoopbackSource* source) {
  source->StopMuting();
}

}  // namespace

LocalMuter::LocalMuter(LoopbackCoordinator* coordinator,
                       const base::UnguessableToken& group_id)
    : loopback_group_observer_(
          coordinator,
          group_id,
          // Start muting each new source added to the group.
          /*on_source_added=*/base::BindRepeating(&StartMutingSource),
          // This looks like a potential bug, but the existing behavior is to do
          // nothing when a source leaves the group. Probably because this
          // normally happens when the audio stream is being destroyed.
          /*on_source_removed=*/LoopbackGroupObserver::do_nothing()) {
  loopback_group_observer_.StartObserving();
  loopback_group_observer_.ForEachMember(
      loopback_group_observer_.on_source_added());

  receivers_.set_disconnect_handler(
      base::BindRepeating(&LocalMuter::OnBindingLost, base::Unretained(this)));
}

LocalMuter::~LocalMuter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Un-mute all members of the group this muter was responsible for.
  loopback_group_observer_.ForEachMember(
      base::BindRepeating(&StopMutingSource));
  loopback_group_observer_.StopObserving();
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

}  // namespace audio
