// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/digital_identity_provider.h"

namespace content {
DigitalIdentityProvider::DigitalCredential::DigitalCredential(
    std::optional<std::string> protocol,
    std::optional<base::Value> data)
    : protocol(std::move(protocol)), data(std::move(data)) {}
DigitalIdentityProvider::DigitalCredential::DigitalCredential(
    DigitalCredential&& other) = default;
DigitalIdentityProvider::DigitalCredential&
DigitalIdentityProvider::DigitalCredential::operator=(
    DigitalCredential&& other) = default;
DigitalIdentityProvider::DigitalCredential::~DigitalCredential() = default;

DigitalIdentityProvider::DigitalIdentityProvider() = default;
DigitalIdentityProvider::~DigitalIdentityProvider() = default;

}  // namespace content
