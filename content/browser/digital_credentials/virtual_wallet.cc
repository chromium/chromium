// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/virtual_wallet.h"

#include <utility>

namespace content {

VirtualWallet::VirtualWallet() = default;
VirtualWallet::~VirtualWallet() = default;

void VirtualWallet::SetCredential(
    DigitalIdentityProvider::DigitalCredential credential) {
  stored_credential_ = std::move(credential);
}

std::optional<DigitalIdentityProvider::DigitalCredential>
VirtualWallet::GetCredential() {
  if (!stored_credential_.has_value()) {
    return std::nullopt;
  }
  return stored_credential_->Clone();
}

void VirtualWallet::Clear() {
  stored_credential_.reset();
  mode_.reset();
}

}  // namespace content
