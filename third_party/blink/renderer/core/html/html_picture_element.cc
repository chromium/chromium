// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_picture_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_source_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/image_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLPictureElement::HTMLPictureElement(Document& document)
    : HTMLElement(html_names::kPictureTag, document) {}

void HTMLPictureElement::SourceOrMediaChanged() {
  for (HTMLImageElement* image_element =
           Traversal<HTMLImageElement>::FirstChild(*this);
       image_element; image_element = Traversal<HTMLImageElement>::NextSibling(
                          *image_element)) {
    image_element->SelectSourceURL(ImageLoader::kUpdateNormal);
  }
}

void HTMLPictureElement::RemoveListenerFromSourceChildren() {
  for (HTMLSourceElement* source_element =
           Traversal<HTMLSourceElement>::FirstChild(*this);
       source_element;
       source_element =
           Traversal<HTMLSourceElement>::NextSibling(*source_element)) {
    source_element->RemoveMediaQueryListListener();
  }
}

void HTMLPictureElement::AddListenerToSourceChildren() {
  for (HTMLSourceElement* source_element =
           Traversal<HTMLSourceElement>::FirstChild(*this);
       source_element;
       source_element =
           Traversal<HTMLSourceElement>::NextSibling(*source_element)) {
    source_element->AddMediaQueryListListener();
  }
}

Node::InsertionNotificationRequest HTMLPictureElement::InsertedInto(
    ContainerNode& insertion_point) {
  UseCounter::Count(GetDocument(), WebFeature::kPicture);
  return HTMLElement::InsertedInto(insertion_point);
}

}  // namespace blink
