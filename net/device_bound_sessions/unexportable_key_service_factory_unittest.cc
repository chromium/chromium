// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/unexportable_key_service_factory.h"

#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

TEST(UnexportableKeyServiceFactoryTest, CreateAndDestroy) {
  UnexportableKeyServiceFactory* instance =
      UnexportableKeyServiceFactory::GetInstanceForTesting();
  ASSERT_TRUE(instance);
  delete instance;
}

}  // namespace

}  // namespace net::device_bound_sessions
