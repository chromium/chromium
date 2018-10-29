// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
#define SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_

#include <list>
#include <string>
#include <unordered_map>

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
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

class MediaController;

class AudioFocusManager : public mojom::AudioFocusManager,
                          public mojom::AudioFocusManagerDebug {
 public:
  AudioFocusManager();
  ~AudioFocusManager() override;

  // TODO(beccahughes): Remove this.
  using RequestId = base::UnguessableToken;

  // mojom::AudioFocusManager.
  void RequestAudioFocus(mojom::AudioFocusRequestClientRequest request,
                         mojom::MediaSessionPtr media_session,
                         mojom::MediaSessionInfoPtr session_info,
                         mojom::AudioFocusType type,
                         RequestAudioFocusCallback callback) override;
  void GetFocusRequests(GetFocusRequestsCallback callback) override;
  void AddObserver(mojom::AudioFocusObserverPtr observer) override;
  void SetSourceName(const std::string& name) override;

  // mojom::AudioFocusManagerDebug.
  void GetDebugInfoForRequest(const RequestId& request_id,
                              GetDebugInfoForRequestCallback callback) override;

  // Bind to a mojom::AudioFocusManagerRequest.
  void BindToInterface(mojom::AudioFocusManagerRequest request);

  // Bind to a mojom::AudioFocusManagerDebugRequest.
  void BindToDebugInterface(mojom::AudioFocusManagerDebugRequest request);

  // Bind to a mojom::MediaControllerRequest.
  void BindToActiveControllerInterface(mojom::MediaControllerRequest request);

 private:
  friend class AudioFocusManagerTest;
  friend class MediaControllerTest;
  friend class test::MockMediaSession;

  // StackRow is an AudioFocusRequestClient and allows a media session to
  // control its audio focus.
  class StackRow;

  // BindingContext stores associated metadata for mojo binding.
  struct BindingContext {
    // The source name is associated with a binding when a client calls
    // |SetSourceName|. It is used to provide more granularity than a
    // service_manager::Identity for metrics and for identifying where an audio
    // focus request originated from.
    std::string source_name;
  };

  void RequestAudioFocusInternal(std::unique_ptr<StackRow>,
                                 mojom::AudioFocusType,
                                 base::OnceCallback<void()>);
  void EnforceAudioFocusRequest(mojom::AudioFocusType);

  void AbandonAudioFocusInternal(RequestId);
  void EnforceAudioFocusAbandon(mojom::AudioFocusType);

  // Called when the active media session with audio focus changes.
  void DidChangeFocus();

  std::unique_ptr<StackRow> RemoveFocusEntryIfPresent(RequestId id);

  // Returns the source name of the binding currently accessing the Audio
  // Focus Manager API over mojo.
  const std::string& GetBindingSourceName() const;

  bool IsSessionOnTopOfAudioFocusStack(RequestId id,
                                       mojom::AudioFocusType type) const;

  // This |MediaController| acts as a proxy for controlling the active
  // |MediaSession| over mojo.
  MediaController active_media_controller_;

  // Holds mojo bindings for the Audio Focus Manager API.
  mojo::BindingSet<mojom::AudioFocusManager, std::unique_ptr<BindingContext>>
      bindings_;

  // Holds mojo bindings for the Audio Focus Manager Debug API.
  mojo::BindingSet<mojom::AudioFocusManagerDebug> debug_bindings_;

  // Weak reference of managed observers. Observers are expected to remove
  // themselves before being destroyed.
  mojo::InterfacePtrSet<mojom::AudioFocusObserver> observers_;

  // A stack of Mojo interface pointers and their requested audio focus type.
  // A MediaSession must abandon audio focus before its destruction.
  std::list<std::unique_ptr<StackRow>> audio_focus_stack_;

  // Adding observers should happen on the same thread that the service is
  // running on.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioFocusManager);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_AUDIO_FOCUS_MANAGER_H_
