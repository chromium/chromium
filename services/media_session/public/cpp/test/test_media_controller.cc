// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/test_media_controller.h"

namespace media_session {
namespace test {

TestMediaControllerImageObserver::TestMediaControllerImageObserver(
    mojo::Remote<mojom::MediaController>& controller,
    int minimum_size_px,
    int desired_size_px) {
  controller->ObserveImages(mojom::MediaSessionImageType::kArtwork,
                            minimum_size_px, desired_size_px,
                            receiver_.BindNewPipeAndPassRemote());
  controller->ObserveImages(
      mojom::MediaSessionImageType::kChapter, minimum_size_px, desired_size_px,
      chapter_image_observer_receiver_.BindNewPipeAndPassRemote());
  controller.FlushForTesting();
}

TestMediaControllerImageObserver::~TestMediaControllerImageObserver() = default;

void TestMediaControllerImageObserver::MediaControllerImageChanged(
    mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  current_ = ImageTypePair(type, bitmap.isNull());

  if (!expected_.has_value() || expected_ != current_)
    return;

  DCHECK(run_loop_);
  run_loop_->Quit();
  expected_.reset();
}

void TestMediaControllerImageObserver::MediaControllerChapterImageChanged(
    int chapter_index,
    const SkBitmap& bitmap) {
  current_chapter_images_[chapter_index] = bitmap.isNull();

  if (!base::Contains(expected_chapter_images_, chapter_index) ||
      expected_chapter_images_[chapter_index] !=
          current_chapter_images_[chapter_index]) {
    return;
  }

  DCHECK(run_loop_);
  run_loop_->Quit();
}

void TestMediaControllerImageObserver::WaitForExpectedImageOfType(
    mojom::MediaSessionImageType type,
    bool expect_null_image) {
  ImageTypePair pair(type, expect_null_image);

  if (current_ == pair)
    return;

  expected_ = pair;

  DCHECK(!run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void TestMediaControllerImageObserver::WaitForExpectedChapterImage(
    int chapter_index,
    bool expect_null_image) {
  if (current_chapter_images_[chapter_index] == expect_null_image) {
    return;
  }
  expected_chapter_images_[chapter_index] = expect_null_image;

  DCHECK(!run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

TestMediaControllerObserver::TestMediaControllerObserver(
    mojo::Remote<mojom::MediaController>& media_controller) {
  media_controller->AddObserver(receiver_.BindNewPipeAndPassRemote());
}

TestMediaControllerObserver::~TestMediaControllerObserver() = default;

void TestMediaControllerObserver::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr session) {
  session_info_ = std::move(session);

  if (session_info_.has_value() && !session_info_->is_null()) {
    if (wanted_state_ == session_info()->state ||
        session_info()->playback_state == wanted_playback_state_) {
      run_loop_->Quit();
    }
  } else if (waiting_for_empty_info_) {
    waiting_for_empty_info_ = false;
    run_loop_->Quit();
  }
}

void TestMediaControllerObserver::MediaSessionMetadataChanged(
    const std::optional<MediaMetadata>& metadata) {
  session_metadata_ = metadata;

  if (expected_metadata_.has_value() && expected_metadata_ == metadata) {
    run_loop_->Quit();
    expected_metadata_.reset();
  } else if (waiting_for_empty_metadata_ &&
             (!metadata.has_value() || metadata->IsEmpty())) {
    run_loop_->Quit();
    waiting_for_empty_metadata_ = false;
  }
}

void TestMediaControllerObserver::MediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  session_actions_ =
      std::set<mojom::MediaSessionAction>(actions.begin(), actions.end());

  if (expected_actions_.has_value() && expected_actions_ == session_actions_) {
    run_loop_->Quit();
    expected_actions_.reset();
  }
}

void TestMediaControllerObserver::MediaSessionChanged(
    const std::optional<base::UnguessableToken>& request_id) {
  session_request_id_ = request_id;

  if (expected_request_id_.has_value() &&
      expected_request_id_ == session_request_id_) {
    run_loop_->Quit();
    expected_request_id_.reset();
  }
}

void TestMediaControllerObserver::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  session_position_ = position;

  if (waiting_for_empty_position_ && !position.has_value()) {
    run_loop_->Quit();
    waiting_for_empty_position_ = false;
  } else if (waiting_for_non_empty_position_ && position.has_value()) {
    run_loop_->Quit();
    waiting_for_non_empty_position_ = false;
  }
}

void TestMediaControllerObserver::WaitForState(
    mojom::MediaSessionInfo::SessionState wanted_state) {
  if (session_info_ && session_info()->state == wanted_state)
    return;

  wanted_state_ = wanted_state;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForPlaybackState(
    mojom::MediaPlaybackState wanted_state) {
  if (session_info_ && session_info()->playback_state == wanted_state)
    return;

  wanted_playback_state_ = wanted_state;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForEmptyInfo() {
  if (session_info_.has_value() && session_info_->is_null())
    return;

  waiting_for_empty_info_ = true;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForEmptyMetadata() {
  if (session_metadata_.has_value())
    return;

  waiting_for_empty_metadata_ = true;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForExpectedMetadata(
    const MediaMetadata& metadata) {
  if (session_metadata_.has_value() && session_metadata_ == metadata)
    return;

  expected_metadata_ = metadata;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForEmptyActions() {
  WaitForExpectedActions(std::set<mojom::MediaSessionAction>());
}

void TestMediaControllerObserver::WaitForExpectedActions(
    const std::set<mojom::MediaSessionAction>& actions) {
  if (session_actions_.has_value() && session_actions_ == actions)
    return;

  expected_actions_ = actions;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForEmptyPosition() {
  // |session_position_| is doubly wrapped in std::optional so we must check
  // both values.
  if (session_position_.has_value() && !session_position_->has_value())
    return;

  waiting_for_empty_position_ = true;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForNonEmptyPosition() {
  if (session_position_.has_value() && session_position_->has_value())
    return;

  waiting_for_non_empty_position_ = true;
  StartWaiting();
}

void TestMediaControllerObserver::WaitForSession(
    const std::optional<base::UnguessableToken>& request_id) {
  if (session_request_id_.has_value() && session_request_id_ == request_id)
    return;

  expected_request_id_ = request_id;
  StartWaiting();
}

void TestMediaControllerObserver::StartWaiting() {
  DCHECK(!run_loop_);

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

TestMediaController::TestMediaController() = default;

TestMediaController::~TestMediaController() = default;

mojo::Remote<mojom::MediaController>
TestMediaController::CreateMediaControllerRemote() {
  mojo::Remote<mojom::MediaController> remote;
  receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  return remote;
}

void TestMediaController::BindMediaControllerReceiver(
    mojo::PendingReceiver<mojom::MediaController> receiver) {
  receiver_.Bind(std::move(receiver));
}

void TestMediaController::Suspend() {
  ++suspend_count_;
}

void TestMediaController::Resume() {
  ++resume_count_;
}

void TestMediaController::Stop() {
  ++stop_count_;
}

void TestMediaController::ToggleSuspendResume() {
  ++toggle_suspend_resume_count_;
}

void TestMediaController::AddObserver(
    mojo::PendingRemote<mojom::MediaControllerObserver> observer) {
  ++add_observer_count_;
  observers_.Add(std::move(observer));
}

void TestMediaController::PreviousTrack() {
  ++previous_track_count_;
}

void TestMediaController::NextTrack() {
  ++next_track_count_;
}

void TestMediaController::Seek(base::TimeDelta seek_time) {
  DCHECK(!seek_time.is_zero());

  if (seek_time.is_positive()) {
    ++seek_forward_count_;
  } else if (seek_time.is_negative()) {
    ++seek_backward_count_;
  }
}

void TestMediaController::SeekTo(base::TimeDelta seek_time) {
  seek_to_time_ = seek_time;
  ++seek_to_count_;
}

void TestMediaController::SkipAd() {
  ++skip_ad_count_;
}

void TestMediaController::EnterPictureInPicture() {
  ++enter_picture_in_picture_count_;
}

void TestMediaController::ExitPictureInPicture() {
  ++exit_picture_in_picture_count_;
}

void TestMediaController::Raise() {
  ++raise_count_;
}

void TestMediaController::RequestMediaRemoting() {
  ++request_media_remoting_count_;
}

void TestMediaController::SimulateMediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr session_info) {
  for (auto& observer : observers_)
    observer->MediaSessionInfoChanged(session_info.Clone());
}

void TestMediaController::SimulateMediaSessionActionsChanged(
    const std::vector<mojom::MediaSessionAction>& actions) {
  for (auto& observer : observers_)
    observer->MediaSessionActionsChanged(actions);
}

void TestMediaController::SimulateMediaSessionChanged(
    base::UnguessableToken token) {
  for (auto& observer : observers_)
    observer->MediaSessionChanged(token);
}

void TestMediaController::SimulateMediaSessionMetadataChanged(
    const media_session::MediaMetadata& meta_data) {
  for (auto& observer : observers_)
    observer->MediaSessionMetadataChanged(meta_data);
}

void TestMediaController::Flush() {
  observers_.FlushForTesting();
}

int TestMediaController::GetActiveObserverCount() {
  int count = 0;
  for (auto& observer : observers_) {
    if (observer.is_connected()) {
      count++;
    }
  }
  return count;
}

}  // namespace test
}  // namespace media_session
