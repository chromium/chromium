// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_RESOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class SVGResource;
class SVGResourceClient;

class StyleSVGResource : public RefCounted<StyleSVGResource> {
  USING_FAST_MALLOC(StyleSVGResource);

 public:
  static scoped_refptr<StyleSVGResource> Create(SVGResource* resource,
                                                const AtomicString& url) {
    return base::AdoptRef(new StyleSVGResource(resource, url));
  }
  CORE_EXPORT ~StyleSVGResource();

  bool operator==(const StyleSVGResource& other) const {
    return resource_.Get() == other.resource_.Get();
  }

  void AddClient(SVGResourceClient& client);
  void RemoveClient(SVGResourceClient& client);

  SVGResource* Resource() const { return resource_; }
  const AtomicString& Url() const { return url_; }

 private:
  StyleSVGResource(SVGResource* resource, const AtomicString& url);

  Persistent<SVGResource> resource_;
  const AtomicString url_;

  StyleSVGResource(const StyleSVGResource&) = delete;
  StyleSVGResource& operator=(const StyleSVGResource&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_RESOURCE_H_
