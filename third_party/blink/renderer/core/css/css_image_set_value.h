/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_SET_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_SET_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSImageSetOptionValue;
class CSSLengthResolver;
class ResourceFetcher;
class StyleImage;
class StyleResolverState;

class CORE_EXPORT CSSImageSetValue : public CSSValueList {
 public:
  explicit CSSImageSetValue();
  ~CSSImageSetValue();

  CSSImageSetValue(StyleImage* cached_image,
                   float device_scale_factor,
                   HeapVector<Member<const CSSImageSetOptionValue>>&& options)
      : CSSValueList(kImageSetClass, kCommaSeparator),
        cached_image_(cached_image),
        cached_device_scale_factor_(device_scale_factor),
        options_(std::move(options)) {}

  bool IsCachePending(ResourceFetcher* fetcher,
                      const float device_scale_factor) const;
  StyleImage* CachedImage(ResourceFetcher* fetcher,
                          const float device_scale_factor) const;
  StyleImage* CacheImage(ResourceFetcher* fetcher,
                         StyleImage*,
                         const float device_scale_factor);

  const CSSImageSetOptionValue* GetBestOption(const CSSLengthResolver&,
                                              const float device_scale_factor);

  String CustomCSSText() const;

  bool HasFailedOrCanceledSubresources(ResourceFetcher*) const;

  const CSSImageSetValue& ResolveValuesIfNeeded(
      const StyleResolverState&) const;
  CSSImageSetValue& ResolveValuesIfNeeded(const StyleResolverState&);

  bool HasRandomFunctions() const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  CSSImageSetValue* ResolveValuesAndCreateCopyIfNeeded(
      const StyleResolverState&) const;

  // Storage class so that we can keep multiple values, including a Member, in
  // a HeapHashMap keyed by a WeakMember.
  class CachedImageAndScale final
      : public GarbageCollected<CachedImageAndScale> {
   public:
    CachedImageAndScale(StyleImage* image, float device_scale_factor)
        : image(image), device_scale_factor(device_scale_factor) {}

    void Trace(Visitor* visitor) const { visitor->Trace(image); }

    Member<StyleImage> image;
    float device_scale_factor = 1.0f;
  };

  Member<StyleImage> cached_image_;
  float cached_device_scale_factor_{1.0f};
  HeapHashMap<WeakMember<ResourceFetcher>, Member<CachedImageAndScale>>
      cached_images_;

  HeapVector<Member<const CSSImageSetOptionValue>> options_;
};

template <>
struct DowncastTraits<CSSImageSetValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsImageSetValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_SET_VALUE_H_
