// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_

#include "components/guest_view/renderer/guest_view_container.h"

namespace extensions {

class ExtensionsGuestViewContainer : public guest_view::GuestViewContainer {
 public:
  explicit ExtensionsGuestViewContainer(content::RenderFrame* render_frame);
  ExtensionsGuestViewContainer(const ExtensionsGuestViewContainer&) = delete;
  ExtensionsGuestViewContainer& operator=(const ExtensionsGuestViewContainer&) =
      delete;

 protected:
  ~ExtensionsGuestViewContainer() override;

 private:
  // GuestViewContainer implementation.
  void OnDestroy(bool embedder_frame_destroyed) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_
