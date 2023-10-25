// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROXY_SVG_RESOURCE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROXY_SVG_RESOURCE_CLIENT_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/core/svg/svg_resource_client.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_counted_set.h"

namespace blink {

// A SVGResourceClients that proxies notifications to ImageResourceObservers.
class ProxySVGResourceClient final
    : public GarbageCollected<ProxySVGResourceClient>,
      public SVGResourceClient {
 public:
  explicit ProxySVGResourceClient(const CSSValue& value) : css_value_(value) {}

  bool AddClient(ImageResourceObserver* observer) {
    const bool was_empty = clients_.empty();
    clients_.insert(observer);
    return was_empty;
  }
  bool RemoveClient(ImageResourceObserver* observer) {
    clients_.erase(observer);
    return clients_.empty();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(clients_);
    visitor->Trace(css_value_);
    SVGResourceClient::Trace(visitor);
  }

 private:
  void ResourceContentChanged(SVGResource*) override {
    for (auto& entry : clients_) {
      entry.key->ImageChanged(static_cast<WrappedImagePtr>(css_value_.Get()),
                              ImageResourceObserver::CanDeferInvalidation::kNo);
    }
  }

  HeapHashCountedSet<Member<ImageResourceObserver>> clients_;
  Member<const CSSValue> css_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PROXY_SVG_RESOURCE_CLIENT_H_
