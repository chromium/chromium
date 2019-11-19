// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
#define MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/blink/key_system_config_selector.h"
#include "media/blink/media_blink_export.h"
#include "third_party/blink/public/platform/web_encrypted_media_client.h"

namespace blink {

class WebContentDecryptionModuleResult;
struct WebMediaKeySystemConfiguration;
class WebSecurityOrigin;

}  // namespace blink

namespace media {

struct CdmConfig;
class CdmFactory;
class MediaPermission;

class MEDIA_BLINK_EXPORT WebEncryptedMediaClientImpl
    : public blink::WebEncryptedMediaClient {
 public:
  WebEncryptedMediaClientImpl(CdmFactory* cdm_factory,
                              MediaPermission* media_permission);
  ~WebEncryptedMediaClientImpl() override;

  // WebEncryptedMediaClient implementation.
  void RequestMediaKeySystemAccess(
      blink::WebEncryptedMediaRequest request) override;

  // Create the CDM for |key_system| and |security_origin|. The caller owns
  // the created cdm (passed back using |result|).
  void CreateCdm(
      const blink::WebString& key_system,
      const blink::WebSecurityOrigin& security_origin,
      const CdmConfig& cdm_config,
      std::unique_ptr<blink::WebContentDecryptionModuleResult> result);

 private:
  // Report usage of key system to UMA. There are 2 different counts logged:
  // 1. The key system is requested.
  // 2. The requested key system and options are supported.
  // Each stat is only reported once per renderer frame per key system.
  class Reporter;

  // Complete a requestMediaKeySystemAccess() request with a supported
  // accumulated configuration.
  void OnRequestSucceeded(
      blink::WebEncryptedMediaRequest request,
      const blink::WebMediaKeySystemConfiguration& accumulated_configuration,
      const CdmConfig& cdm_config);

  // Complete a requestMediaKeySystemAccess() request with a NotSupportedError.
  void OnRequestNotSupported(blink::WebEncryptedMediaRequest request);

  // Gets the Reporter for |key_system|. If it doesn't already exist,
  // create one.
  Reporter* GetReporter(const blink::WebString& key_system);

  // Reporter singletons.
  std::unordered_map<std::string, std::unique_ptr<Reporter>> reporters_;

  CdmFactory* cdm_factory_;
  KeySystemConfigSelector key_system_config_selector_;
  base::WeakPtrFactory<WebEncryptedMediaClientImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
