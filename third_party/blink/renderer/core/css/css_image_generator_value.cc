/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_image_generator_value.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

using cssvalue::CSSConicGradientValue;
using cssvalue::CSSConstantGradientValue;
using cssvalue::CSSLinearGradientValue;
using cssvalue::CSSRadialGradientValue;

Image* GeneratedImageCache::GetImage(const gfx::SizeF& size) const {
  if (size.IsEmpty()) {
    return nullptr;
  }

  DCHECK(base::Contains(sizes_, size));
  GeneratedImageMap::const_iterator image_iter = images_.find(size);
  if (image_iter == images_.end()) {
    return nullptr;
  }
  return image_iter->value.get();
}

void GeneratedImageCache::PutImage(const gfx::SizeF& size,
                                   scoped_refptr<Image> image) {
  DCHECK(!size.IsEmpty());
  images_.insert(size, std::move(image));
}

void GeneratedImageCache::AddSize(const gfx::SizeF& size) {
  DCHECK(!size.IsEmpty());
  sizes_.insert(size);
}

void GeneratedImageCache::RemoveSize(const gfx::SizeF& size) {
  DCHECK(!size.IsEmpty());
  SECURITY_DCHECK(base::Contains(sizes_, size));
  bool fully_erased = sizes_.erase(size);
  if (fully_erased) {
    DCHECK(base::Contains(images_, size));
    images_.erase(images_.find(size));
  }
}

CSSImageGeneratorValue::CSSImageGeneratorValue(ClassType class_type)
    : CSSValue(class_type) {}

CSSImageGeneratorValue::~CSSImageGeneratorValue() = default;

void CSSImageGeneratorValue::AddClient(const ImageResourceObserver* client) {
  DCHECK(client);
  if (clients_.empty()) {
    DCHECK(!keep_alive_);
    keep_alive_ = this;
  }

  SizeAndCount& size_count =
      clients_.insert(client, SizeAndCount()).stored_value->value;
  size_count.count++;
}

void CSSImageGeneratorValue::RemoveClient(const ImageResourceObserver* client) {
  DCHECK(client);
  ClientSizeCountMap::iterator it = clients_.find(client);
  SECURITY_CHECK(it != clients_.end());

  SizeAndCount& size_count = it->value;
  if (!size_count.size.IsEmpty()) {
    cached_images_.RemoveSize(size_count.size);
    size_count.size = gfx::SizeF();
  }

  if (!--size_count.count) {
    clients_.erase(client);
  }

  if (clients_.empty()) {
    DCHECK(keep_alive_);
    keep_alive_.Clear();
  }
}

void CSSImageGeneratorValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(clients_);
  CSSValue::TraceAfterDispatch(visitor);
}

Image* CSSImageGeneratorValue::GetImage(const ImageResourceObserver* client,
                                        const gfx::SizeF& size) const {
  ClientSizeCountMap::iterator it = clients_.find(client);
  if (it != clients_.end()) {
    DCHECK(keep_alive_);
    SizeAndCount& size_count = it->value;
    if (size_count.size != size) {
      if (!size_count.size.IsEmpty()) {
        cached_images_.RemoveSize(size_count.size);
        size_count.size = gfx::SizeF();
      }

      if (!size.IsEmpty()) {
        cached_images_.AddSize(size);
        size_count.size = size;
      }
    }
  }
  return cached_images_.GetImage(size);
}

void CSSImageGeneratorValue::PutImage(const gfx::SizeF& size,
                                      scoped_refptr<Image> image) const {
  cached_images_.PutImage(size, std::move(image));
}

scoped_refptr<Image> CSSImageGeneratorValue::GetImage(
    const ImageResourceObserver& client,
    const Document& document,
    const ComputedStyle& style,
    const ContainerSizes& container_sizes,
    const gfx::SizeF& target_size) {
  switch (GetClassType()) {
    case kLinearGradientClass:
      return To<CSSLinearGradientValue>(this)->GetImage(
          client, document, style, container_sizes, target_size);
    case kPaintClass:
      return To<CSSPaintValue>(this)->GetImage(client, document, style,
                                               target_size);
    case kRadialGradientClass:
      return To<CSSRadialGradientValue>(this)->GetImage(
          client, document, style, container_sizes, target_size);
    case kConicGradientClass:
      return To<CSSConicGradientValue>(this)->GetImage(
          client, document, style, container_sizes, target_size);
    case kConstantGradientClass:
      return To<CSSConstantGradientValue>(this)->GetImage(
          client, document, style, container_sizes, target_size);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

bool CSSImageGeneratorValue::IsUsingCustomProperty(
    const AtomicString& custom_property_name,
    const Document& document) const {
  if (GetClassType() == kPaintClass) {
    return To<CSSPaintValue>(this)->IsUsingCustomProperty(custom_property_name,
                                                          document);
  }
  return false;
}

bool CSSImageGeneratorValue::IsUsingCurrentColor() const {
  switch (GetClassType()) {
    case kLinearGradientClass:
      return To<CSSLinearGradientValue>(this)->IsUsingCurrentColor();
    case kRadialGradientClass:
      return To<CSSRadialGradientValue>(this)->IsUsingCurrentColor();
    case kConicGradientClass:
      return To<CSSConicGradientValue>(this)->IsUsingCurrentColor();
    case kConstantGradientClass:
      return To<CSSConstantGradientValue>(this)->IsUsingCurrentColor();
    default:
      return false;
  }
}

bool CSSImageGeneratorValue::IsUsingContainerRelativeUnits() const {
  switch (GetClassType()) {
    case kLinearGradientClass:
      return To<CSSLinearGradientValue>(this)->IsUsingContainerRelativeUnits();
    case kRadialGradientClass:
      return To<CSSRadialGradientValue>(this)->IsUsingContainerRelativeUnits();
    case kConicGradientClass:
      return To<CSSConicGradientValue>(this)->IsUsingContainerRelativeUnits();
    default:
      return false;
  }
}

bool CSSImageGeneratorValue::KnownToBeOpaque(const Document& document,
                                             const ComputedStyle& style) const {
  switch (GetClassType()) {
    case kLinearGradientClass:
      return To<CSSLinearGradientValue>(this)->KnownToBeOpaque(document, style);
    case kPaintClass:
      return To<CSSPaintValue>(this)->KnownToBeOpaque(document, style);
    case kRadialGradientClass:
      return To<CSSRadialGradientValue>(this)->KnownToBeOpaque(document, style);
    case kConicGradientClass:
      return To<CSSConicGradientValue>(this)->KnownToBeOpaque(document, style);
    case kConstantGradientClass:
      return To<CSSConstantGradientValue>(this)->KnownToBeOpaque(document,
                                                                 style);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

}  // namespace blink
