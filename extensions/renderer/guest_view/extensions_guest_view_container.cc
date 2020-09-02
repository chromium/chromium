// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/extensions_guest_view_container.h"

namespace extensions {

ExtensionsGuestViewContainer::ExtensionsGuestViewContainer(
    content::RenderFrame* render_frame)
    : GuestViewContainer(render_frame) {
}

ExtensionsGuestViewContainer::~ExtensionsGuestViewContainer() = default;

void ExtensionsGuestViewContainer::OnDestroy(bool embedder_frame_destroyed) {
}

}  // namespace extensions
