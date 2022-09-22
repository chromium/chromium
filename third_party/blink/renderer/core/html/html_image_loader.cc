/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/html/html_image_loader.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"

namespace blink {

HTMLImageLoader::HTMLImageLoader(Element* element) : ImageLoader(element) {}

HTMLImageLoader::~HTMLImageLoader() = default;

void HTMLImageLoader::DispatchLoadEvent() {
  RESOURCE_LOADING_DVLOG(1) << "HTMLImageLoader::dispatchLoadEvent " << this;

  // HTMLVideoElement uses this class to load the poster image, but it should
  // not fire events for loading or failure.
  if (IsA<HTMLVideoElement>(*GetElement()))
    return;

  bool error_occurred = GetContent()->ErrorOccurred();
  if (IsA<HTMLObjectElement>(*GetElement()) && !error_occurred) {
    // An <object> considers a 404 to be an error and should fire onerror.
    error_occurred = (GetContent()->GetResponse().HttpStatusCode() >= 400);
  }
  GetElement()->DispatchEvent(*Event::Create(
      error_occurred ? event_type_names::kError : event_type_names::kLoad));
}

void HTMLImageLoader::NoImageResourceToLoad() {
  // FIXME: Use fallback content even when there is no alt-text. The only
  // blocker is the large amount of rebaselining it requires.
  if (To<HTMLElement>(GetElement())->AltText().empty())
    return;

  if (auto* image = DynamicTo<HTMLImageElement>(GetElement()))
    image->EnsureCollapsedOrFallbackContent();
  else if (auto* input = DynamicTo<HTMLInputElement>(GetElement()))
    input->EnsureFallbackContent();
}

void HTMLImageLoader::ImageNotifyFinished(ImageResourceContent*) {
  ImageResourceContent* cached_image = GetContent();
  Element* element = GetElement();
  ImageLoader::ImageNotifyFinished(cached_image);

  bool load_error = cached_image->ErrorOccurred();
  if (auto* image = DynamicTo<HTMLImageElement>(*element)) {
    if (load_error) {
      image->EnsureCollapsedOrFallbackContent();
    } else {
      if (cached_image->IsAdResource())
        image->SetIsAdRelated();
      image->EnsurePrimaryContent();
    }
  }

  if (auto* input = DynamicTo<HTMLInputElement>(*element)) {
    if (load_error)
      input->EnsureFallbackContent();
    else
      input->EnsurePrimaryContent();
  }

  auto* html_object_element = DynamicTo<HTMLObjectElement>(element);
  if ((load_error || cached_image->GetResponse().HttpStatusCode() >= 400) &&
      html_object_element) {
    // https://whatwg.org/C/iframe-embed-object.html#the-object-element does not
    // specify dispatching an error event on image decode failure and simply
    // jumps straight to the fallback step.
    //
    // Interestingly enough, Blink still fires an error event in this case since
    // the ImageLoader base class will dispatch an error event itself directly.
    html_object_element->RenderFallbackContent(
        HTMLObjectElement::ErrorEventPolicy::kDoNotDispatch);
  }
}

}  // namespace blink
