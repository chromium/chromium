// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <windows.h>

#include <map>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace {
// TODO(jamiewalch): Use the correct DPI for the mode: http://crbug.com/172405.
const int kDefaultDPI = 96;
}  // namespace

namespace remoting {

// Provide comparison operation for ScreenResolution so we can use it in
// std::map.
static inline bool operator<(const ScreenResolution& a,
                             const ScreenResolution& b) {
  if (a.dimensions().width() != b.dimensions().width()) {
    return a.dimensions().width() < b.dimensions().width();
  }
  if (a.dimensions().height() != b.dimensions().height()) {
    return a.dimensions().height() < b.dimensions().height();
  }
  if (a.dpi().x() != b.dpi().x()) {
    return a.dpi().x() < b.dpi().x();
  }
  return a.dpi().y() < b.dpi().y();
}

class DesktopResizerWin : public DesktopResizer {
 public:
  DesktopResizerWin();
  DesktopResizerWin(const DesktopResizerWin&) = delete;
  DesktopResizerWin& operator=(const DesktopResizerWin&) = delete;
  ~DesktopResizerWin() override;

  // DesktopResizer interface.
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
  void UpdateBestModeForResolution(const DEVMODE& current_mode,
                                   const DEVMODE& candidate_mode);
  static bool IsResizeSupported();

  // Calls EnumDisplaySettingsEx() for the primary monitor.
  // Returns false if |mode_number| does not exist.
  static bool GetPrimaryDisplayMode(DWORD mode_number,
                                    DWORD flags,
                                    DEVMODE* mode);

  // Returns true if the mode has width, height, bits-per-pixel, frequency
  // and orientation fields.
  static bool IsModeValid(const DEVMODE& mode);

  // Returns the width & height of |mode|, or 0x0 if they are missing.
  static ScreenResolution GetModeResolution(const DEVMODE& mode);

  std::map<ScreenResolution, DEVMODE> best_mode_for_resolution_;
  DEVMODE initial_mode_;
};

DesktopResizerWin::DesktopResizerWin() {
  if (!GetPrimaryDisplayMode(ENUM_CURRENT_SETTINGS, 0, &initial_mode_) ||
      !IsModeValid(initial_mode_)) {
    LOG(ERROR) << "GetPrimaryDisplayMode failed. Resize will not prefer "
               << "initial orientation or frequency settings.";
    initial_mode_.dmFields = 0;
  }
}

DesktopResizerWin::~DesktopResizerWin() {}

ScreenResolution DesktopResizerWin::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  DEVMODE current_mode;
  if (GetPrimaryDisplayMode(ENUM_CURRENT_SETTINGS, 0, &current_mode) &&
      IsModeValid(current_mode)) {
    return GetModeResolution(current_mode);
  }
  return ScreenResolution();
}

std::list<ScreenResolution> DesktopResizerWin::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  if (!IsResizeSupported()) {
    return std::list<ScreenResolution>();
  }

  // Enumerate the resolutions to return, and where there are multiple modes of
  // the same resolution, store the one most closely matching the current mode
  // in |best_mode_for_resolution_|.
  DEVMODE current_mode;
  if (!GetPrimaryDisplayMode(ENUM_CURRENT_SETTINGS, 0, &current_mode) ||
      !IsModeValid(current_mode)) {
    return std::list<ScreenResolution>();
  }

  best_mode_for_resolution_.clear();
  for (DWORD i = 0;; ++i) {
    DEVMODE candidate_mode;
    if (!GetPrimaryDisplayMode(i, EDS_ROTATEDMODE, &candidate_mode)) {
      break;
    }
    UpdateBestModeForResolution(current_mode, candidate_mode);
  }

  std::list<ScreenResolution> resolutions;
  for (const auto& kv : best_mode_for_resolution_) {
    resolutions.push_back(kv.first);
  }
  return resolutions;
}

void DesktopResizerWin::SetResolution(const ScreenResolution& resolution,
                                      webrtc::ScreenId screen_id) {
  if (best_mode_for_resolution_.count(resolution) == 0) {
    return;
  }

  DEVMODE new_mode = best_mode_for_resolution_[resolution];
  DWORD result = ChangeDisplaySettings(&new_mode, CDS_FULLSCREEN);
  if (result != DISP_CHANGE_SUCCESSFUL) {
    LOG(ERROR) << "SetResolution failed: " << result;
  }
}

void DesktopResizerWin::RestoreResolution(const ScreenResolution& original,
                                          webrtc::ScreenId screen_id) {
  // Restore the display mode based on the registry configuration.
  DWORD result = ChangeDisplaySettings(nullptr, 0);
  if (result != DISP_CHANGE_SUCCESSFUL) {
    LOG(ERROR) << "RestoreResolution failed: " << result;
  }
}

void DesktopResizerWin::SetVideoLayout(const protocol::VideoLayout& layout) {
  NOTIMPLEMENTED();
}

