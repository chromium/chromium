// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOCAL_MUTER_H_
#define SERVICES_AUDIO_LOCAL_MUTER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/audio/loopback_coordinator.h"

namespace audio {

class LoopbackGroupMember;

// Mutes a group of streams, from construction time until destruction time. In
// between, LocalMuter ensures new group members are also muted. Holds all
// media::mojom::LocalMuter bindings.
class LocalMuter final : public media::mojom::LocalMuter,
                         public LoopbackCoordinator::Observer {
 public:
  LocalMuter(LoopbackCoordinator* coordinator,
             const base::UnguessableToken& group_id);

  LocalMuter(const LocalMuter&) = delete;
  LocalMuter& operator=(const LocalMuter&) = delete;

  ~LocalMuter() final;

  const base::UnguessableToken& group_id() const { return group_id_; }

  // SetAllBindingsLostCallback() must be called before the first call to
  // AddBinding().
  void SetAllBindingsLostCallback(base::RepeatingClosure callback);
  void AddReceiver(
      mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver);

  // LoopbackCoordinator::Observer implementation.
  void OnMemberJoinedGroup(LoopbackGroupMember* member) final;
  void OnMemberLeftGroup(LoopbackGroupMember* member) final;

  bool HasReceivers() { return !receivers_.empty(); }

  base::WeakPtr<LocalMuter> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  // Runs the |all_bindings_lost_callback_| when |bindings_| becomes empty.
  void OnBindingLost();

  const raw_ptr<LoopbackCoordinator> coordinator_;
  const base::UnguessableToken group_id_;

  mojo::AssociatedReceiverSet<media::mojom::LocalMuter> receivers_;
  base::RepeatingClosure all_bindings_lost_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LocalMuter> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOCAL_MUTER_H_
