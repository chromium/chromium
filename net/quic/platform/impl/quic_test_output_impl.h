// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TEST_OUTPUT_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TEST_OUTPUT_IMPL_H_

#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace quic {

void QuicSaveTestOutputImpl(absl::string_view filename, absl::string_view data);

bool QuicLoadTestOutputImpl(absl::string_view filename, std::string* data);

void QuicRecordTraceImpl(absl::string_view identifier, absl::string_view data);

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TEST_OUTPUT_IMPL_H_
