// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
#define MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "media/base/cdm_config.h"
#include "media/blink/media_blink_export.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"

namespace blink {
class WebSecurityOrigin;
}

namespace media {

struct CdmConfig;
class CdmContextRef;
class CdmFactory;
class CdmSessionAdapter;

using WebCdmCreatedCB =
    base::OnceCallback<void(blink::WebContentDecryptionModule* cdm,
                            const std::string& error_message)>;

class MEDIA_BLINK_EXPORT WebContentDecryptionModuleImpl
    : public blink::WebContentDecryptionModule {
 public:
  static void Create(CdmFactory* cdm_factory,
                     const base::string16& key_system,
                     const blink::WebSecurityOrigin& security_origin,
                     const CdmConfig& cdm_config,
                     WebCdmCreatedCB web_cdm_created_cb);

  ~WebContentDecryptionModuleImpl() override;

  // blink::WebContentDecryptionModule implementation.
  std::unique_ptr<blink::WebContentDecryptionModuleSession> CreateSession()
      override;
  void SetServerCertificate(
      const uint8_t* server_certificate,
      size_t server_certificate_length,
      blink::WebContentDecryptionModuleResult result) override;
  void GetStatusForPolicy(
      const blink::WebString& min_hdcp_version_string,
      blink::WebContentDecryptionModuleResult result) override;

  std::unique_ptr<CdmContextRef> GetCdmContextRef();

  std::string GetKeySystem() const;

  CdmConfig GetCdmConfig() const;

 private:
  friend CdmSessionAdapter;

  // Takes reference to |adapter|.
  WebContentDecryptionModuleImpl(scoped_refptr<CdmSessionAdapter> adapter);

  scoped_refptr<CdmSessionAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleImpl);
};

// Allow typecasting from blink type as this is the only implementation.
inline WebContentDecryptionModuleImpl* ToWebContentDecryptionModuleImpl(
    blink::WebContentDecryptionModule* cdm) {
  return static_cast<WebContentDecryptionModuleImpl*>(cdm);
}

}  // namespace media

#endif  // MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
