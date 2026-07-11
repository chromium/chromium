// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/test_static_ech_mode_getter.h"

#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TestStaticEchModeGetter::TestStaticEchModeGetter(EchMode ech_mode,
                                                 std::string_view expected_host)
    : ech_mode_(ech_mode), expected_host_(expected_host) {}

TestStaticEchModeGetter::TestStaticEchModeGetter(EchMode ech_mode,
                                                 const char* expected_host)
    : TestStaticEchModeGetter(ech_mode, std::string_view(expected_host)) {}

TestStaticEchModeGetter::TestStaticEchModeGetter(EchMode ech_mode,
                                                 std::string&& expected_host)
    : ech_mode_(ech_mode), expected_host_(std::move(expected_host)) {}

TestStaticEchModeGetter::~TestStaticEchModeGetter() = default;

EchMode TestStaticEchModeGetter::GetEchMode(std::string_view hostname) const {
  EXPECT_EQ(hostname, expected_host_);
  return ech_mode_;
}

}  // namespace net
