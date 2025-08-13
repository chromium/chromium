// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
#define REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"

namespace remoting {

class GnomeDesktopResizer : public DesktopResizer {
 public:
  explicit GnomeDesktopResizer(
      base::WeakPtr<PipewireCaptureStreamManager> stream_manager);
  GnomeDesktopResizer(const GnomeDesktopResizer&) = delete;
  GnomeDesktopResizer& operator=(const GnomeDesktopResizer&) = delete;
  ~GnomeDesktopResizer() override;

  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override;
  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override;
  void RestoreResolution(const ScreenResolution& original,
                         webrtc::ScreenId screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& layout) override;

 private:
  void OnAddStreamResult(PipewireCaptureStreamManager::AddStreamResult result);

  base::WeakPtr<PipewireCaptureStreamManager> stream_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<GnomeDesktopResizer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
