// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(KeyPinningStateDummy, Test) {
  constexpr const char* kHost = "chrome.google.com";

  auto state = base::MakeRefCounted<KeyPinningStateDummy>();

  EXPECT_FALSE(state->HasPublicKeyPins(kHost));
  KeyPinningState::PKPState pkp_state;
  EXPECT_FALSE(state->GetPKPState(kHost, &pkp_state));

  EXPECT_EQ(KeyPinningState::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost,
                                      /*is_issued_by_known_root=*/true, {}));
  SHA256HashValue hash;
  hash.fill(1);
  EXPECT_EQ(
      KeyPinningState::PKPStatus::OK,
      state->CheckPublicKeyPins(kHost,
                                /*is_issued_by_known_root=*/true, {hash}));
}

}  // namespace
}  // namespace net
