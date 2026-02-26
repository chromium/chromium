// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/persistent_display_layout_manager.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "remoting/base/async_file_util.h"
#include "remoting/base/logging.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

namespace {

// Delay to throttle writes to the display layout file.
constexpr base::TimeDelta kWriteDisplayLayoutDelay = base::Seconds(10);

bool IsLayoutValid(const protocol::VideoLayout& layout) {
  if (layout.video_track_size() < 1 || layout.video_track_size() > 4) {
    LOG(ERROR) << "Invalid track count: " << layout.video_track_size();
    return false;
  }

  for (const auto& track : layout.video_track()) {
    if (track.position_x() < 0 || track.position_y() < 0) {
      LOG(ERROR) << "Invalid position: " << track.position_x() << ","
                 << track.position_y();
      return false;
    }
    if (track.width() < 1 || track.width() > 32767 || track.height() < 1 ||
        track.height() > 32767) {
      LOG(ERROR) << "Invalid size: " << track.width() << "x" << track.height();
      return false;
    }
    if (track.x_dpi() < 1) {
      LOG(ERROR) << "Invalid x_dpi: " << track.x_dpi();
      return false;
    }
    if (track.has_y_dpi() && track.y_dpi() != track.x_dpi()) {
      LOG(ERROR) << "Invalid y_dpi: " << track.y_dpi()
                 << " (must match x_dpi or be unset)";
      return false;
    }
  }
  return true;
}

}  // namespace

PersistentDisplayLayoutManager::PersistentDisplayLayoutManager(
    const base::FilePath& display_layout_file_path,
    std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor,
    base::WeakPtr<DesktopResizer> desktop_resizer)
    : display_layout_file_path_(display_layout_file_path),
      display_info_monitor_(std::move(display_info_monitor)),
      desktop_resizer_(std::move(desktop_resizer)),
      write_display_layout_timer_(
          FROM_HERE,
          kWriteDisplayLayoutDelay,
          base::BindRepeating(
              &PersistentDisplayLayoutManager::WriteDisplayLayout,
              base::Unretained(this))) {}

PersistentDisplayLayoutManager::~PersistentDisplayLayoutManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PersistentDisplayLayoutManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReadFileAsync(
      display_layout_file_path_,
      base::BindOnce(&PersistentDisplayLayoutManager::OnDisplayLayoutFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PersistentDisplayLayoutManager::OnDisplayLayoutFileLoaded(
    base::FileErrorOr<std::string> load_file_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ApplyDisplayLayout(load_file_result);
  display_info_monitor_->AddCallback(base::BindRepeating(
      &PersistentDisplayLayoutManager::OnDisplayInfoReceived,
      weak_ptr_factory_.GetWeakPtr()));
  display_info_monitor_->Start();
}

void PersistentDisplayLayoutManager::OnDisplayInfoReceived() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto* display_info = display_info_monitor_->GetLatestDisplayInfo();
  DCHECK(display_info);
  latest_display_layout_ = display_info->GetVideoLayoutProto();
  // This either starts or delays the timer.
  write_display_layout_timer_.Reset();
}

void PersistentDisplayLayoutManager::ApplyDisplayLayout(
    const base::FileErrorOr<std::string>& load_file_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (load_file_result.has_value()) {
    auto display_layout_from_file = std::make_unique<protocol::VideoLayout>();
    if (display_layout_from_file->ParseFromString(*load_file_result)) {
      // Run some simple checks to make sure the file is not corrupted.
      if (!IsLayoutValid(*display_layout_from_file)) {
        LOG(ERROR) << "Invalid display layout from file.";
        return;
      }
      for (protocol::VideoTrackLayout& track :
           *display_layout_from_file->mutable_video_track()) {
        // Clear the screen ID to indicate that a new display should be created.
        // See comment in protobuf.
        track.clear_screen_id();
      }
      if (desktop_resizer_) {
        HOST_LOG << "Applying display layout from file: "
                 << display_layout_file_path_;
        desktop_resizer_->SetVideoLayout(*display_layout_from_file);
      }
    } else {
      LOG(ERROR) << "Failed to parse display layout.";
    }
  } else if (load_file_result.error() !=
             base::File::Error::FILE_ERROR_NOT_FOUND) {
    LOG(ERROR) << "Failed to read file " << display_layout_file_path_ << ": "
               << base::File::ErrorToString(load_file_result.error());
  }
}

void PersistentDisplayLayoutManager::WriteDisplayLayout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WriteFileAsync(
      display_layout_file_path_, latest_display_layout_->SerializeAsString(),
      base::BindOnce(
          [](const base::FilePath& display_layout_file_path,
             base::FileErrorOr<void> result) {
            if (!result.has_value()) {
              LOG(ERROR) << "Failed to write display layout to file "
                         << display_layout_file_path << ": "
                         << base::File::ErrorToString(result.error());
            }
          },
          display_layout_file_path_));
}

}  // namespace remoting
