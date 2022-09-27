// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_description_type_converter.h"

#include "third_party/blink/public/mojom/content_index/content_index.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_icon_definition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace mojo {

namespace {

blink::mojom::ContentCategory GetContentCategory(const WTF::String& category) {
  if (category == "")
    return blink::mojom::ContentCategory::NONE;
  if (category == "homepage")
    return blink::mojom::ContentCategory::HOME_PAGE;
  if (category == "article")
    return blink::mojom::ContentCategory::ARTICLE;
  if (category == "video")
    return blink::mojom::ContentCategory::VIDEO;
  if (category == "audio")
    return blink::mojom::ContentCategory::AUDIO;

  NOTREACHED();
  return blink::mojom::ContentCategory::NONE;
}

WTF::String GetContentCategory(blink::mojom::ContentCategory category) {
  switch (category) {
    case blink::mojom::ContentCategory::NONE:
      return "";
    case blink::mojom::ContentCategory::HOME_PAGE:
      return "homepage";
    case blink::mojom::ContentCategory::ARTICLE:
      return "article";
    case blink::mojom::ContentCategory::VIDEO:
      return "video";
    case blink::mojom::ContentCategory::AUDIO:
      return "audio";
  }
}

}  // namespace

blink::mojom::blink::ContentDescriptionPtr TypeConverter<
    blink::mojom::blink::ContentDescriptionPtr,
    const blink::ContentDescription*>::Convert(const blink::ContentDescription*
                                                   description) {
  auto result = blink::mojom::blink::ContentDescription::New();
  result->id = description->id();
  result->title = description->title();
  result->description = description->description();
  result->category = GetContentCategory(description->category());
  for (const auto& icon : description->icons()) {
    result->icons.push_back(blink::mojom::blink::ContentIconDefinition::New(
        icon->src(), icon->hasSizes() ? icon->sizes() : String(),
        icon->hasType() ? icon->type() : String()));
  }
  result->launch_url = description->url();

  return result;
}

blink::ContentDescription*
TypeConverter<blink::ContentDescription*,
              blink::mojom::blink::ContentDescriptionPtr>::
    Convert(const blink::mojom::blink::ContentDescriptionPtr& description) {
  auto* result = blink::MakeGarbageCollected<blink::ContentDescription>();
  result->setId(description->id);
  result->setTitle(description->title);
  result->setDescription(description->description);
  result->setCategory(GetContentCategory(description->category));

  blink::HeapVector<blink::Member<blink::ContentIconDefinition>> blink_icons;
  for (const auto& icon : description->icons) {
    auto* blink_icon = blink::ContentIconDefinition::Create();
    blink_icon->setSrc(icon->src);
    if (!icon->sizes.IsNull())
      blink_icon->setSizes(icon->sizes);
    if (!icon->type.IsNull())
      blink_icon->setType(icon->type);
    blink_icons.push_back(blink_icon);
  }
  result->setIcons(blink_icons);

  result->setUrl(description->launch_url);
  return result;
}

}  // namespace mojo
