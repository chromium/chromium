// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/promos_manager/constants.h"

#include "base/notreached.h"

namespace promos_manager {

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
Promo PromoForName(std::string promo) {
  if (promo == "promos_manager::Promo::Test") {
    return promos_manager::Promo::Test;
  } else {
    NOTREACHED();

    // Returns promos_manager::Promo::Test by default, but this should never be
    // reached!
    return promos_manager::Promo::Test;
  }
}

std::string NameForPromo(Promo promo) {
  switch (promo) {
    case promos_manager::Promo::Test:
      return "promos_manager::Promo::Test";
  }
}

}  // namespace promos_manager
