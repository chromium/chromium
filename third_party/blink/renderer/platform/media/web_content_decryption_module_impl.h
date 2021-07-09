// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_CONTENT_DECRYPTION_MODULE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_CONTENT_DECRYPTION_MODULE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/cdm_config.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace media {
class CdmContextRef;
class CdmFactory;
struct CdmConfig;
}  // namespace media

namespace blink {
class CdmSessionAdapter;
class WebSecurityOrigin;

using WebCdmCreatedCB =
    base::OnceCallback<void(blink::WebContentDecryptionModule* cdm,
                            const std::string& error_message)>;

class PLATFORM_EXPORT WebContentDecryptionModuleImpl
    : public blink::WebContentDecryptionModule {
 public:
  static void Create(media::CdmFactory* cdm_factory,
                     const std::u16string& key_system,
                     const blink::WebSecurityOrigin& security_origin,
                     const media::CdmConfig& cdm_config,
                     WebCdmCreatedCB web_cdm_created_cb);

  WebContentDecryptionModuleImpl(const WebContentDecryptionModuleImpl&) =
      delete;
  WebContentDecryptionModuleImpl& operator=(
      const WebContentDecryptionModuleImpl&) = delete;
  ~WebContentDecryptionModuleImpl() override;

  // blink::WebContentDecryptionModule implementation.
  std::unique_ptr<blink::WebContentDecryptionModuleSession> CreateSession(
      blink::WebEncryptedMediaSessionType session_type) override;
  void SetServerCertificate(
      const uint8_t* server_certificate,
      size_t server_certificate_length,
      blink::WebContentDecryptionModuleResult result) override;
  void GetStatusForPolicy(
      const blink::WebString& min_hdcp_version_string,
      blink::WebContentDecryptionModuleResult result) override;

  std::unique_ptr<media::CdmContextRef> GetCdmContextRef();

  std::string GetKeySystem() const;

  media::CdmConfig GetCdmConfig() const;

 private:
  friend CdmSessionAdapter;

  // Takes reference to |adapter|.
  WebContentDecryptionModuleImpl(scoped_refptr<CdmSessionAdapter> adapter);

  scoped_refptr<CdmSessionAdapter> adapter_;
};

// Allow typecasting from blink type as this is the only implementation.
PLATFORM_EXPORT
inline WebContentDecryptionModuleImpl* ToWebContentDecryptionModuleImpl(
    blink::WebContentDecryptionModule* cdm) {
  return static_cast<WebContentDecryptionModuleImpl*>(cdm);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_CONTENT_DECRYPTION_MODULE_IMPL_H_
