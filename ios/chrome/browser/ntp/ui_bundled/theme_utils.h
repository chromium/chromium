// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_THEME_UTILS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_THEME_UTILS_H_

#import "components/sync/protocol/theme_types.pb.h"
#import "ui/color/color_provider_key.h"

// Converts a BrowserColorVariant proto enum value to the corresponding
// ColorProviderKey::SchemeVariant.
ui::ColorProviderKey::SchemeVariant ProtoEnumToSchemeVariant(
    sync_pb::UserColorTheme::BrowserColorVariant proto_variant);

// Converts a `SchemeVariant` into the corresponding `BrowserColorVariant` proto
// enum value.
sync_pb::UserColorTheme::BrowserColorVariant SchemeVariantToProtoEnum(
    ui::ColorProviderKey::SchemeVariant scheme_variant);

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_THEME_UTILS_H_
