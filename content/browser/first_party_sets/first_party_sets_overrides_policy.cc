// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_overrides_policy.h"

#include "net/first_party_sets/sets_mutation.h"

namespace content {

FirstPartySetsOverridesPolicy::FirstPartySetsOverridesPolicy(
    net::SetsMutation mutation)
    : mutation_(std::move(mutation)) {}

FirstPartySetsOverridesPolicy::FirstPartySetsOverridesPolicy(
    FirstPartySetsOverridesPolicy&&) = default;
FirstPartySetsOverridesPolicy& FirstPartySetsOverridesPolicy::operator=(
    FirstPartySetsOverridesPolicy&&) = default;

FirstPartySetsOverridesPolicy::~FirstPartySetsOverridesPolicy() = default;

bool FirstPartySetsOverridesPolicy::operator==(
    const FirstPartySetsOverridesPolicy& other) const = default;

}  // namespace content
