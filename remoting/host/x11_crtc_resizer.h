// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_X11_CRTC_RESIZER_H_
#define REMOTING_HOST_X11_CRTC_RESIZER_H_

#include <list>

#include "base/memory/raw_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/gfx/x/randr.h"

namespace remoting {

// Helper class for DesktopResizerX11 which handles much of the logic
// for arranging and resizing a set of active CRTCs.
class X11CrtcResizer {
 public:
  X11CrtcResizer(x11::RandR::GetScreenResourcesCurrentReply* resources,
                 x11::RandR* randr);
  X11CrtcResizer(const X11CrtcResizer&) = delete;
  X11CrtcResizer& operator=(const X11CrtcResizer&) = delete;
  ~X11CrtcResizer();

  // Queries the server for all active CRTCs and stores them in |active_crtcs_|.
  void FetchActiveCrtcs();

  // Searches |active_crts_| to find the one for the given output. If none is
  // found, this returns kDisabledCrtc. Since the information on all CRTCs is
  // already fetched, this method avoids a server round-trip from using
  // RRGetOutputInfo.
  x11::RandR::Crtc GetCrtcForOutput(x11::RandR::Output output) const;

  // Disables a CRTC. A disabled CRTC no longer has a mode selected (allowing
  // the CRD mode to be removed). It also no longer occupies space in the root
  // window, which may allow the root window to be resized. This does not
  // modify |active_crtcs_|, so the stored information can be used to enable
  // the CRTC again.
  void DisableCrtc(x11::RandR::Crtc crtc);

  // This operates only on |active_crtcs_| without making any X server calls.
  // It sets the new mode and width/height for the given CRTC. And it changes
  // any xy-offsets as needed, to avoid overlaps between CRTCs. Every modified
  // CRTC is marked by setting its |changed| flag.
  void UpdateActiveCrtcs(x11::RandR::Crtc crtc,
                         x11::RandR::Mode mode,
                         const webrtc::DesktopSize& new_size);

  // Disables any CRTCs whose |changed| flag is true.
  void DisableChangedCrtcs();

  // Returns the bounding box of |active_crtcs_| from their current xy-offsets
  // and sizes.
  webrtc::DesktopSize GetBoundingBox() const;

  // Applies any changed CRTCs back to the X server. This will re-enable any
  // outputs/CRTCs that were disabled.
  void ApplyActiveCrtcs();

 private:
  // Information for an active CRTC, from RRGetCrtcInfo response. When
  // modifying a CRTC, the information here can reconstruct the original
  // properties that should not be changed.
  struct CrtcInfo {
    CrtcInfo();
    CrtcInfo(x11::RandR::Crtc crtc,
             int16_t x,
             int16_t y,
             uint16_t width,
             uint16_t height,
             x11::RandR::Mode mode,
             x11::RandR::Rotation rotation,
             std::vector<x11::RandR::Output>&& outputs);
    CrtcInfo(const CrtcInfo&);
    CrtcInfo(CrtcInfo&&);
    CrtcInfo& operator=(const CrtcInfo&);
    CrtcInfo& operator=(CrtcInfo&&);
    ~CrtcInfo();

    x11::RandR::Crtc crtc;
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    x11::RandR::Mode mode;
    x11::RandR::Rotation rotation;
    std::vector<x11::RandR::Output> outputs;

    // True if any values are different from the response from RRGetCrtcInfo.
    bool changed = false;
  };

  raw_ptr<x11::RandR::GetScreenResourcesCurrentReply> resources_;
  raw_ptr<x11::RandR> randr_;

  // Information on all CRTCs that are currently enabled (including the CRTC
  // being resized). This is only used during a resize operation, while the X
  // server is grabbed. Some of these xy-positions may be adjusted. At the end,
  // the root window size will be set to the bounding rectangle of all these
  // CRTCs. If a CRTC needs to be disabled temporarily, the list entry will be
  // preserved so that the CRTC can be re-enabled with the original and new
  // properties.
  std::list<CrtcInfo> active_crtcs_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_X11_CRTC_RESIZER_H_
