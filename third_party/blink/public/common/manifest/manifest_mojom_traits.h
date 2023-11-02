// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/manifest/manifest.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace mojo {
namespace internal {

inline base::StringPiece16 TruncateString16(const std::u16string& string) {
  // We restrict the maximum length for all the strings inside the Manifest
  // when it is sent over Mojo. The renderer process truncates the strings
  // before sending the Manifest and the browser process validates that.
  return base::StringPiece16(string).substr(0, 4 * 1024);
}

inline absl::optional<base::StringPiece16> TruncateOptionalString16(
    const absl::optional<std::u16string>& string) {
  if (!string)
    return absl::nullopt;

  return TruncateString16(*string);
}

inline absl::optional<base::StringPiece16> ConvertAndTruncateOptionalString(
    const absl::optional<std::string>& string) {
  if (!string)
    return absl::nullopt;

  return TruncateOptionalString16(base::UTF8ToUTF16(string.value()));
}

}  // namespace internal

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestImageResourceDataView,
                 ::blink::Manifest::ImageResource> {
  static const GURL& src(const ::blink::Manifest::ImageResource& icon) {
    return icon.src;
  }

  static base::StringPiece16 type(
      const ::blink::Manifest::ImageResource& icon) {
    return internal::TruncateString16(icon.type);
  }
  static const std::vector<gfx::Size>& sizes(
      const ::blink::Manifest::ImageResource& icon) {
    return icon.sizes;
  }

  static const std::vector<::blink::mojom::ManifestImageResource_Purpose>&
  purpose(const ::blink::Manifest::ImageResource& icon) {
    return icon.purpose;
  }

  static bool Read(blink::mojom::ManifestImageResourceDataView data,
                   ::blink::Manifest::ImageResource* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestShortcutItemDataView,
                 ::blink::Manifest::ShortcutItem> {
  static base::StringPiece16 name(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return internal::TruncateString16(shortcut.name);
  }

  static absl::optional<base::StringPiece16> short_name(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return internal::TruncateOptionalString16(shortcut.short_name);
  }

  static absl::optional<base::StringPiece16> description(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return internal::TruncateOptionalString16(shortcut.description);
  }

  static const GURL& url(const ::blink::Manifest::ShortcutItem& shortcut) {
    return shortcut.url;
  }

  static const std::vector<::blink::Manifest::ImageResource>& icons(
      const ::blink::Manifest::ShortcutItem& shortcut) {
    return shortcut.icons;
  }

  static bool Read(blink::mojom::ManifestShortcutItemDataView data,
                   ::blink::Manifest::ShortcutItem* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestRelatedApplicationDataView,
                 ::blink::Manifest::RelatedApplication> {
  static absl::optional<base::StringPiece16> platform(
      const ::blink::Manifest::RelatedApplication& related_application) {
    return internal::TruncateOptionalString16(related_application.platform);
  }

  static const GURL& url(
      const ::blink::Manifest::RelatedApplication& related_application) {
    return related_application.url;
  }

  static absl::optional<base::StringPiece16> id(
      const ::blink::Manifest::RelatedApplication& related_application) {
    return internal::TruncateOptionalString16(related_application.id);
  }

  static bool Read(blink::mojom::ManifestRelatedApplicationDataView data,
                   ::blink::Manifest::RelatedApplication* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestFileFilterDataView,
                 ::blink::Manifest::FileFilter> {
  static base::StringPiece16 name(
      const ::blink::Manifest::FileFilter& share_target_file) {
    return internal::TruncateString16(share_target_file.name);
  }

  static const std::vector<base::StringPiece16> accept(
      const ::blink::Manifest::FileFilter& share_target_file) {
    std::vector<base::StringPiece16> accept_types;

    for (const std::u16string& accept_type : share_target_file.accept)
      accept_types.push_back(internal::TruncateString16(accept_type));

    return accept_types;
  }

  static bool Read(blink::mojom::ManifestFileFilterDataView data,
                   ::blink::Manifest::FileFilter* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestShareTargetParamsDataView,
                 ::blink::Manifest::ShareTargetParams> {
  static const absl::optional<base::StringPiece16> text(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return internal::TruncateOptionalString16(share_target_params.text);
  }
  static const absl::optional<base::StringPiece16> title(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return internal::TruncateOptionalString16(share_target_params.title);
  }
  static const absl::optional<base::StringPiece16> url(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return internal::TruncateOptionalString16(share_target_params.url);
  }
  static const std::vector<blink::Manifest::FileFilter>& files(
      const ::blink::Manifest::ShareTargetParams& share_target_params) {
    return share_target_params.files;
  }

  static bool Read(blink::mojom::ManifestShareTargetParamsDataView data,
                   ::blink::Manifest::ShareTargetParams* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestShareTargetDataView,
                 ::blink::Manifest::ShareTarget> {
  static const GURL& action(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.action;
  }
  static ::blink::mojom::ManifestShareTarget_Method method(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.method;
  }
  static ::blink::mojom::ManifestShareTarget_Enctype enctype(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.enctype;
  }
  static const ::blink::Manifest::ShareTargetParams& params(
      const ::blink::Manifest::ShareTarget& share_target) {
    return share_target.params;
  }
  static bool Read(blink::mojom::ManifestShareTargetDataView data,
                   ::blink::Manifest::ShareTarget* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestLaunchHandlerDataView,
                 ::blink::Manifest::LaunchHandler> {
  static blink::mojom::ManifestLaunchHandler::ClientMode client_mode(
      const ::blink::Manifest::LaunchHandler& launch_handler) {
    return launch_handler.client_mode;
  }

  static bool Read(blink::mojom::ManifestLaunchHandlerDataView data,
                   ::blink::Manifest::LaunchHandler* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ManifestTranslationItemDataView,
                 ::blink::Manifest::TranslationItem> {
  static absl::optional<base::StringPiece16> name(
      const ::blink::Manifest::TranslationItem& translation) {
    return internal::ConvertAndTruncateOptionalString(translation.name);
  }

  static absl::optional<base::StringPiece16> short_name(
      const ::blink::Manifest::TranslationItem& translation) {
    return internal::ConvertAndTruncateOptionalString(translation.short_name);
  }

  static absl::optional<base::StringPiece16> description(
      const ::blink::Manifest::TranslationItem& translation) {
    return internal::ConvertAndTruncateOptionalString(translation.description);
  }

  static bool Read(blink::mojom::ManifestTranslationItemDataView data,
                   ::blink::Manifest::TranslationItem* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::HomeTabParamsDataView,
                                        ::blink::Manifest::HomeTabParams> {
  static const std::vector<::blink::Manifest::ImageResource>& icons(
      const ::blink::Manifest::HomeTabParams& params) {
    return params.icons;
  }

  static bool Read(blink::mojom::HomeTabParamsDataView data,
                   ::blink::Manifest::HomeTabParams* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::NewTabButtonParamsDataView,
                 ::blink::Manifest::NewTabButtonParams> {
  static const absl::optional<GURL>& url(
      const ::blink::Manifest::NewTabButtonParams& params) {
    return params.url;
  }

  static bool Read(blink::mojom::NewTabButtonParamsDataView data,
                   ::blink::Manifest::NewTabButtonParams* out);
};

template <>
struct BLINK_COMMON_EXPORT UnionTraits<blink::mojom::HomeTabUnionDataView,
                                       ::blink::Manifest::TabStrip::HomeTab> {
  static blink::mojom::HomeTabUnionDataView::Tag GetTag(
      const ::blink::Manifest::TabStrip::HomeTab& value) {
    if (absl::holds_alternative<blink::mojom::TabStripMemberVisibility>(
            value)) {
      return blink::mojom::HomeTabUnion::Tag::kVisibility;
    } else {
      return blink::mojom::HomeTabUnion::Tag::kParams;
    }
  }

  static ::blink::mojom::TabStripMemberVisibility visibility(
      const ::blink::Manifest::TabStrip::HomeTab& value) {
    return absl::get<blink::mojom::TabStripMemberVisibility>(value);
  }

  static const ::blink::Manifest::HomeTabParams& params(
      const ::blink::Manifest::TabStrip::HomeTab& value) {
    return absl::get<blink::Manifest::HomeTabParams>(value);
  }

  static bool Read(blink::mojom::HomeTabUnionDataView data,
                   ::blink::Manifest::TabStrip::HomeTab* out);
};

template <>
struct BLINK_COMMON_EXPORT
    UnionTraits<blink::mojom::NewTabButtonUnionDataView,
                ::blink::Manifest::TabStrip::NewTabButton> {
  static blink::mojom::NewTabButtonUnionDataView::Tag GetTag(
      const ::blink::Manifest::TabStrip::NewTabButton& value) {
    if (absl::holds_alternative<blink::mojom::TabStripMemberVisibility>(
            value)) {
      return blink::mojom::NewTabButtonUnion::Tag::kVisibility;
    } else {
      return blink::mojom::NewTabButtonUnion::Tag::kParams;
    }
  }

  static ::blink::mojom::TabStripMemberVisibility visibility(
      const ::blink::Manifest::TabStrip::NewTabButton& value) {
    return absl::get<blink::mojom::TabStripMemberVisibility>(value);
  }

  static const ::blink::Manifest::NewTabButtonParams& params(
      const ::blink::Manifest::TabStrip::NewTabButton& value) {
    return absl::get<blink::Manifest::NewTabButtonParams>(value);
  }

  static bool Read(blink::mojom::NewTabButtonUnionDataView data,
                   ::blink::Manifest::TabStrip::NewTabButton* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::ManifestTabStripDataView,
                                        ::blink::Manifest::TabStrip> {
  static const ::blink::Manifest::TabStrip::HomeTab& home_tab(
      const ::blink::Manifest::TabStrip& tab_strip) {
    return tab_strip.home_tab;
  }

  static const ::blink::Manifest::TabStrip::NewTabButton& new_tab_button(
      const ::blink::Manifest::TabStrip& tab_strip) {
    return tab_strip.new_tab_button;
  }

  static bool Read(blink::mojom::ManifestTabStripDataView data,
                   ::blink::Manifest::TabStrip* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_MOJOM_TRAITS_H_
