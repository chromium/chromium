/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/style/content_data.h"

#include <memory>
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ContentData* ContentData::Clone() const {
  ContentData* result = CloneInternal();

  ContentData* last_new_data = result;
  for (const ContentData* content_data = Next(); content_data;
       content_data = content_data->Next()) {
    ContentData* new_data = content_data->CloneInternal();
    last_new_data->SetNext(new_data);
    last_new_data = last_new_data->Next();
  }

  return result;
}

void ContentData::Trace(Visitor* visitor) const {
  visitor->Trace(next_);
}

LayoutObject* ImageContentData::CreateLayoutObject(LayoutObject& owner) const {
  LayoutImage* image = LayoutImage::CreateAnonymous(owner.GetDocument());
  bool match_parent_size = image_ && image_->IsGeneratedImage();
  image->SetPseudoElementStyle(owner, match_parent_size);
  if (image_) {
    image->SetImageResource(
        MakeGarbageCollected<LayoutImageResourceStyleImage>(image_.Get()));
  } else {
    image->SetImageResource(MakeGarbageCollected<LayoutImageResource>());
  }
  return image;
}

void ImageContentData::Trace(Visitor* visitor) const {
  visitor->Trace(image_);
  ContentData::Trace(visitor);
}

LayoutObject* TextContentData::CreateLayoutObject(LayoutObject& owner) const {
  LayoutObject* layout_object =
      LayoutTextFragment::CreateAnonymous(owner.GetDocument(), text_);
  layout_object->SetPseudoElementStyle(owner);
  return layout_object;
}

LayoutObject* AltTextContentData::CreateLayoutObject(
    LayoutObject& owner) const {
  // Does not require a layout object. Calling site should first check
  // IsAltContentData() before calling this method.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

LayoutObject* CounterContentData::CreateLayoutObject(
    LayoutObject& owner) const {
  LayoutObject* layout_object =
      MakeGarbageCollected<LayoutCounter>(owner.GetDocument(), *this);
  layout_object->SetPseudoElementStyle(owner);
  return layout_object;
}

void CounterContentData::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  ContentData::Trace(visitor);
}

LayoutObject* QuoteContentData::CreateLayoutObject(LayoutObject& owner) const {
  LayoutObject* layout_object =
      MakeGarbageCollected<LayoutQuote>(owner, quote_);
  layout_object->SetPseudoElementStyle(owner);
  return layout_object;
}

LayoutObject* NoneContentData::CreateLayoutObject(LayoutObject& owner) const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace blink
