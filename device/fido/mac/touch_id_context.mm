// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/touch_id_context.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/apple_keychain_util.h"
#include "crypto/apple_keychain_v2.h"
#include "device/fido/mac/authenticator_config.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace device::fido::mac {

namespace {

// Returns whether creating a key pair in the secure enclave succeeds. Keys are
// not persisted to the keychain.
bool CanCreateSecureEnclaveKeyPairBlocking() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSDictionary* params = @{
    CFToNSPtrCast(kSecAttrKeyType) :
        CFToNSPtrCast(kSecAttrKeyTypeECSECPrimeRandom),
    CFToNSPtrCast(kSecAttrKeySizeInBits) : @256,
    CFToNSPtrCast(kSecAttrTokenID) :
        CFToNSPtrCast(kSecAttrTokenIDSecureEnclave),
    CFToNSPtrCast(kSecAttrIsPermanent) : @NO,
  };

  base::apple::ScopedCFTypeRef<CFErrorRef> cferr;
  base::apple::ScopedCFTypeRef<SecKeyRef> private_key(
      crypto::AppleKeychainV2::GetInstance().KeyCreateRandomKey(
          NSToCFPtrCast(params), cferr.InitializeInto()));
  return !!private_key;
}

base::apple::ScopedCFTypeRef<SecAccessControlRef> CreateDefaultAccessControl() {
  return base::apple::ScopedCFTypeRef<SecAccessControlRef>(
      SecAccessControlCreateWithFlags(
          kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
          kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence,
          nullptr));
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
bool TouchIdContext::TouchIdAvailableImpl(AuthenticatorConfig config) {
  // Ensure that the main executable is signed with the keychain-access-group
  // entitlement that is configured by the embedder; that user authentication
  // with biometry, watch, or device passcode possible; and that the device has
  // a secure enclave.
  if (!crypto::ExecutableHasKeychainAccessGroupEntitlement(
          config.keychain_access_group)) {
    FIDO_LOG(ERROR)
        << "Touch ID authenticator unavailable because keychain-access-group "
           "entitlement is missing or incorrect. Expected value: "
        << config.keychain_access_group;
    return false;
  }

  LAContext* context = [[LAContext alloc] init];
  NSError* nserr;
  if (![context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication
                            error:&nserr]) {
    FIDO_LOG(DEBUG) << "canEvaluatePolicy failed: " << nserr;
    return false;
  }

  // CryptoKit offers a SecureEnclave.isAvailable property, but no ObjectiveC
  // bindings exist. Instead, test whether we can create a key pair in the
  // secure enclave. This takes hundreds of milliseconds, so only do it once.
  static const bool kHasSecureEnclave = CanCreateSecureEnclaveKeyPairBlocking();
  return kHasSecureEnclave;
}

// Testing seam to allow faking Touch ID in tests.
TouchIdContext::TouchIdAvailableFuncPtr TouchIdContext::g_touch_id_available_ =
    &TouchIdContext::TouchIdAvailableImpl;

// static
void TouchIdContext::TouchIdAvailable(
    AuthenticatorConfig config,
    base::OnceCallback<void(bool is_available)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(g_touch_id_available_, std::move(config)),
      std::move(callback));
}

TouchIdContext::TouchIdContext() : context_([[LAContext alloc] init]) {}

TouchIdContext::~TouchIdContext() {
  // Invalidating the LAContext will dismiss any pending UI dialog (e.g. if the
  // transaction was cancelled while we are waiting for a fingerprint). Simply
  // releasing the LAContext does not have the same effect.
  [context_ invalidate];
}

void TouchIdContext::PromptTouchId(const std::u16string& reason,
                                   Callback callback) {
  callback_ = std::move(callback);
  scoped_refptr<base::SequencedTaskRunner> runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_self = weak_ptr_factory_.GetWeakPtr();
  // Generate a SecAccessControl that can be used for obtaining signatures.
  // For current credentials we can actually obtain signatures without the
  // SecAccessControl, but for older credentials we used kSecAttrAccessControl
  // attribute to ensure the keychain would only produce signatures in exchange
  // for biometrics or device password.
  base::apple::ScopedCFTypeRef<SecAccessControlRef> access_control =
      CreateDefaultAccessControl();
  [context_ evaluateAccessControl:access_control.get()
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
                                // were, it would have to be retained first.
                                DCHECK(error != nil);
                                DVLOG(1) << "Touch ID prompt failed: "
                                         << base::apple::NSToCFPtrCast(error);
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

}  // namespace device::fido::mac
