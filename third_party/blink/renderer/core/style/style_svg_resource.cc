// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_svg_resource.h"

#include "third_party/blink/renderer/core/svg/svg_resource.h"

namespace blink {

StyleSVGResource::StyleSVGResource(SVGResource* resource,
                                   const AtomicString& url)
    : resource_(resource), url_(url) {}

StyleSVGResource::~StyleSVGResource() = default;

void StyleSVGResource::AddClient(SVGResourceClient& client) {
  if (resource_) {
    resource_->AddClient(client);
  }
}

void StyleSVGResource::RemoveClient(SVGResourceClient& client) {
  if (resource_) {
    resource_->RemoveClient(client);
  }
}

}  // namespace blink
