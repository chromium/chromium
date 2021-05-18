// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/badges/badge_type_util.h"

#include <ostream>

#include "base/notreached.h"

BadgeType BadgeTypeForInfobarType(InfobarType infobar_type) {
  switch (infobar_type) {
    case InfobarType::kInfobarTypePasswordSave:
      return BadgeType::kBadgeTypePasswordSave;
    case InfobarType::kInfobarTypePasswordUpdate:
      return BadgeType::kBadgeTypePasswordUpdate;
    case InfobarType::kInfobarTypeSaveAutofillAddressProfile:
      return BadgeType::kBadgeTypeSaveAddressProfile;
    case InfobarType::kInfobarTypeSaveCard:
      return BadgeType::kBadgeTypeSaveCard;
    case InfobarType::kInfobarTypeTranslate:
      return BadgeType::kBadgeTypeTranslate;
    default:
      return BadgeType::kBadgeTypeNone;
  }
}

InfobarType InfobarTypeForBadgeType(BadgeType badge_type) {
  switch (badge_type) {
    case BadgeType::kBadgeTypePasswordSave:
      return InfobarType::kInfobarTypePasswordSave;
    case BadgeType::kBadgeTypePasswordUpdate:
      return InfobarType::kInfobarTypePasswordUpdate;
    case BadgeType::kBadgeTypeSaveAddressProfile:
      return InfobarType::kInfobarTypeSaveAutofillAddressProfile;
    case BadgeType::kBadgeTypeSaveCard:
      return InfobarType::kInfobarTypeSaveCard;
    case BadgeType::kBadgeTypeTranslate:
      return InfobarType::kInfobarTypeTranslate;
    default:
      NOTREACHED() << "Unsupported badge type.";
      return InfobarType::kInfobarTypeConfirm;
  }
}
