// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"

#import "base/notreached.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "ui/color/color_provider_key.h"

ui::ColorProviderKey::SchemeVariant ProtoEnumToSchemeVariant(
    sync_pb::UserColorTheme::BrowserColorVariant proto_variant) {
  switch (proto_variant) {
    case sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT:
      return ui::ColorProviderKey::SchemeVariant::kTonalSpot;
    case sync_pb::UserColorTheme_BrowserColorVariant_NEUTRAL:
      return ui::ColorProviderKey::SchemeVariant::kNeutral;
    case sync_pb::UserColorTheme_BrowserColorVariant_VIBRANT:
      return ui::ColorProviderKey::SchemeVariant::kVibrant;
    case sync_pb::UserColorTheme_BrowserColorVariant_EXPRESSIVE:
      return ui::ColorProviderKey::SchemeVariant::kExpressive;
    case sync_pb::UserColorTheme_BrowserColorVariant_SYSTEM:
    case sync_pb::
        UserColorTheme_BrowserColorVariant_BROWSER_COLOR_VARIANT_UNSPECIFIED:
    default:
      NOTREACHED();
  }
}

sync_pb::UserColorTheme::BrowserColorVariant SchemeVariantToProtoEnum(
    ui::ColorProviderKey::SchemeVariant scheme_variant) {
  switch (scheme_variant) {
    case ui::ColorProviderKey::SchemeVariant::kTonalSpot:
      return sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT;
    case ui::ColorProviderKey::SchemeVariant::kNeutral:
      return sync_pb::UserColorTheme_BrowserColorVariant_NEUTRAL;
    case ui::ColorProviderKey::SchemeVariant::kVibrant:
      return sync_pb::UserColorTheme_BrowserColorVariant_VIBRANT;
    case ui::ColorProviderKey::SchemeVariant::kExpressive:
      return sync_pb::UserColorTheme_BrowserColorVariant_EXPRESSIVE;
    default:
      return sync_pb::
          UserColorTheme_BrowserColorVariant_BROWSER_COLOR_VARIANT_UNSPECIFIED;
  }
}
