// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_TOUCH_ID_CONTEXT_H_
#define DEVICE_FIDO_MAC_TOUCH_ID_CONTEXT_H_

#import <LocalAuthentication/LocalAuthentication.h>
#include <Security/Security.h>

#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/mac/credential_store.h"

namespace device::fido::mac {

struct AuthenticatorConfig;

// TouchIdContext wraps a macOS user authentication prompt for signing with a
// secure enclave key. It is a essentially a simpler facade for the LAContext
// class from the macOS LocalAuthentication framework (c.f.
// https://developer.apple.com/documentation/localauthentication/lacontext?language=objc).
//
// Use |Create| to instantiate a new context. Multiple instances can be created
// at the same time. However, calling |PromptTouchId| on one instance will
// cancel any other pending evaluations with an error. Deleting an instance
// will invalidate any pending evaluation prompts (i.e. the dialog will
// disappear and evaluation will fail with an error).
class COMPONENT_EXPORT(DEVICE_FIDO) TouchIdContext {
 public:
  // The callback is invoked when the local user authentication prompt
  // completes. It receives a boolean indicating whether obtaining the
  // fingerprint was successful.
  using Callback = base::OnceCallback<void(bool)>;

  // Factory method for instantiating a TouchIdContext.
  static std::unique_ptr<TouchIdContext> Create();

  // Returns whether the device has a secure enclave and can authenticate the
  // local user, and whether the main executable carries a
  // keychain-access-groups entitlement that matches the one set in |config|.
  static void TouchIdAvailable(AuthenticatorConfig config,
                               base::OnceCallback<void(bool is_available)>);

  TouchIdContext(const TouchIdContext&) = delete;
  TouchIdContext& operator=(const TouchIdContext&) = delete;

  virtual ~TouchIdContext();

  // PromptTouchId displays a local user authentication prompt with the provided
  // reason string to the user. On completion or error, the provided callback is
  // invoked, unless the TouchIdContext instance has been destroyed in the
  // meantime (in which case nothing happens).
  virtual void PromptTouchId(const std::u16string& reason, Callback callback);

  // authentication_context returns the LAContext used for the local user
  // authentication prompt.
  LAContext* authentication_context() const { return context_; }

 protected:
  TouchIdContext();

 private:
  using CreateFuncPtr = decltype(&Create);
  static CreateFuncPtr g_create_;

  static bool TouchIdAvailableImpl(AuthenticatorConfig config);
  using TouchIdAvailableFuncPtr = decltype(&TouchIdAvailableImpl);
  static TouchIdAvailableFuncPtr g_touch_id_available_;

  static std::unique_ptr<TouchIdContext> CreateImpl();

  void RunCallback(bool success);

  LAContext* __strong context_;
  Callback callback_;
  base::WeakPtrFactory<TouchIdContext> weak_ptr_factory_{this};

  friend class ScopedTouchIdTestEnvironment;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_TOUCH_ID_CONTEXT_H_
