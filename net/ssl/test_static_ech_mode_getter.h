// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_TEST_STATIC_ECH_MODE_GETTER_H_
#define NET_SSL_TEST_STATIC_ECH_MODE_GETTER_H_

#include <string>
#include <string_view>

#include "net/base/ech_mode.h"
#include "net/ssl/ech_mode_getter.h"

namespace net {

// A completely immutable, test-only implementation of EchModeGetter that
// returns a static EchMode configured at construction.
class TestStaticEchModeGetter final : public EchModeGetter {
 public:
  TestStaticEchModeGetter(EchMode ech_mode, std::string_view expected_host);
  TestStaticEchModeGetter(EchMode ech_mode, const char* expected_host);
  TestStaticEchModeGetter(EchMode ech_mode, std::string&& expected_host);
  ~TestStaticEchModeGetter() override;

  EchMode GetEchMode(std::string_view hostname) const override;

 private:
  const EchMode ech_mode_;
  const std::string expected_host_;
};

}  // namespace net

#endif  // NET_SSL_TEST_STATIC_ECH_MODE_GETTER_H_
