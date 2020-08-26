// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_OZONE_H_
#define REMOTING_HOST_DESKTOP_RESIZER_OZONE_H_

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "remoting/host/desktop_resizer.h"

namespace remoting {

class DesktopResizerOzone : public DesktopResizer {
 public:
  DesktopResizerOzone();
  DesktopResizerOzone(const DesktopResizerOzone&) = delete;
  DesktopResizerOzone& operator=(const DesktopResizerOzone&) = delete;
  ~DesktopResizerOzone() override;

  // DesktopResizer:
  ScreenResolution GetCurrentResolution() override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) override;
  void SetResolution(const ScreenResolution& resolution) override;
  void RestoreResolution(const ScreenResolution& original) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_OZONE_H_
