// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/touch_id_context.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Security/Security.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/mac/authenticator_config.h"

namespace device {
namespace fido {
namespace mac {

namespace {
API_AVAILABLE(macosx(10.12.2))
base::ScopedCFTypeRef<SecAccessControlRef> DefaultAccessControl() {
  // The default access control policy used for WebAuthn credentials stored by
  // the Touch ID platform authenticator.
  return base::ScopedCFTypeRef<SecAccessControlRef>(
      SecAccessControlCreateWithFlags(
          kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
          kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence,
          nullptr));
}

// Returns whether the current binary is signed with a keychain-access-groups
// entitlement that contains |keychain_access_group|. This is required for the
// TouchIdAuthenticator to access key material stored in the Touch ID secure
// enclave.
bool BinaryHasKeychainAccessGroupEntitlement(
    const std::string& keychain_access_group) {
  base::ScopedCFTypeRef<SecCodeRef> code;
  if (SecCodeCopySelf(kSecCSDefaultFlags, code.InitializeInto()) !=
      errSecSuccess) {
    return false;
  }
  base::ScopedCFTypeRef<CFDictionaryRef> signing_info;
  if (SecCodeCopySigningInformation(code, kSecCSDefaultFlags,
                                    signing_info.InitializeInto()) !=
      errSecSuccess) {
    return false;
  }
  CFDictionaryRef entitlements =
      base::mac::GetValueFromDictionary<CFDictionaryRef>(
          signing_info, kSecCodeInfoEntitlementsDict);
  if (!entitlements) {
    return false;
  }
  CFArrayRef keychain_access_groups =
      base::mac::GetValueFromDictionary<CFArrayRef>(
          entitlements,
          base::ScopedCFTypeRef<CFStringRef>(
              base::SysUTF8ToCFStringRef("keychain-access-groups")));
  if (!keychain_access_groups) {
    return false;
  }
  return CFArrayContainsValue(
      keychain_access_groups,
      CFRangeMake(0, CFArrayGetCount(keychain_access_groups)),
      base::ScopedCFTypeRef<CFStringRef>(
          base::SysUTF8ToCFStringRef(keychain_access_group)));
}
}  // namespace

// static
std::unique_ptr<TouchIdContext> TouchIdContext::CreateImpl() {
  return base::WrapUnique(new TouchIdContext());
}

// static
TouchIdContext::CreateFuncPtr TouchIdContext::g_create_ =
    &TouchIdContext::CreateImpl;

// static
std::unique_ptr<TouchIdContext> TouchIdContext::Create() {
  // Testing seam to allow faking Touch ID in tests.
  return (*g_create_)();
}

// static
bool TouchIdContext::TouchIdAvailableImpl(const AuthenticatorConfig& config) {
  if (!BinaryHasKeychainAccessGroupEntitlement(config.keychain_access_group)) {
    FIDO_LOG(ERROR) << "Touch ID unavailable because keychain-access-group "
                       "entitlement is missing or incorrect";
    return false;
  }

  base::scoped_nsobject<LAContext> context([[LAContext alloc] init]);
  return
      [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                           error:nil];
}

// static
TouchIdContext::TouchIdAvailableFuncPtr TouchIdContext::g_touch_id_available_ =
    &TouchIdContext::TouchIdAvailableImpl;

// static
bool TouchIdContext::TouchIdAvailable(const AuthenticatorConfig& config) {
  // Testing seam to allow faking Touch ID in tests.
  return (*g_touch_id_available_)(config);
}

TouchIdContext::TouchIdContext()
    : context_([[LAContext alloc] init]),
      access_control_(DefaultAccessControl()),
      callback_(),
      weak_ptr_factory_(this) {}

TouchIdContext::~TouchIdContext() {
  // Invalidating the LAContext will dismiss any pending UI dialog (e.g. if the
  // transaction was cancelled while we are waiting for a fingerprint). Simply
  // releasing the LAContext does not have the same effect.
  [context_ invalidate];
}

void TouchIdContext::PromptTouchId(const base::string16& reason,
                                   Callback callback) {
  callback_ = std::move(callback);
  scoped_refptr<base::SequencedTaskRunner> runner =
      base::SequencedTaskRunnerHandle::Get();
  auto weak_self = weak_ptr_factory_.GetWeakPtr();
  // If evaluation succeeds (i.e. user provides a fingerprint), |context_| can
  // be used for one signing operation. N.B. even in |MakeCredentialOperation|,
  // we need to perform a signature for the attestation statement, so we need
  // the sign bit there.
  [context_ evaluateAccessControl:access_control_
                        operation:LAAccessControlOperationUseKeySign
                  localizedReason:base::SysUTF16ToNSString(reason)
                            reply:^(BOOL success, NSError* error) {
                              // The reply block is invoked in a separate
                              // thread. We want to invoke the callback in the
                              // original thread, so we post it onto the
                              // originating runner. The weak_self and runner
                              // variables inside the block are const-copies of
                              // the ones in the enclosing scope (c.f.
                              // http://clang.llvm.org/docs/Block-ABI-Apple.html#imported-variables).
                              if (!success) {
                                // |error| is autoreleased in this block. It
                                // is not currently passed onto the other
                                // thread running the callback; but if it
                                // were, it would have to be retained first
                                // (and probably captured in a
                                // scoped_nsobject<NSError>).
                                DCHECK(error != nil);
                                DVLOG(1) << "Touch ID prompt failed: "
                                         << base::mac::NSToCFCast(error);
                              }
                              runner->PostTask(
                                  FROM_HERE,
                                  base::BindOnce(&TouchIdContext::RunCallback,
                                                 weak_self, success));
                            }];
}

void TouchIdContext::RunCallback(bool success) {
  std::move(callback_).Run(success);
}

}  // namespace mac
}  // namespace fido
}  // namespace device
