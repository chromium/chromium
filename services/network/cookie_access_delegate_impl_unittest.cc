// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_access_delegate_impl.h"

#include <optional>

#include "base/test/task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "services/network/first_party_sets/first_party_sets_manager.h"
#include "services/network/public/mojom/cookie_manager.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using testing::_;
using testing::FieldsAre;
using testing::IsEmpty;
using testing::Optional;

class CookieAccessDelegateImplTest : public testing::Test {
 public:
  CookieAccessDelegateImplTest()
      : delegate_(mojom::CookieAccessDelegateType::ALWAYS_LEGACY,
                  /*first_party_sets_access_delegate=*/nullptr,
                  /*cookie_settings=*/nullptr) {}

 protected:
  CookieAccessDelegateImpl& delegate() { return delegate_; }

 private:
  CookieAccessDelegateImpl delegate_;
};

TEST_F(CookieAccessDelegateImplTest, NullFirstPartySetsManager) {
  net::SchemefulSite site(GURL("https://site.test"));

  // Since the first_party_sets_manager pointer is nullptr, none of the
  // callbacks should ever be called, and the return values should all be
  // non-nullopt.

  // Same as the default ctor, but just to be explicit:
  net::FirstPartySetMetadata expected_metadata(
      /*frame_entry=*/nullptr,
      /*top_frame_entry=*/nullptr);
  EXPECT_THAT(
      delegate().ComputeFirstPartySetMetadataMaybeAsync(
          site, &site,
          base::BindOnce(
              [](net::FirstPartySetMetadata,
                 net::FirstPartySetsCacheFilter::MatchInfo) { FAIL(); })),
      Optional(std::make_pair(std::ref(expected_metadata),
                              net::FirstPartySetsCacheFilter::MatchInfo())));

  EXPECT_THAT(
      delegate().FindFirstPartySetEntries(
          {site},
          base::BindOnce([](FirstPartySetsManager::EntriesResult) { FAIL(); })),
      Optional(IsEmpty()));
}

}  // namespace
}  // namespace network
