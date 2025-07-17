// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DISPLAY_INFO_LOADER_H_
#define REMOTING_HOST_LINUX_GNOME_DISPLAY_INFO_LOADER_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/desktop_display_info_loader.h"

namespace remoting {

class GnomeInteractionStrategy;

class GnomeDisplayInfoLoader : public DesktopDisplayInfoLoader {
 public:
  explicit GnomeDisplayInfoLoader(
      base::WeakPtr<GnomeInteractionStrategy> session);
  GnomeDisplayInfoLoader(const GnomeDisplayInfoLoader&) = delete;
  GnomeDisplayInfoLoader& operator=(const GnomeDisplayInfoLoader&) = delete;
  ~GnomeDisplayInfoLoader() override;

  DesktopDisplayInfo GetCurrentDisplayInfo() override;

 private:
  base::WeakPtr<GnomeInteractionStrategy> session_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DISPLAY_INFO_LOADER_H_
