// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/local_muter.h"

#include <utility>

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

  bindings_.set_connection_error_handler(
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

void LocalMuter::SetAllBindingsLostCallback(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  all_bindings_lost_callback_ = std::move(callback);
}

void LocalMuter::AddBinding(mojom::LocalMuterAssociatedRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bindings_.AddBinding(this, std::move(request));
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

  if (bindings_.empty()) {
    std::move(all_bindings_lost_callback_).Run();
  }
}

}  // namespace audio
