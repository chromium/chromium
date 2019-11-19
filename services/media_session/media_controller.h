// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
#define SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_session {

class AudioFocusRequest;

// MediaController provides a control surface over Mojo for controlling a
// specific MediaSession. If |session_| is nullptr then all commands will be
// dropped. MediaController is also a MediaSessionObserver and will forward
// events to added observers.
class MediaController : public mojom::MediaController,
                        public mojom::MediaSessionObserver {
 public:
  MediaController();
  ~MediaController() override;

  // mojom::MediaController overrides.
  void Suspend() override;
  void Resume() override;
  void Stop() override;
  void ToggleSuspendResume() override;
  void AddObserver(
      mojo::PendingRemote<mojom::MediaControllerObserver> observer) override;
  void PreviousTrack() override;
  void NextTrack() override;
  void Seek(base::TimeDelta seek_time) override;
  void ObserveImages(mojom::MediaSessionImageType type,
                     int minimum_size_px,
                     int desired_size_px,
                     mojo::PendingRemote<mojom::MediaControllerImageObserver>
                         observer) override;
  void SeekTo(base::TimeDelta seek_time) override;
  void ScrubTo(base::TimeDelta seek_time) override;

  // mojom::MediaSessionObserver overrides.
  void MediaSessionInfoChanged(
      mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<MediaMetadata>&) override;
  void MediaSessionActionsChanged(
      const std::vector<mojom::MediaSessionAction>& action) override;
  void MediaSessionImagesChanged(
      const base::flat_map<mojom::MediaSessionImageType,
                           std::vector<MediaImage>>& images) override;
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override;

  void SetMediaSession(AudioFocusRequest* session);
  void ClearMediaSession();

  void BindToInterface(mojo::PendingReceiver<mojom::MediaController> receiver);
  void FlushForTesting();

 private:
  friend class MediaControllerTest;

  class ImageObserverHolder;

  // Removes unbound or faulty image observers.
  void CleanupImageObservers();

  void Reset();

  // Holds mojo bindings for mojom::MediaController.
  mojo::ReceiverSet<mojom::MediaController> receivers_;

  // The current info for the |session_|.
  mojom::MediaSessionInfoPtr session_info_;

  // The current metadata for |session_|.
  base::Optional<MediaMetadata> session_metadata_;

  // The current actions for |session_|.
  std::vector<mojom::MediaSessionAction> session_actions_;

  // The current position for |session_|.
  base::Optional<MediaPosition> session_position_;

  // The current images for |session_|.
  base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>>
      session_images_;

  // Raw pointer to the media session we are controlling.
  AudioFocusRequest* session_ = nullptr;

  // Observers that are observing |this|.
  mojo::RemoteSet<mojom::MediaControllerObserver> observers_;

  // Binding for |this| to act as an observer to |session_|.
  mojo::Receiver<mojom::MediaSessionObserver> session_receiver_{this};

  // Manages individual image observers.
  std::vector<std::unique_ptr<ImageObserverHolder>> image_observers_;

  // Protects |session_| as it is not thread safe.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MediaController);
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_MEDIA_CONTROLLER_H_
