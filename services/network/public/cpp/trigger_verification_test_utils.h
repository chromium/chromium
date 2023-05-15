// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRIGGER_VERIFICATION_TEST_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRIGGER_VERIFICATION_TEST_UTILS_H_

#include <ostream>

namespace network {
class TriggerVerification;

bool operator==(const TriggerVerification&, const TriggerVerification&);

std::ostream& operator<<(std::ostream&, const TriggerVerification&);
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRIGGER_VERIFICATION_TEST_UTILS_H_
