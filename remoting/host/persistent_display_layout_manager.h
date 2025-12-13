// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PERSISTENT_DISPLAY_LAYOUT_MANAGER_H_
#define REMOTING_HOST_PERSISTENT_DISPLAY_LAYOUT_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

// Class that reads and applies any previously stored display layouts from a
// file, monitors display layout changes, and writes the latest layout back to
// said file.
class PersistentDisplayLayoutManager {
 public:
  PersistentDisplayLayoutManager(
      const base::FilePath& display_layout_file_path,
      std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor,
      base::WeakPtr<DesktopResizer> desktop_resizer);
  PersistentDisplayLayoutManager(const PersistentDisplayLayoutManager&) =
      delete;
  PersistentDisplayLayoutManager& operator=(
      const PersistentDisplayLayoutManager&) = delete;
  ~PersistentDisplayLayoutManager();

  // Reads and applies any previously stored display layouts, from
  // `display_layout_file_path`, then starts monitoring display layout changes
  // and writing them back to `display_layout_file_path`.
  // If no file exists at `display_layout_file_path`, or it fails to be read,
  // `default_layout` will be applied.
  void Start(std::unique_ptr<protocol::VideoLayout> default_layout);

 private:
  void OnDisplayLayoutFileLoaded(
      std::unique_ptr<protocol::VideoLayout> default_layout,
      base::FileErrorOr<std::string> load_file_result);
  void OnDisplayInfoReceived();
  void ApplyDisplayLayout(
      std::unique_ptr<protocol::VideoLayout> default_layout,
      const base::FileErrorOr<std::string>& load_file_result);
  void WriteDisplayLayout();

  base::FilePath display_layout_file_path_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtr<DesktopResizer> desktop_resizer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<protocol::VideoLayout> latest_display_layout_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RetainingOneShotTimer write_display_layout_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PersistentDisplayLayoutManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_PERSISTENT_DISPLAY_LAYOUT_MANAGER_H_
