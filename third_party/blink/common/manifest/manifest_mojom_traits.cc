// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_mojom_traits.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"
#include "url/url_util.h"

namespace mojo {
namespace {

// A wrapper around std::optional<std::u16string> so a custom StructTraits
// specialization can enforce maximum string length.
struct TruncatedString16 {
  std::optional<std::u16string> string;
};

std::optional<std::string> ConvertOptionalString16(
    const TruncatedString16& string) {
  return string.string.has_value()
             ? std::make_optional(base::UTF16ToUTF8(string.string.value()))
             : std::nullopt;
}

}  // namespace

template <>
struct StructTraits<mojo_base::mojom::String16DataView, TruncatedString16> {
  static void SetToNull(TruncatedString16* output) { output->string.reset(); }

  static bool Read(mojo_base::mojom::String16DataView input,
                   TruncatedString16* output) {
    if (input.is_null()) {
      output->string.reset();
      return true;
    }
    mojo::ArrayDataView<uint16_t> buffer_view;
    input.GetDataDataView(&buffer_view);
    if (buffer_view.size() > 4 * 1024)
      return false;

    output->string.emplace();
    return StructTraits<mojo_base::mojom::String16DataView,
                        std::u16string>::Read(input, &output->string.value());
  }
};

bool StructTraits<blink::mojom::ManifestImageResourceDataView,
                  ::blink::Manifest::ImageResource>::
    Read(blink::mojom::ManifestImageResourceDataView data,
         ::blink::Manifest::ImageResource* out) {
  if (!data.ReadSrc(&out->src))
    return false;

  TruncatedString16 string;
  if (!data.ReadType(&string))
    return false;

  if (!string.string)
    return false;

  out->type = *std::move(string.string);

  if (!data.ReadSizes(&out->sizes))
    return false;

  if (!data.ReadPurpose(&out->purpose))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestShortcutItemDataView,
                  ::blink::Manifest::ShortcutItem>::
    Read(blink::mojom::ManifestShortcutItemDataView data,
         ::blink::Manifest::ShortcutItem* out) {
  if (!data.ReadName(&out->name))
    return false;

  TruncatedString16 string;
  if (!data.ReadShortName(&string))
    return false;
  out->short_name = std::move(string.string);

  if (!data.ReadDescription(&string))
    return false;
  out->description = std::move(string.string);

  if (!data.ReadUrl(&out->url))
    return false;

  if (!data.ReadIcons(&out->icons))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestRelatedApplicationDataView,
                  ::blink::Manifest::RelatedApplication>::
    Read(blink::mojom::ManifestRelatedApplicationDataView data,
         ::blink::Manifest::RelatedApplication* out) {
  TruncatedString16 string;
  if (!data.ReadPlatform(&string))
    return false;
  out->platform = std::move(string.string);

  std::optional<GURL> url;
  if (!data.ReadUrl(&url))
    return false;
  out->url = std::move(url).value_or(GURL());

  if (!data.ReadId(&string))
    return false;
  out->id = std::move(string.string);

  return !out->url.is_empty() || out->id;
}

bool StructTraits<blink::mojom::ManifestFileFilterDataView,
                  ::blink::Manifest::FileFilter>::
    Read(blink::mojom::ManifestFileFilterDataView data,
         ::blink::Manifest::FileFilter* out) {
  TruncatedString16 name;
  if (!data.ReadName(&name))
    return false;

  if (!name.string)
    return false;

  out->name = *std::move(name.string);

  if (!data.ReadAccept(&out->accept))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestShareTargetParamsDataView,
                  ::blink::Manifest::ShareTargetParams>::
    Read(blink::mojom::ManifestShareTargetParamsDataView data,
         ::blink::Manifest::ShareTargetParams* out) {
  TruncatedString16 string;
  if (!data.ReadText(&string))
    return false;
  out->text = std::move(string.string);

  if (!data.ReadTitle(&string))
    return false;
  out->title = std::move(string.string);

  if (!data.ReadUrl(&string))
    return false;
  out->url = std::move(string.string);

  if (!data.ReadFiles(&out->files))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestShareTargetDataView,
                  ::blink::Manifest::ShareTarget>::
    Read(blink::mojom::ManifestShareTargetDataView data,
         ::blink::Manifest::ShareTarget* out) {
  if (!data.ReadAction(&out->action))
    return false;

  if (!data.ReadMethod(&out->method))
    return false;

  if (!data.ReadEnctype(&out->enctype))
    return false;

  return data.ReadParams(&out->params);
}

bool StructTraits<blink::mojom::ManifestLaunchHandlerDataView,
                  ::blink::Manifest::LaunchHandler>::
    Read(blink::mojom::ManifestLaunchHandlerDataView data,
         ::blink::Manifest::LaunchHandler* out) {
  if (!data.ReadClientMode(&out->client_mode))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestTranslationItemDataView,
                  ::blink::Manifest::TranslationItem>::
    Read(blink::mojom::ManifestTranslationItemDataView data,
         ::blink::Manifest::TranslationItem* out) {
  TruncatedString16 string;
  if (!data.ReadName(&string))
    return false;
  out->name = ConvertOptionalString16(string);

  if (!data.ReadShortName(&string))
    return false;
  out->short_name = ConvertOptionalString16(string);

  if (!data.ReadDescription(&string))
    return false;
  out->description = ConvertOptionalString16(string);

  return true;
}

bool StructTraits<blink::mojom::HomeTabParamsDataView,
                  ::blink::Manifest::HomeTabParams>::
    Read(blink::mojom::HomeTabParamsDataView data,
         ::blink::Manifest::HomeTabParams* out) {
  if (!data.ReadIcons(&out->icons)) {
    return false;
  }

  if (!data.ReadScopePatterns(&out->scope_patterns)) {
    return false;
  }

  return true;
}

bool StructTraits<blink::mojom::NewTabButtonParamsDataView,
                  ::blink::Manifest::NewTabButtonParams>::
    Read(blink::mojom::NewTabButtonParamsDataView data,
         ::blink::Manifest::NewTabButtonParams* out) {
  return data.ReadUrl(&out->url);
}

blink::mojom::HomeTabUnionDataView::Tag
UnionTraits<blink::mojom::HomeTabUnionDataView,
            ::blink::Manifest::TabStrip::HomeTab>::
    GetTag(const ::blink::Manifest::TabStrip::HomeTab& value) {
  if (absl::holds_alternative<blink::mojom::TabStripMemberVisibility>(value)) {
    return blink::mojom::HomeTabUnion::Tag::kVisibility;
  } else {
    return blink::mojom::HomeTabUnion::Tag::kParams;
  }
}

bool UnionTraits<blink::mojom::HomeTabUnionDataView,
                 ::blink::Manifest::TabStrip::HomeTab>::
    Read(blink::mojom::HomeTabUnionDataView data,
         blink::Manifest::TabStrip::HomeTab* out) {
  switch (data.tag()) {
    case blink::mojom::HomeTabUnionDataView::Tag::kVisibility:
      ::blink::mojom::TabStripMemberVisibility visibility;
      if (!data.ReadVisibility(&visibility))
        return false;
      *out = visibility;
      return true;
    case blink::mojom::HomeTabUnionDataView::Tag::kParams:
      ::blink::Manifest::HomeTabParams params;
      if (!data.ReadParams(&params))
        return false;
      *out = params;
      return true;
  }
  return false;
}

bool StructTraits<blink::mojom::ManifestTabStripDataView,
                  ::blink::Manifest::TabStrip>::
    Read(blink::mojom::ManifestTabStripDataView data,
         ::blink::Manifest::TabStrip* out) {
  if (!data.ReadHomeTab(&out->home_tab))
    return false;

  if (!data.ReadNewTabButton(&out->new_tab_button))
    return false;

  return true;
}

}  // namespace mojo
