// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"

#import "ios/chrome/browser/badges/ui_bundled/badge_type_util.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"

using infobars::InfoBar;

OVERLAY_USER_DATA_SETUP_IMPL(InfobarOverlayRequestConfig);

InfobarOverlayRequestConfig::InfobarOverlayRequestConfig(
    InfoBarIOS* infobar,
    InfobarOverlayType overlay_type,
    bool is_high_priority)
    : infobar_(infobar->GetWeakPtr()),
      infobar_type_(infobar->infobar_type()),
      has_badge_(BadgeTypeForInfobarType(infobar_type_) != kBadgeTypeNone),
      is_high_priority_(is_high_priority),
      overlay_type_(overlay_type) {}

InfobarOverlayRequestConfig::~InfobarOverlayRequestConfig() = default;
