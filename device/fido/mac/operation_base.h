// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_OPERATION_BASE_H_
#define DEVICE_FIDO_MAC_OPERATION_BASE_H_

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/operation.h"
#include "device/fido/mac/touch_id_context.h"

namespace device {
namespace fido {
namespace mac {

// OperationBase abstracts behavior common to both concrete Operations,
// |MakeCredentialOperation| and |GetAssertionOperation|.
template <class Request, class Response>
class API_AVAILABLE(macosx(10.12.2)) OperationBase : public Operation {
 public:
  using Callback = base::OnceCallback<void(CtapDeviceResponseCode,
                                           base::Optional<Response>)>;

  OperationBase(Request request,
                std::string metadata_secret,
                std::string keychain_access_group,
                Callback callback)
      : request_(std::move(request)),
        metadata_secret_(std::move(metadata_secret)),
        keychain_access_group_(std::move(keychain_access_group)),
        callback_(std::move(callback)),
        touch_id_context_(TouchIdContext::Create()) {}

  ~OperationBase() override = default;

 protected:
  // Subclasses must call Init() at the beginning of Run().
  bool Init() {
    base::Optional<std::string> encoded_rp_id =
        EncodeRpId(metadata_secret(), RpId());
    if (!encoded_rp_id)
      return false;

    encoded_rp_id_ = std::move(*encoded_rp_id);
    return true;
  }

  // PromptTouchId triggers a Touch ID consent dialog with the given reason
  // string. Subclasses implement the PromptTouchIdDone callback to receive the
  // result.
  void PromptTouchId(const base::string16& reason) {
    // The callback passed to TouchIdContext::Prompt will not fire if the
    // TouchIdContext itself has been deleted. Since that it is owned by this
    // class, there is no need to bind the callback to a weak ref here.
    touch_id_context_->PromptTouchId(
        reason, base::BindOnce(&OperationBase::PromptTouchIdDone,
                               base::Unretained(this)));
  }

  // Callback for |PromptTouchId|.
  virtual void PromptTouchIdDone(bool success) = 0;

  // Subclasses override RpId to return the RP ID from the type-specific
  // request.
  virtual const std::string& RpId() const = 0;

  LAContext* authentication_context() const {
    return touch_id_context_->authentication_context();
  }
  SecAccessControlRef access_control() const {
    return touch_id_context_->access_control();
  }

  // DefaultKeychainQuery returns a default keychain query dictionary that has
  // the keychain item class, keychain access group and RP ID filled out (but
  // not the credential ID). More fields can be set on the return value to
  // refine the query.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> DefaultKeychainQuery() const {
    base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
    CFDictionarySetValue(query, kSecClass, kSecClassKey);
    CFDictionarySetValue(query, kSecAttrAccessGroup,
                         base::SysUTF8ToNSString(keychain_access_group_));
    DCHECK(!encoded_rp_id_.empty());
    CFDictionarySetValue(query, kSecAttrLabel,
                         base::SysUTF8ToNSString(encoded_rp_id_));
    return query;
  }

  const std::string& metadata_secret() const { return metadata_secret_; }
  const std::string& keychain_access_group() const {
    return keychain_access_group_;
  }
  const Request& request() const { return request_; }
  Callback& callback() { return callback_; }

 private:
  Request request_;
  // The secret parameter passed to |CredentialMetadata| operations to encrypt
  // or encode credential metadata for storage in the macOS keychain.
  std::string metadata_secret_;
  std::string keychain_access_group_;
  std::string encoded_rp_id_ = "";
  Callback callback_;

  std::unique_ptr<TouchIdContext> touch_id_context_;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_OPERATION_BASE_H_
