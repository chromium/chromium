// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/integrity_policy.h"

namespace network {

IntegrityPolicy::IntegrityPolicy() = default;
IntegrityPolicy::~IntegrityPolicy() = default;

IntegrityPolicy::IntegrityPolicy(IntegrityPolicy&&) = default;
IntegrityPolicy& IntegrityPolicy::operator=(IntegrityPolicy&&) = default;

IntegrityPolicy::IntegrityPolicy(const IntegrityPolicy&) = default;
IntegrityPolicy& IntegrityPolicy::operator=(const IntegrityPolicy&) = default;
bool IntegrityPolicy::operator==(const IntegrityPolicy&) const = default;

}  // namespace network
