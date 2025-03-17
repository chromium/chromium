// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/known_legacy_scope_domains_delegate.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Verify that prefs are written and read correctly.
TEST(KnownLegacyScopeDomainsPrefDelegateTest, WriteAndReadPrefs) {
  TestingPrefServiceSimple pref_service;
  KnownLegacyScopeDomainsPrefDelegate delegate(&pref_service);
  delegate.RegisterPrefs(pref_service.registry());

  // Verify that the pref is empty to begin with.
  ASSERT_EQ(delegate.GetLegacyDomains().size(), 0u);

  base::Value::Dict dict;
  dict.Set("example.com", base::Value());
  dict.Set("example2.com", base::Value());
  delegate.SetLegacyDomains(std::move(dict));

  // Verify that the pref was written correctly.
  EXPECT_EQ(delegate.GetLegacyDomains().size(), 2u);
}

}  // namespace

}  // namespace network
