// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_SCOPED_PASSKEY_REAUTH_MODULE_OVERRIDE_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_SCOPED_PASSKEY_REAUTH_MODULE_OVERRIDE_H_

#import <memory>

@protocol ReauthenticationProtocol;

// Util class enabling a global override of the Reauthentication Module used in
// newly-constructed PasskeyCreationBottomSheetCoordinator, for testing
// purposes only.
class ScopedPasskeyReauthModuleOverride {
 public:
  ~ScopedPasskeyReauthModuleOverride();

  // Returns the override module, if one exists.
  static id<ReauthenticationProtocol> Get();

  // Creates a scoped override so that the provided fake/mock/disarmed/etc
  // reauthentication module will be used in place of the production
  // implementation. The caller is responsible for maintaining the lifetime of
  // the `module` since `ScopedPasskeyReauthModuleOverride` only holds a raw
  // pointer.
  static std::unique_ptr<ScopedPasskeyReauthModuleOverride>
  MakeAndArmForTesting(id<ReauthenticationProtocol> module);

  // The module to be used.
  id<ReauthenticationProtocol> module;

 private:
  ScopedPasskeyReauthModuleOverride() = default;
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_SCOPED_PASSKEY_REAUTH_MODULE_OVERRIDE_H_
