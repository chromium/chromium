// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_controller.h"

#include <set>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/audio_focus_request.h"
#include "services/media_session/public/cpp/media_image_manager.h"

namespace media_session {

// ImageObserverHolder will hold each mojo image observer with the image
// size and type preferences it specified when the observer was added.
class MediaController::ImageObserverHolder {
 public:
  ImageObserverHolder(
      MediaController* owner,
      mojom::MediaSessionImageType type,
      int minimum_size_px,
      int desired_size_px,
      mojo::PendingRemote<mojom::MediaControllerImageObserver> observer,
      const std::vector<MediaImage>& current_images)
      : manager_(minimum_size_px, desired_size_px),
        owner_(owner),
        type_(type),
        minimum_size_px_(minimum_size_px),
        desired_size_px_(desired_size_px),
        observer_(std::move(observer)) {
    // Set a connection error handler so that we will remove observers that have
    // had an error / been closed.
    observer_.set_disconnect_handler(base::BindOnce(
        &MediaController::CleanupImageObservers, base::Unretained(owner_)));

    // Flush the observer with the latest state.
    ImagesChanged(current_images);
  }

  ImageObserverHolder(const ImageObserverHolder&) = delete;
  ImageObserverHolder& operator=(const ImageObserverHolder&) = delete;

  ~ImageObserverHolder() = default;

  bool is_valid() const { return observer_.is_connected(); }

  mojom::MediaSessionImageType type() const { return type_; }

  void ImagesChanged(const std::vector<MediaImage>& images) {
    absl::optional<MediaImage> image = manager_.SelectImage(images);

    // If we could not find an image then we should call with an empty image to
    // flush the observer.
    if (!image) {
      ClearImage();
      return;
    }

    DCHECK(owner_->session_->ipc());
    owner_->session_->GetMediaImageBitmap(
        *image, minimum_size_px_, desired_size_px_,
        base::BindOnce(&MediaController::ImageObserverHolder::OnImage,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void ClearImage() {
    // If the last thing we sent was a ClearImage, don't send another one. If we
    // haven't sent anything before, then send a ClearImage.
    if (!did_send_image_last_.value_or(true))
      return;
    did_send_image_last_ = false;
    observer_->MediaControllerImageChanged(type_, SkBitmap());
  }

 private:
  void OnImage(const SkBitmap& image) {
    did_send_image_last_ = true;
    observer_->MediaControllerImageChanged(type_, image);
  }

  media_session::MediaImageManager manager_;

  const raw_ptr<MediaController> owner_;

  mojom::MediaSessionImageType const type_;

  int const minimum_size_px_;

  int const desired_size_px_;

  mojo::Remote<mojom::MediaControllerImageObserver> observer_;

  // Whether the last information sent to the observer was an image.
  // Empty if we have not yet sent anything.
  absl::optional<bool> did_send_image_last_;

  base::WeakPtrFactory<ImageObserverHolder> weak_ptr_factory_{this};
};

MediaController::MediaController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MediaController::~MediaController() = default;

void MediaController::Suspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->PerformUIAction(mojom::MediaSessionAction::kPause);
}

void MediaController::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->PerformUIAction(mojom::MediaSessionAction::kPlay);
}

void MediaController::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->PerformUIAction(mojom::MediaSessionAction::kStop);
}

void MediaController::ToggleSuspendResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_info_.is_null())
    return;

  switch (session_info_->playback_state) {
    case mojom::MediaPlaybackState::kPlaying:
      Suspend();
      break;
    case mojom::MediaPlaybackState::kPaused:
      Resume();
      break;
  }
}

void MediaController::AddObserver(
    mojo::PendingRemote<mojom::MediaControllerObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<mojom::MediaControllerObserver> media_controller_observer(
      std::move(observer));
  if (session_) {
    media_controller_observer->MediaSessionChanged(session_->id());
  } else {
    media_controller_observer->MediaSessionChanged(absl::nullopt);
  }

  // Flush the new observer with the current state.
  media_controller_observer->MediaSessionInfoChanged(session_info_.Clone());
  media_controller_observer->MediaSessionMetadataChanged(session_metadata_);
  media_controller_observer->MediaSessionActionsChanged(session_actions_);
  media_controller_observer->MediaSessionPositionChanged(session_position_);

  observers_.Add(std::move(media_controller_observer));
}

void MediaController::MediaSessionInfoChanged(mojom::MediaSessionInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_)
    observer->MediaSessionInfoChanged(info.Clone());

  session_info_ = std::move(info);
}

