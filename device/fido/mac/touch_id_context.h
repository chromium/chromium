// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_TOUCH_ID_CONTEXT_H_
#define DEVICE_FIDO_MAC_TOUCH_ID_CONTEXT_H_

#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"

namespace device {
namespace fido {
namespace mac {

struct AuthenticatorConfig;

// TouchIdContext wraps a macOS Touch ID consent prompt for signing with a
// secure enclave key. It is a essentially a simpler facade for the LAContext
// class from the macOS LocalAuthentication framework (c.f.
// https://developer.apple.com/documentation/localauthentication/lacontext?language=objc).
//
// Use |Create| to instantiate a new context. Multiple instances can be created
// at the same time. However, calling |PromptTouchId| on one instance will
// cancel any other pending evaluations with an error. Deleting an instance
// will invalidate any pending evaluation prompts (i.e. the dialog will
// disappear and evaluation will fail with an error).
class COMPONENT_EXPORT(DEVICE_FIDO)
    API_AVAILABLE(macosx(10.12.2)) TouchIdContext {
 public:
  // The callback is invoked when the Touch ID prompt completes. It receives a
  // boolean indicating whether obtaining the fingerprint was successful.
  using Callback = base::OnceCallback<void(bool)>;

  // Factory method for instantiating a TouchIdContext.
  static std::unique_ptr<TouchIdContext> Create();

  // Returns whether Touch ID is available and enrolled on the
  // current device and the current binary carries a
  // keychain-access-groups entitlement that matches the one set
  // in |config|.
  static bool TouchIdAvailable(const AuthenticatorConfig& config);

  virtual ~TouchIdContext();

  // PromptTouchId displays a Touch ID consent prompt with the provided reason
  // string to the user. On completion or error, the provided callback is
  // invoked, unless the TouchIdContext instance has been destroyed in the
  // meantime (in which case nothing happens).
  virtual void PromptTouchId(const base::string16& reason, Callback callback);

  // authentication_context returns the LAContext used for the Touch ID prompt.
  LAContext* authentication_context() const { return context_; }

  // access_control returns a reference to the SecAccessControl object that was
  // evaluated/authorized in the Touch ID prompt.
  SecAccessControlRef access_control() const { return access_control_; }

 protected:
  TouchIdContext();

 private:
  using CreateFuncPtr = decltype(&Create);
  using TouchIdAvailableFuncPtr = decltype(&TouchIdAvailable);

  static CreateFuncPtr g_create_;
  static TouchIdAvailableFuncPtr g_touch_id_available_;

  static std::unique_ptr<TouchIdContext> CreateImpl();
  static bool TouchIdAvailableImpl(const AuthenticatorConfig& config);

  void RunCallback(bool success);

  base::scoped_nsobject<LAContext> context_;
  base::ScopedCFTypeRef<SecAccessControlRef> access_control_;
  Callback callback_;
  base::WeakPtrFactory<TouchIdContext> weak_ptr_factory_;

  friend class ScopedTouchIdTestEnvironment;
  DISALLOW_COPY_AND_ASSIGN(TouchIdContext);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_TOUCH_ID_CONTEXT_H_
