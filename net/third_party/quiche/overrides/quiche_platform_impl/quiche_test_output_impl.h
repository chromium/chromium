// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TEST_OUTPUT_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TEST_OUTPUT_IMPL_H_

#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace quiche {

void QuicheSaveTestOutputImpl(absl::string_view filename,
                              absl::string_view data);

bool QuicheLoadTestOutputImpl(absl::string_view filename, std::string* data);

void QuicheRecordTraceImpl(absl::string_view identifier,
                           absl::string_view data);

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TEST_OUTPUT_IMPL_H_