void MediaController::MediaSessionMetadataChanged(
    const absl::optional<MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_)
    observer->MediaSessionMetadataChanged(metadata);

  session_metadata_ = metadata;
}

void MediaController::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_)
    observer->MediaSessionActionsChanged(actions);

  session_actions_ = actions;
}

void MediaController::MediaSessionPositionChanged(
    const absl::optional<media_session::MediaPosition>& position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_)
    observer->MediaSessionPositionChanged(position);

  session_position_ = position;
}

void MediaController::MediaSessionImagesChanged(
    const base::flat_map<mojom::MediaSessionImageType, std::vector<MediaImage>>&
        images) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Work out which image types have changed.
  std::set<mojom::MediaSessionImageType> types_changed;
  for (const auto& entry : images) {
    auto it = session_images_.find(entry.first);
    if (it != session_images_.end() && entry.second == it->second)
      continue;

    types_changed.insert(entry.first);
  }

  session_images_ = images;

  for (auto& holder : image_observers_) {
    auto it = session_images_.find(holder->type());

    if (it == session_images_.end()) {
      // No image of this type is available from the session so we should clear
      // any image the observers might have.
      holder->ClearImage();
    } else if (base::Contains(types_changed, holder->type())) {
      holder->ImagesChanged(it->second);
    }
  }
}

void MediaController::PreviousTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->PreviousTrack();
}

void MediaController::NextTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->NextTrack();
}

void MediaController::Seek(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->Seek(seek_time);
}

void MediaController::ObserveImages(
    mojom::MediaSessionImageType type,
    int minimum_size_px,
    int desired_size_px,
    mojo::PendingRemote<mojom::MediaControllerImageObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = session_images_.find(type);

  image_observers_.push_back(std::make_unique<ImageObserverHolder>(
      this, type, minimum_size_px, desired_size_px, std::move(observer),
      it == session_images_.end() ? std::vector<MediaImage>() : it->second));
}

void MediaController::SeekTo(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->SeekTo(seek_time);
}

void MediaController::ScrubTo(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->ScrubTo(seek_time);
}

void MediaController::EnterPictureInPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->EnterPictureInPicture();
}

void MediaController::ExitPictureInPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->ExitPictureInPicture();
}

void MediaController::SetAudioSinkId(const absl::optional<std::string>& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->SetAudioSinkId(id);
}

void MediaController::ToggleMicrophone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->ToggleMicrophone();
}

void MediaController::ToggleCamera() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->ToggleCamera();
}

void MediaController::HangUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->HangUp();
}

void MediaController::Raise() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->Raise();
}

void MediaController::SetMute(bool mute) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->SetMute(mute);
}

void MediaController::RequestMediaRemoting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_)
    session_->ipc()->RequestMediaRemoting();
}

void MediaController::EnterAutoPictureInPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_) {
    session_->ipc()->EnterAutoPictureInPicture();
  }
}

void MediaController::SetMediaSession(AudioFocusRequest* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(session);

  if (session_ == session)
    return;

  Reset();

  session_ = session;

  // We should always notify the observers that the media session has changed.
  for (auto& observer : observers_)
    observer->MediaSessionChanged(session->id());

  // Add |this| as an observer for |session|.
  session->ipc()->AddObserver(session_receiver_.BindNewPipeAndPassRemote());
}

void MediaController::ClearMediaSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!session_)
    return;

  Reset();

  // If we are no longer bound to a session we should flush the observers
  // with empty data.
  for (auto& observer : observers_) {
    observer->MediaSessionChanged(absl::nullopt);
    observer->MediaSessionInfoChanged(nullptr);
    observer->MediaSessionMetadataChanged(absl::nullopt);
    observer->MediaSessionActionsChanged(
        std::vector<mojom::MediaSessionAction>());
    observer->MediaSessionPositionChanged(absl::nullopt);
  }

  for (auto& holder : image_observers_)
    holder->ClearImage();
}

void MediaController::BindToInterface(
    mojo::PendingReceiver<mojom::MediaController> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void MediaController::FlushForTesting() {
  receivers_.FlushForTesting();
}

void MediaController::CleanupImageObservers() {
  base::EraseIf(image_observers_,
                [](const auto& holder) { return !holder->is_valid(); });
}

void MediaController::Reset() {
  session_ = nullptr;
  session_receiver_.reset();
  session_info_.reset();
  session_metadata_.reset();
  session_actions_.clear();
  session_images_.clear();
  session_position_.reset();
}

}  // namespace media_session
