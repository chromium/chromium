// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/doh_provider_entry.h"

#include <set>
#include <string>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(DohProviderListTest, GetDohProviderList) {
  const DohProviderEntry::List& list = DohProviderEntry::GetList();
  EXPECT_FALSE(list.empty());
}

TEST(DohProviderListTest, ProviderNamesAreUnique) {
  std::set<std::string> names;
  for (const DohProviderEntry* entry : DohProviderEntry::GetList()) {
    EXPECT_FALSE(entry->provider.empty());
    auto [_, did_insert] = names.insert(entry->provider);
    EXPECT_TRUE(did_insert);
  }
}

TEST(DohProviderListTest, UiNamesAreUniqueOrEmpty) {
  std::set<std::string> ui_names;
  for (const DohProviderEntry* entry : DohProviderEntry::GetList()) {
    if (entry->ui_name.empty())
      continue;
    auto [_, did_insert] = ui_names.insert(entry->ui_name);
    EXPECT_TRUE(did_insert) << "UI name was not unique: " << entry->ui_name;
  }
}

TEST(DohProviderListTest, NonEmptyDnsOverTlsHostnames) {
  for (const DohProviderEntry* entry : DohProviderEntry::GetList()) {
    SCOPED_TRACE(entry->provider);
    for (const std::string& s : entry->dns_over_tls_hostnames) {
      EXPECT_FALSE(s.empty());
    }
  }
}

}  // namespace
}  // namespace net
