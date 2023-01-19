// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/image_resource_type_converters.h"

#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

namespace {

using Purpose = blink::mojom::blink::ManifestImageResource::Purpose;
using blink::WebString;
using blink::WebVector;

// https://w3c.github.io/manifest/#sizes-member.
WTF::Vector<gfx::Size> ParseSizes(const WTF::String& sizes) {
  WebVector<gfx::Size> parsed_sizes = blink::WebIconSizesParser::ParseIconSizes(
      WebString::FromASCII(sizes.Ascii()));
  WTF::HashSet<std::pair<int, int>,
               PairHashTraits<IntWithZeroKeyHashTraits<int>,
                              IntWithZeroKeyHashTraits<int>>>
      unique_sizes;

  WTF::Vector<gfx::Size> results;
  for (const auto& size : parsed_sizes) {
    auto add_result =
        unique_sizes.insert(std::make_pair(size.width(), size.height()));
    if (add_result.is_new_entry) {
      results.push_back(size);
    }
  }

  return results;
}

// https://w3c.github.io/manifest/#purpose-member.
WTF::Vector<Purpose> ParsePurpose(const WTF::String& purpose) {
  WTF::HashSet<WTF::String> valid_purpose_set;
  WTF::Vector<Purpose> results;

  // Only two purpose values are defined.
  valid_purpose_set.ReserveCapacityForSize(2u);
  results.ReserveInitialCapacity(2u);

  WTF::Vector<WTF::String> split_purposes;
  purpose.LowerASCII().Split(' ', false /* allow_empty_entries */,
                             split_purposes);

  for (const WTF::String& lowercase_purpose : split_purposes) {
    Purpose purpose_enum;
    if (lowercase_purpose == "any") {
      purpose_enum = Purpose::ANY;
    } else if (lowercase_purpose == "monochrome") {
      purpose_enum = Purpose::MONOCHROME;
    } else if (lowercase_purpose == "maskable") {
      purpose_enum = Purpose::MASKABLE;
    } else {
      // TODO(rayankans): Issue developer warning.
      continue;
    }

    auto add_result = valid_purpose_set.insert(lowercase_purpose);
    if (add_result.is_new_entry) {
      results.push_back(purpose_enum);
    } else {
      // TODO(rayankans): Issue developer warning.
    }
  }

  return results;
}

WTF::String ParseType(const WTF::String& type) {
  if (type.IsNull() || type.empty())
    return "";

  if (!blink::IsSupportedMimeType(type.Ascii())) {
    // TODO(rayankans): Issue developer warning.
    return "";
  }
  return type;
}

}  // namespace

blink::mojom::blink::ManifestImageResourcePtr TypeConverter<
    blink::mojom::blink::ManifestImageResourcePtr,
    blink::ManifestImageResource*>::Convert(const blink::ManifestImageResource*
                                                image_resource) {
  auto image_resource_ptr = blink::mojom::blink::ManifestImageResource::New();
  image_resource_ptr->src = blink::KURL(image_resource->src());
  if (image_resource->hasSizes())
    image_resource_ptr->sizes = ParseSizes(image_resource->sizes());
  if (image_resource->hasPurpose())
    image_resource_ptr->purpose = ParsePurpose(image_resource->purpose());

  if (image_resource->hasType())
    image_resource_ptr->type = ParseType(image_resource->type());
  else
    image_resource_ptr->type = "";

  return image_resource_ptr;
}

}  // namespace mojo

namespace blink {

Manifest::ImageResource ConvertManifestImageResource(
    const ManifestImageResource* icon) {
  Manifest::ImageResource manifest_icon;
  manifest_icon.src = GURL(icon->src().Utf8());
  if (icon->hasType())
    manifest_icon.type = WebString(mojo::ParseType(icon->type())).Utf16();

  // Parse 'purpose'
  if (icon->hasPurpose()) {
    // ParsePurpose() would've weeded out any purposes that're not ANY or
    // MONOCHROME.
    for (auto purpose : mojo::ParsePurpose(icon->purpose())) {
      switch (purpose) {
        case mojo::Purpose::ANY:
          manifest_icon.purpose.emplace_back(
              mojom::ManifestImageResource_Purpose::ANY);
          break;
        case mojo::Purpose::MONOCHROME:
          manifest_icon.purpose.emplace_back(
              mojom::ManifestImageResource_Purpose::MONOCHROME);
          break;
        case mojo::Purpose::MASKABLE:
          manifest_icon.purpose.emplace_back(
              mojom::ManifestImageResource_Purpose::MASKABLE);
          break;
      }
    }
  }
  // Parse 'sizes'.
  if (icon->hasSizes()) {
    for (const auto& size : mojo::ParseSizes(icon->sizes())) {
      manifest_icon.sizes.emplace_back(size);
    }
  }

  return manifest_icon;
}

}  // namespace blink
