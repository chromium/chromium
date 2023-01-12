// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/local_muter.h"

#include <utility>

#include "base/functional/bind.h"
#include "services/audio/loopback_group_member.h"

namespace audio {

LocalMuter::LocalMuter(LoopbackCoordinator* coordinator,
                       const base::UnguessableToken& group_id)
    : coordinator_(coordinator), group_id_(group_id) {
  DCHECK(coordinator_);

  coordinator_->AddObserver(group_id_, this);
  coordinator_->ForEachMemberInGroup(
      group_id_, base::BindRepeating([](LoopbackGroupMember* member) {
        member->StartMuting();
      }));

  receivers_.set_disconnect_handler(
      base::BindRepeating(&LocalMuter::OnBindingLost, base::Unretained(this)));
}

LocalMuter::~LocalMuter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  coordinator_->ForEachMemberInGroup(
      group_id_, base::BindRepeating([](LoopbackGroupMember* member) {
        member->StopMuting();
      }));
  coordinator_->RemoveObserver(group_id_, this);
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

void LocalMuter::OnMemberJoinedGroup(LoopbackGroupMember* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  member->StartMuting();
}

void LocalMuter::OnMemberLeftGroup(LoopbackGroupMember* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No change to muting state.
}

void LocalMuter::OnBindingLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!HasReceivers()) {
    all_bindings_lost_callback_.Run();
  }
}

}  // namespace audio
