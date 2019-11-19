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

  PublicKeyCredentialUserEntity();
  explicit PublicKeyCredentialUserEntity(std::vector<uint8_t> id);
  PublicKeyCredentialUserEntity(std::vector<uint8_t> id,
                                base::Optional<std::string> name,
                                base::Optional<std::string> display_name,
                                base::Optional<GURL> icon_url);
  PublicKeyCredentialUserEntity(const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity(PublicKeyCredentialUserEntity&& other);
  PublicKeyCredentialUserEntity& operator=(
      const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity& operator=(
      PublicKeyCredentialUserEntity&& other);
  bool operator==(const PublicKeyCredentialUserEntity& other) const;
  ~PublicKeyCredentialUserEntity();

  std::vector<uint8_t> id;
  base::Optional<std::string> name;
  base::Optional<std::string> display_name;
  base::Optional<GURL> icon_url;
};

cbor::Value AsCBOR(const PublicKeyCredentialUserEntity&);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_
