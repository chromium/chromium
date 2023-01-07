// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/supplementary_branding.h"

#include <string>

#include "base/no_destructor.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/rlz_value_store.h"

namespace rlz_lib {

namespace {

std::string& GetSupplementaryBrandingStorage() {
  static base::NoDestructor<std::string> instance;
  return *instance;
}

}  // namespace

SupplementaryBranding::SupplementaryBranding(const char* brand)
    : lock_(new ScopedRlzValueStoreLock) {
  if (!lock_->GetStore())
    return;

  auto& supplementary_brand = GetSupplementaryBrandingStorage();
  if (!supplementary_brand.empty()) {
    ASSERT_STRING("ProductBranding: existing brand is not empty");
    return;
  }

  if (brand == nullptr || brand[0] == 0) {
    ASSERT_STRING("ProductBranding: new brand is empty");
    return;
  }

  supplementary_brand = brand;
}

SupplementaryBranding::~SupplementaryBranding() {
  if (lock_->GetStore())
    GetSupplementaryBrandingStorage().clear();
  delete lock_;
}

// static
const std::string& SupplementaryBranding::GetBrand() {
  return GetSupplementaryBrandingStorage();
}

}  // namespace rlz_lib
