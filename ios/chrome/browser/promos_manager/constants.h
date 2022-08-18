// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_

#include <string>

namespace promos_manager {

enum class Promo {
  Test = 0,  // Test promo used for testing purposes (e.g. unit tests)
};

// Returns string representation of promos_manager::Promo `promo`.
std::string NameForPromo(Promo promo);

// Returns promos_manager::Promo for string `promo`.
Promo PromoForName(std::string promo);

}  // namespace promos_manager

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_
