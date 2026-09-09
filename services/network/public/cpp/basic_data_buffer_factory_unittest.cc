// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/basic_data_buffer_factory.h"

#include "services/network/public/cpp/data_buffer_factory_test_util.h"

namespace network {

INSTANTIATE_TYPED_TEST_SUITE_P(BasicDataBufferFactory,
                               DataBufferFactoryTest,
                               BasicDataBufferFactory);

}  // namespace network
