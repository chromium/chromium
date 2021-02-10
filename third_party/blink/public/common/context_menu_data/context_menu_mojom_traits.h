// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_CONTEXT_MENU_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_CONTEXT_MENU_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::CustomContextMenuItemType, blink::MenuItem::Type> {
  static blink::mojom::CustomContextMenuItemType ToMojom(
      blink::MenuItem::Type input);
  static bool FromMojom(blink::mojom::CustomContextMenuItemType input,
                        blink::MenuItem::Type* output);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ImpressionDataView, blink::Impression> {
  static url::Origin conversion_destination(const blink::Impression& r) {
    return r.conversion_destination;
  }

  static base::Optional<url::Origin> reporting_origin(
      const blink::Impression& r) {
    return r.reporting_origin;
  }

  static uint64_t impression_data(const blink::Impression& r) {
    return r.impression_data;
  }

  static base::Optional<base::TimeDelta> expiry(const blink::Impression& r) {
    return r.expiry;
  }

  static bool Read(blink::mojom::ImpressionDataView r, blink::Impression* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::CustomContextMenuItemDataView, blink::MenuItem> {
  static base::string16 label(const blink::MenuItem& r) { return r.label; }

  static base::string16 icon(const blink::MenuItem& r) { return r.icon; }

  static base::string16 tool_tip(const blink::MenuItem& r) {
    return r.tool_tip;
  }

  static blink::MenuItem::Type type(const blink::MenuItem& r) { return r.type; }

  static int32_t action(const blink::MenuItem& r) { return r.action; }

  static bool rtl(const blink::MenuItem& r) { return r.rtl; }

  static bool has_directional_override(const blink::MenuItem& r) {
    return r.has_directional_override;
  }

  static bool enabled(const blink::MenuItem& r) { return r.enabled; }

  static bool checked(const blink::MenuItem& r) { return r.checked; }

  static std::vector<blink::MenuItem> submenu(const blink::MenuItem& r) {
    return r.submenu;
  }

  static bool Read(blink::mojom::CustomContextMenuItemDataView r,
                   blink::MenuItem* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::UntrustworthyContextMenuParamsDataView,
                 blink::UntrustworthyContextMenuParams> {
  static blink::mojom::ContextMenuDataMediaType media_type(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.media_type;
  }

  static int x(const blink::UntrustworthyContextMenuParams& r) { return r.x; }

  static int y(const blink::UntrustworthyContextMenuParams& r) { return r.y; }

  static const GURL link_url(const blink::UntrustworthyContextMenuParams& r) {
    return r.link_url;
  }

  static base::string16 link_text(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.link_text;
  }

  static base::Optional<blink::Impression> impression(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.impression;
  }

  static const GURL unfiltered_link_url(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.unfiltered_link_url;
  }

  static const GURL src_url(const blink::UntrustworthyContextMenuParams& r) {
    return r.src_url;
  }

  static bool has_image_contents(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.has_image_contents;
  }

  static int media_flags(const blink::UntrustworthyContextMenuParams& r) {
    return r.media_flags;
  }

  static base::string16 selection_text(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.selection_text;
  }

  static base::string16 title_text(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.title_text;
  }

  static base::string16 alt_text(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.alt_text;
  }

  static base::string16 suggested_filename(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.suggested_filename;
  }

  static base::string16 misspelled_word(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.misspelled_word;
  }

  static std::vector<base::string16> dictionary_suggestions(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.dictionary_suggestions;
  }

  static bool spellcheck_enabled(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.spellcheck_enabled;
  }

  static bool is_editable(const blink::UntrustworthyContextMenuParams& r) {
    return r.is_editable;
  }

  static int writing_direction_default(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.writing_direction_default;
  }

  static int writing_direction_left_to_right(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.writing_direction_left_to_right;
  }

  static int writing_direction_right_to_left(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.writing_direction_right_to_left;
  }

  static int edit_flags(const blink::UntrustworthyContextMenuParams& r) {
    return r.edit_flags;
  }

  static std::string frame_charset(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.frame_charset;
  }

  static network::mojom::ReferrerPolicy referrer_policy(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.referrer_policy;
  }

  static const GURL& link_followed(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.link_followed;
  }

  static std::vector<blink::MenuItem> custom_items(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.custom_items;
  }

  static ui::MenuSourceType source_type(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.source_type;
  }

  static blink::mojom::ContextMenuDataInputFieldType input_field_type(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.input_field_type;
  }

  static gfx::Rect selection_rect(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.selection_rect;
  }

  static int selection_start_offset(
      const blink::UntrustworthyContextMenuParams& r) {
    return r.selection_start_offset;
  }

  static bool Read(blink::mojom::UntrustworthyContextMenuParamsDataView r,
                   blink::UntrustworthyContextMenuParams* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_CONTEXT_MENU_MOJOM_TRAITS_H_
