// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_manifest.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"

namespace blink {

LinkManifest::LinkManifest(HTMLLinkElement* owner) : LinkResource(owner) {}

LinkManifest::~LinkManifest() = default;

void LinkManifest::Process() {
  if (!owner_ || !owner_->GetDocument().GetFrame())
    return;

  owner_->GetDocument().GetFrame()->Client()->DispatchDidChangeManifest();
}

bool LinkManifest::HasLoaded() const {
  return false;
}

void LinkManifest::OwnerRemoved() {
  Process();
}

}  // namespace blink
