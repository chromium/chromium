// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PORTAL_DISPLAY_INFO_LOADER_H_
#define REMOTING_HOST_LINUX_PORTAL_DISPLAY_INFO_LOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/desktop_display_info_loader.h"
#include "remoting/host/linux/capture_stream_manager.h"

namespace remoting {

class PortalDisplayInfoLoader : public DesktopDisplayInfoLoader {
 public:
  // `stream_manager` must outlive `this`.
  explicit PortalDisplayInfoLoader(CaptureStreamManager& stream_manager);
  PortalDisplayInfoLoader(const PortalDisplayInfoLoader&) = delete;
  PortalDisplayInfoLoader& operator=(const PortalDisplayInfoLoader&) = delete;
  ~PortalDisplayInfoLoader() override;

  // DesktopDisplayInfoLoader interface.
  DesktopDisplayInfo GetCurrentDisplayInfo() override;

  base::WeakPtr<PortalDisplayInfoLoader> GetWeakPtr();

 private:
  raw_ptr<CaptureStreamManager> stream_manager_;
  base::WeakPtrFactory<PortalDisplayInfoLoader> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PORTAL_DISPLAY_INFO_LOADER_H_
