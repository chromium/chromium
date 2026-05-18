// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIGITAL_CREDENTIALS_VIRTUAL_WALLET_H_
#define CONTENT_BROWSER_DIGITAL_CREDENTIALS_VIRTUAL_WALLET_H_

#include <optional>

#include "content/common/content_export.h"
#include "content/public/browser/digital_identity_provider.h"

namespace content {

class CONTENT_EXPORT VirtualWallet {
 public:
  // Controls the simulated wallet behavior for WebDriver flows.
  enum class Behavior {
    kRespond,
    kWait,
    kDecline,
  };

  VirtualWallet();
  ~VirtualWallet();

  VirtualWallet(const VirtualWallet&) = delete;
  VirtualWallet& operator=(const VirtualWallet&) = delete;

  // Stores a credential to be returned by the next call to
  // `GetCredential()`.
  void SetCredential(DigitalIdentityProvider::DigitalCredential credential);

  // Returns a copy of the currently stored credential (if any).
  std::optional<DigitalIdentityProvider::DigitalCredential> GetCredential()
      const;

  // Clears the stored credential and behavior.
  void Clear();

  // Sets the wallet behavior.
  void set_behavior(Behavior behavior) { behavior_ = behavior; }

  // Returns the current wallet behavior (if any).
  std::optional<Behavior> behavior() const { return behavior_; }

 private:
  std::optional<DigitalIdentityProvider::DigitalCredential> stored_credential_;
  std::optional<Behavior> behavior_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIGITAL_CREDENTIALS_VIRTUAL_WALLET_H_
