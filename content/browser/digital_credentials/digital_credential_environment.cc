// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/digital_credential_environment.h"

#include "base/no_destructor.h"
#include "content/browser/digital_credentials/virtual_wallet.h"

namespace content {

// static
DigitalCredentialEnvironment* DigitalCredentialEnvironment::GetInstance() {
  static base::NoDestructor<DigitalCredentialEnvironment> environment;
  return environment.get();
}

DigitalCredentialEnvironment::DigitalCredentialEnvironment() = default;
DigitalCredentialEnvironment::~DigitalCredentialEnvironment() = default;

DigitalCredentialEnvironment::ObservedWallet::ObservedWallet(
    FrameTreeNode* node,
    FrameTreeNode::Observer* observer)
    : wallet(std::make_unique<VirtualWallet>()), observation(observer) {
  observation.Observe(node);
}

DigitalCredentialEnvironment::ObservedWallet::~ObservedWallet() = default;

void DigitalCredentialEnvironment::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  virtual_wallets_.clear();
}

VirtualWallet* DigitalCredentialEnvironment::EnableVirtualWalletFor(
    FrameTreeNode* node) {
  auto [it, _] = virtual_wallets_.emplace(
      node, std::make_unique<ObservedWallet>(node, this));
  return it->second->wallet.get();
}

void DigitalCredentialEnvironment::DisableVirtualWalletFor(
    FrameTreeNode* node) {
  virtual_wallets_.erase(node);
}

VirtualWallet* DigitalCredentialEnvironment::MaybeGetVirtualWallet(
    FrameTreeNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (; node; node = FrameTreeNode::From(node->parent())) {
    auto it = virtual_wallets_.find(node);
    if (it != virtual_wallets_.end()) {
      return it->second->wallet.get();
    }
  }
  return nullptr;
}

VirtualWallet* DigitalCredentialEnvironment::GetOrCreateVirtualWallet(
    FrameTreeNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = virtual_wallets_.find(node);
  if (it != virtual_wallets_.end()) {
    return it->second->wallet.get();
  }
  return EnableVirtualWalletFor(node);
}

void DigitalCredentialEnvironment::OnFrameTreeNodeDestroyed(
    FrameTreeNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisableVirtualWalletFor(node);
}

}  // namespace content
