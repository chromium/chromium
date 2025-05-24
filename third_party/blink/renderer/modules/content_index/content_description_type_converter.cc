// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_description_type_converter.h"

#include "third_party/blink/public/mojom/content_index/content_index.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_category.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_icon_definition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace mojo {

namespace {

blink::mojom::blink::ContentCategory GetContentCategory(
    blink::V8ContentCategory::Enum category) {
  switch (category) {
    case blink::V8ContentCategory::Enum::k:
      return blink::mojom::blink::ContentCategory::NONE;
    case blink::V8ContentCategory::Enum::kHomepage:
      return blink::mojom::blink::ContentCategory::HOME_PAGE;
    case blink::V8ContentCategory::Enum::kArticle:
      return blink::mojom::blink::ContentCategory::ARTICLE;
    case blink::V8ContentCategory::Enum::kVideo:
      return blink::mojom::blink::ContentCategory::VIDEO;
    case blink::V8ContentCategory::Enum::kAudio:
      return blink::mojom::blink::ContentCategory::AUDIO;
  }
  NOTREACHED();
}

blink::V8ContentCategory::Enum GetContentCategory(
    blink::mojom::blink::ContentCategory category) {
  switch (category) {
    case blink::mojom::blink::ContentCategory::NONE:
      return blink::V8ContentCategory::Enum::k;
    case blink::mojom::blink::ContentCategory::HOME_PAGE:
      return blink::V8ContentCategory::Enum::kHomepage;
    case blink::mojom::blink::ContentCategory::ARTICLE:
      return blink::V8ContentCategory::Enum::kArticle;
    case blink::mojom::blink::ContentCategory::VIDEO:
      return blink::V8ContentCategory::Enum::kVideo;
    case blink::mojom::blink::ContentCategory::AUDIO:
      return blink::V8ContentCategory::Enum::kAudio;
  }
  NOTREACHED();
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
  result->category = GetContentCategory(description->category().AsEnum());
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
