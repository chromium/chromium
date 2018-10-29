// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "url/gurl.h"

namespace device {

// Data structure containing a user id, an optional user name, an optional user
// display image url, and an optional user display name as specified by the CTAP
// spec. Used as required parameter type for AuthenticatorMakeCredential
// request.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialUserEntity {
 public:
  static base::Optional<PublicKeyCredentialUserEntity> CreateFromCBORValue(
      const cbor::Value& cbor);

  explicit PublicKeyCredentialUserEntity(std::vector<uint8_t> user_id);
  PublicKeyCredentialUserEntity(const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity(PublicKeyCredentialUserEntity&& other);
  PublicKeyCredentialUserEntity& operator=(
      const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity& operator=(
      PublicKeyCredentialUserEntity&& other);
  ~PublicKeyCredentialUserEntity();

  cbor::Value ConvertToCBOR() const;
  PublicKeyCredentialUserEntity& SetUserName(std::string user_name);
  PublicKeyCredentialUserEntity& SetDisplayName(std::string display_name);
  PublicKeyCredentialUserEntity& SetIconUrl(GURL icon_url);

  const std::vector<uint8_t>& user_id() const { return user_id_; }
  const base::Optional<std::string>& user_name() const { return user_name_; }
  const base::Optional<std::string>& user_display_name() const {
    return user_display_name_;
  }
  const base::Optional<GURL>& user_icon_url() const { return user_icon_url_; }

 private:
  std::vector<uint8_t> user_id_;
  base::Optional<std::string> user_name_;
  base::Optional<std::string> user_display_name_;
  base::Optional<GURL> user_icon_url_;
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_