void DesktopResizerWin::UpdateBestModeForResolution(
    const DEVMODE& current_mode,
    const DEVMODE& candidate_mode) {
  // Ignore modes missing the fields that we expect.
  if (!IsModeValid(candidate_mode)) {
    LOG(INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
              << candidate_mode.dmPelsHeight << ": invalid fields " << std::hex
              << candidate_mode.dmFields;
    return;
  }

  // Ignore modes with differing bits-per-pixel.
  if (candidate_mode.dmBitsPerPel != current_mode.dmBitsPerPel) {
    LOG(INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
              << candidate_mode.dmPelsHeight << ": mismatched BPP: expected "
              << current_mode.dmFields << " but got "
              << candidate_mode.dmFields;
    return;
  }

  // If there are multiple modes with the same dimensions:
  // - Prefer the modes which match either the initial (preferred) or the
  //   current rotation.
  // - Among those, prefer modes which match the initial (preferred) or the
  //   current frequency.
  // - Otherwise, prefer modes with a higher frequency.
  ScreenResolution candidate_resolution = GetModeResolution(candidate_mode);
  if (best_mode_for_resolution_.count(candidate_resolution) != 0) {
    DEVMODE best_mode = best_mode_for_resolution_[candidate_resolution];

    bool best_mode_matches_initial_orientation =
        (initial_mode_.dmDisplayOrientation & DM_DISPLAYORIENTATION) &&
        (best_mode.dmDisplayOrientation == initial_mode_.dmDisplayOrientation);
    bool candidate_mode_matches_initial_orientation =
        candidate_mode.dmDisplayOrientation ==
        initial_mode_.dmDisplayOrientation;
    if (best_mode_matches_initial_orientation &&
        !candidate_mode_matches_initial_orientation) {
      LOG(INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                << candidate_mode.dmPelsHeight
                << ": mode matching initial orientation already found.";
      return;
    }

    bool best_mode_matches_current_orientation =
        best_mode.dmDisplayOrientation == current_mode.dmDisplayOrientation;
    bool candidate_mode_matches_current_orientation =
        candidate_mode.dmDisplayOrientation ==
        current_mode.dmDisplayOrientation;
    if (best_mode_matches_current_orientation &&
        !candidate_mode_matches_initial_orientation &&
        !candidate_mode_matches_current_orientation) {
      LOG(INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                << candidate_mode.dmPelsHeight
                << ": mode matching current orientation already found.";
      return;
    }

    bool best_mode_matches_initial_frequency =
        (initial_mode_.dmDisplayOrientation & DM_DISPLAYFREQUENCY) &&
        (best_mode.dmDisplayFrequency == initial_mode_.dmDisplayFrequency);
    bool candidate_mode_matches_initial_frequency =
        candidate_mode.dmDisplayFrequency == initial_mode_.dmDisplayFrequency;
    if (best_mode_matches_initial_frequency &&
        !candidate_mode_matches_initial_frequency) {
      LOG(INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                << candidate_mode.dmPelsHeight
                << ": mode matching initial frequency already found.";
      return;
    }

    bool best_mode_matches_current_frequency =
        best_mode.dmDisplayFrequency == current_mode.dmDisplayFrequency;
    bool candidate_mode_matches_current_frequency =
        candidate_mode.dmDisplayFrequency == current_mode.dmDisplayFrequency;
    if (best_mode_matches_current_frequency &&
        !candidate_mode_matches_initial_frequency &&
        !candidate_mode_matches_current_frequency) {
      LOG(INFO) << "Ignoring mode " << candidate_mode.dmPelsWidth << "x"
                << candidate_mode.dmPelsHeight
                << ": mode matching current frequency already found.";
      return;
    }
  }

  // If we haven't seen this resolution before, or if it's a better match than
  // one we enumerated previously, save it.
  best_mode_for_resolution_[candidate_resolution] = candidate_mode;
}

// static
bool DesktopResizerWin::IsResizeSupported() {
  // Resize is supported only on single-monitor systems.
  return GetSystemMetrics(SM_CMONITORS) == 1;
}

// static
bool DesktopResizerWin::GetPrimaryDisplayMode(DWORD mode_number,
                                              DWORD flags,
                                              DEVMODE* mode) {
  memset(mode, 0, sizeof(DEVMODE));
  mode->dmSize = sizeof(DEVMODE);
  if (!EnumDisplaySettingsEx(nullptr, mode_number, mode, flags)) {
    return false;
  }
  return true;
}

// static
bool DesktopResizerWin::IsModeValid(const DEVMODE& mode) {
  const DWORD kRequiredFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
                                DM_DISPLAYFREQUENCY | DM_DISPLAYORIENTATION;
  return (mode.dmFields & kRequiredFields) == kRequiredFields;
}

// static
ScreenResolution DesktopResizerWin::GetModeResolution(const DEVMODE& mode) {
  DCHECK(IsModeValid(mode));
  return ScreenResolution(
      webrtc::DesktopSize(mode.dmPelsWidth, mode.dmPelsHeight),
      webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
}

std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return base::WrapUnique(new DesktopResizerWin);
}

}  // namespace remoting
