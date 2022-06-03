// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/doh_provider_entry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(DohProviderListTest, GetDohProviderList) {
  const DohProviderEntry::List& list = DohProviderEntry::GetList();
  EXPECT_FALSE(list.empty());
}

}  // namespace
}  // namespace net
