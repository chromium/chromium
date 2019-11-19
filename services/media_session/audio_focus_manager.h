// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
#define SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/media_controller.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media_session {

namespace test {
class MockMediaSession;
}  // namespace test

class AudioFocusRequest;
class MediaController;
class MediaPowerDelegate;

struct EnforcementState {
  bool should_duck = false;
  bool should_stop = false;
  bool should_suspend = false;
};

class AudioFocusManager : public mojom::AudioFocusManager,
                          public mojom::AudioFocusManagerDebug,
                          public mojom::MediaControllerManager {
 public:
  AudioFocusManager();
  ~AudioFocusManager() override;

  // TODO(beccahughes): Remove this.
  using RequestId = base::UnguessableToken;

  // mojom::AudioFocusManager.
  void RequestAudioFocus(
      mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
      mojo::PendingRemote<mojom::MediaSession> media_session,
      mojom::MediaSessionInfoPtr session_info,
      mojom::AudioFocusType type,
      RequestAudioFocusCallback callback) override;
  void RequestGroupedAudioFocus(
      const base::UnguessableToken& request_id,
      mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
      mojo::PendingRemote<mojom::MediaSession> media_session,
      mojom::MediaSessionInfoPtr session_info,
      mojom::AudioFocusType type,
      const base::UnguessableToken& group_id,
      RequestGroupedAudioFocusCallback callback) override;
  void GetFocusRequests(GetFocusRequestsCallback callback) override;
  void AddObserver(
      mojo::PendingRemote<mojom::AudioFocusObserver> observer) override;
  void SetSource(const base::UnguessableToken& identity,
                 const std::string& name) override;
  void SetEnforcementMode(mojom::EnforcementMode mode) override;
  void AddSourceObserver(
      const base::UnguessableToken& source_id,
      mojo::PendingRemote<mojom::AudioFocusObserver> observer) override;
  void GetSourceFocusRequests(const base::UnguessableToken& source_id,
                              GetFocusRequestsCallback callback) override;

  // mojom::AudioFocusManagerDebug.
  void GetDebugInfoForRequest(const RequestId& request_id,
                              GetDebugInfoForRequestCallback callback) override;

  // mojom::MediaControllerManager.
  void CreateActiveMediaController(
      mojo::PendingReceiver<mojom::MediaController> receiver) override;
  void CreateMediaControllerForSession(
      mojo::PendingReceiver<mojom::MediaController> receiver,
      const base::UnguessableToken& receiver_id) override;
  void SuspendAllSessions() override;

  // Bind to a receiver of mojom::AudioFocusManager.
  void BindToInterface(
      mojo::PendingReceiver<mojom::AudioFocusManager> receiver);

  // Bind to a receiver of mojom::AudioFocusManagerDebug.
  void BindToDebugInterface(
      mojo::PendingReceiver<mojom::AudioFocusManagerDebug> receiver);

  // Bind to a receiver of mojom::MediaControllerManager.
  void BindToControllerManagerInterface(
      mojo::PendingReceiver<mojom::MediaControllerManager> receiver);

 private:
  friend class AudioFocusManagerTest;
  friend class AudioFocusRequest;
  friend class MediaControllerTest;
  friend class test::MockMediaSession;

  class SourceObserverHolder;

  // ReceiverContext stores associated metadata for mojo binding.
  struct ReceiverContext {
    // The source name is associated with a binding when a client calls
    // |SetSourceName|. It is used to provide more granularity than a
    // service_manager::Identity for metrics and for identifying where an audio
    // focus request originated from.
    std::string source_name;

    // The identity associated with the binding when it was created.
    base::UnguessableToken identity;
  };

  void RequestAudioFocusInternal(std::unique_ptr<AudioFocusRequest>,
                                 mojom::AudioFocusType);
  void AbandonAudioFocusInternal(RequestId);

  void EnforceAudioFocus();

  void MaybeUpdateActiveSession();

  std::unique_ptr<AudioFocusRequest> RemoveFocusEntryIfPresent(RequestId id);

  bool IsFocusEntryPresent(const base::UnguessableToken& id) const;

  // Returns the source name of the binding currently accessing the Audio
  // Focus Manager API over mojo.
  const std::string& GetBindingSourceName() const;

  // Returns the identity of the binding currently accessing the Audio Focus
  // Manager API over mojo.
  const base::UnguessableToken& GetBindingIdentity() const;

  bool IsSessionOnTopOfAudioFocusStack(RequestId id,
                                       mojom::AudioFocusType type) const;

  bool ShouldSessionBeSuspended(const AudioFocusRequest* session,
                                const EnforcementState& state) const;
  bool ShouldSessionBeDucked(const AudioFocusRequest* session,
                             const EnforcementState& state) const;

  void EnforceSingleSession(AudioFocusRequest* session,
                            const EnforcementState& state);

  // Removes unbound or faulty source observers.
  void CleanupSourceObservers();

  // This |MediaController| acts as a proxy for controlling the active
  // |MediaSession| over mojo.
  MediaController active_media_controller_;

  // Holds mojo receivers for the Audio Focus Manager API.
  mojo::ReceiverSet<mojom::AudioFocusManager, std::unique_ptr<ReceiverContext>>
      receivers_;

  // Holds mojo receivers for the Audio Focus Manager Debug API.
  mojo::ReceiverSet<mojom::AudioFocusManagerDebug> debug_receivers_;

  // Holds mojo receivers for the Media Controller Manager API.
  mojo::ReceiverSet<mojom::MediaControllerManager> controller_receivers_;

  // Weak reference of managed observers. Observers are expected to remove
  // themselves before being destroyed.
  mojo::RemoteSet<mojom::AudioFocusObserver> observers_;

  // Manages individual source observers.
  std::vector<std::unique_ptr<SourceObserverHolder>> source_observers_;

  // A stack of Mojo interface pointers and their requested audio focus type.
  // A MediaSession must abandon audio focus before its destruction.
  std::list<std::unique_ptr<AudioFocusRequest>> audio_focus_stack_;

  // Controls media playback when device power events occur.
  std::unique_ptr<MediaPowerDelegate> power_delegate_;

  mojom::EnforcementMode enforcement_mode_;

  // Adding observers should happen on the same thread that the service is
  // running on.
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<AudioFocusManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioFocusManager);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
