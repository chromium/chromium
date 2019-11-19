// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_RESOURCE_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class StyleSVGResource : public RefCounted<StyleSVGResource> {
  USING_FAST_MALLOC(StyleSVGResource);

 public:
  static scoped_refptr<StyleSVGResource> Create(SVGResource* resource,
                                                const AtomicString& url) {
    return base::AdoptRef(new StyleSVGResource(resource, url));
  }

  bool operator==(const StyleSVGResource& other) const {
    return resource_.Get() == other.resource_.Get();
  }

  void AddClient(SVGResourceClient& client) {
    if (resource_)
      resource_->AddClient(client);
  }
  void RemoveClient(SVGResourceClient& client) {
    if (resource_)
      resource_->RemoveClient(client);
  }

  SVGResource* Resource() const { return resource_; }
  const AtomicString& Url() const { return url_; }

 private:
  StyleSVGResource(SVGResource* resource, const AtomicString& url)
      : resource_(resource), url_(url) {}

  Persistent<SVGResource> resource_;
  const AtomicString url_;

  DISALLOW_COPY_AND_ASSIGN(StyleSVGResource);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_RESOURCE_H_
