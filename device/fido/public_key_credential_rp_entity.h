// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "url/gurl.h"

namespace device {

// Data structure containing information about relying party that invoked
// WebAuth API. Includes a relying party id, an optional relying party name,,
// and optional relying party display image url.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialRpEntity {
 public:
  static base::Optional<PublicKeyCredentialRpEntity> CreateFromCBORValue(
      const cbor::Value& cbor);

  explicit PublicKeyCredentialRpEntity(std::string rp_id);
  PublicKeyCredentialRpEntity(const PublicKeyCredentialRpEntity& other);
  PublicKeyCredentialRpEntity(PublicKeyCredentialRpEntity&& other);
  PublicKeyCredentialRpEntity& operator=(
      const PublicKeyCredentialRpEntity& other);
  PublicKeyCredentialRpEntity& operator=(PublicKeyCredentialRpEntity&& other);
  ~PublicKeyCredentialRpEntity();

  cbor::Value ConvertToCBOR() const;

  PublicKeyCredentialRpEntity& SetRpName(std::string rp_name);
  PublicKeyCredentialRpEntity& SetRpIconUrl(GURL icon_url);

  const std::string& rp_id() const { return rp_id_; }
  const base::Optional<std::string>& rp_name() const { return rp_name_; }
  const base::Optional<GURL>& rp_icon_url() const { return rp_icon_url_; }

 private:
  std::string rp_id_;
  base::Optional<std::string> rp_name_;
  base::Optional<GURL> rp_icon_url_;
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_
