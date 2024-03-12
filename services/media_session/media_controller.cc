// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_controller.h"

#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/audio_focus_request.h"
#include "services/media_session/public/cpp/chapter_information.h"
#include "services/media_session/public/cpp/media_image_manager.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_session {

namespace {
using ChapterMap = base::flat_map<int, std::vector<MediaImage>>;
using ImagesVariant = std::variant<std::vector<MediaImage>, ChapterMap>;
}  // namespace

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
      const ImagesVariant& current_images)
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
    if (type == mojom::MediaSessionImageType::kChapter) {
      CHECK(std::holds_alternative<ChapterMap>(current_images));
      UpdateChapterSize(std::get<ChapterMap>(current_images));
      for (auto& chapter_images : std::get<ChapterMap>(current_images)) {
        ImagesChanged(chapter_images.second, chapter_images.first);
      }
    } else {
      CHECK(std::holds_alternative<std::vector<MediaImage>>(current_images));
      ImagesChanged(std::get<std::vector<MediaImage>>(current_images),
                    std::nullopt);
    }
  }

  ImageObserverHolder(const ImageObserverHolder&) = delete;
  ImageObserverHolder& operator=(const ImageObserverHolder&) = delete;

  ~ImageObserverHolder() = default;

  bool is_valid() const { return observer_.is_connected(); }

  mojom::MediaSessionImageType type() const { return type_; }

  void UpdateChapterSize(const ChapterMap& images) {
    chapter_size_ = static_cast<int>(images.size());
  }

  void ImagesChanged(const std::vector<MediaImage>& images,
                     const std::optional<int>& chapter_index) {
    std::optional<MediaImage> image = manager_.SelectImage(images);

    // If we could not find an image then we should call with an empty image to
    // flush the observer.
    if (!image) {
      ClearImage(chapter_index);
      return;
    }

    DCHECK(owner_->session_->ipc());
    owner_->session_->GetMediaImageBitmap(
        *image, minimum_size_px_, desired_size_px_,
        base::BindOnce(&MediaController::ImageObserverHolder::OnImage,
                       weak_ptr_factory_.GetWeakPtr(), chapter_index));
  }

  void ClearImage(const std::optional<int>& chapter_index) {
    // If the last thing we sent was a ClearImage, don't send another one. If we
    // haven't sent anything before, then send a ClearImage.
    auto index = chapter_index.value_or(-1);
    auto it = did_send_image_last_.find(index);
    if (it != did_send_image_last_.end() && !it->second) {
      return;
    }
    did_send_image_last_[index] = false;
    if (type_ == mojom::MediaSessionImageType::kChapter) {
      observer_->MediaControllerChapterImageChanged(chapter_index.value(),
                                                    SkBitmap());
      return;
    }
    observer_->MediaControllerImageChanged(type_, SkBitmap());
  }

  void ClearAllChapterImage() {
    if (type_ != mojom::MediaSessionImageType::kChapter) {
      return;
    }

    for (int index = 0; index < chapter_size_; index++) {
      // If the last thing we sent was a ClearImage for this index, don't send
      // another one. If we haven't sent anything before, then send a
      // ClearImage.
      auto it = did_send_image_last_.find(index);
      if (it != did_send_image_last_.end() && !it->second) {
        continue;
      }
      did_send_image_last_[index] = false;
      observer_->MediaControllerChapterImageChanged(index, SkBitmap());
    }
  }

 private:
  void OnImage(const std::optional<int>& chapter_index, const SkBitmap& image) {
    auto index = chapter_index.value_or(-1);
    did_send_image_last_[index] = true;
    if (type_ == mojom::MediaSessionImageType::kChapter) {
      observer_->MediaControllerChapterImageChanged(chapter_index.value(),
                                                    image);
      return;
    }
    observer_->MediaControllerImageChanged(type_, image);
  }

  media_session::MediaImageManager manager_;

  const raw_ptr<MediaController> owner_;

  mojom::MediaSessionImageType const type_;

  int const minimum_size_px_;

  int const desired_size_px_;

  mojo::Remote<mojom::MediaControllerImageObserver> observer_;

  // Whether the last information sent to the observer was an image for the
  // chapter index.  If the image type is not chapter, it will use the default
  // index -1. Empty at the index if we have not yet sent anything.
  base::flat_map<int, bool> did_send_image_last_;

  // The size of the chapter list. This will be used to clear all the chapter
  // images in this holder.
  int chapter_size_ = 0;

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
    media_controller_observer->MediaSessionChanged(std::nullopt);
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
    const std::optional<MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_)
    observer->MediaSessionMetadataChanged(metadata);

  session_metadata_ = metadata;

  if (!metadata.has_value()) {
    return;
  }

  // Removes the chapter image metadata from the last media and sets the new
  // ones.
  chapter_images_.clear();
  for (int index = 0;
       index < static_cast<int>(metadata.value().chapters.size()); index++) {
    chapter_images_[index] = metadata.value().chapters[index].artwork();
  }

  for (auto& holder : image_observers_) {
    if (holder->type() != mojom::MediaSessionImageType::kChapter) {
      continue;
    }

    holder->UpdateChapterSize(chapter_images_);
    for (int index = 0;
         index < static_cast<int>(metadata.value().chapters.size()); index++) {
      auto it = chapter_images_.find(index);

      if (it == chapter_images_.end()) {
        // No image of this chapter is available from the session so we should
        // clear any image the observers might have.
        holder->ClearImage(index);
      } else {
        holder->ImagesChanged(it->second, index);
      }
    }
  }
}

void MediaController::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_)
    observer->MediaSessionActionsChanged(actions);

  session_actions_ = actions;
}

void MediaController::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
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
    if (holder->type() == mojom::MediaSessionImageType::kChapter) {
      continue;
    }
    auto it = session_images_.find(holder->type());

    if (it == session_images_.end()) {
      // No image is available from the session so we should clear any image the
      // observers might have.
      holder->ClearImage(std::nullopt);
    } else if (base::Contains(types_changed, holder->type())) {
      holder->ImagesChanged(it->second, std::nullopt);
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

void MediaController::SkipAd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (session_) {
    session_->ipc()->SkipAd();
  }
}

void MediaController::ObserveImages(
    mojom::MediaSessionImageType type,
    int minimum_size_px,
    int desired_size_px,
    mojo::PendingRemote<mojom::MediaControllerImageObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (type == mojom::MediaSessionImageType::kChapter) {
    image_observers_.push_back(std::make_unique<ImageObserverHolder>(
        this, type, minimum_size_px, desired_size_px, std::move(observer),
        chapter_images_));

    return;
  }
  auto it = session_images_.find(type);
  auto images =
      it == session_images_.end() ? std::vector<MediaImage>() : it->second;

  image_observers_.push_back(std::make_unique<ImageObserverHolder>(
      this, type, minimum_size_px, desired_size_px, std::move(observer),
      images));
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

void MediaController::SetAudioSinkId(const std::optional<std::string>& id) {
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
    observer->MediaSessionChanged(std::nullopt);
    observer->MediaSessionInfoChanged(nullptr);
    observer->MediaSessionMetadataChanged(std::nullopt);
    observer->MediaSessionActionsChanged(
        std::vector<mojom::MediaSessionAction>());
    observer->MediaSessionPositionChanged(std::nullopt);
  }

  for (auto& holder : image_observers_) {
    if (holder->type() == mojom::MediaSessionImageType::kChapter) {
      holder->ClearAllChapterImage();
    } else {
      holder->ClearImage(std::nullopt);
    }
  }
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
  std::erase_if(image_observers_,
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
