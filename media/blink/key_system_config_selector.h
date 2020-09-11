// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_KEY_SYSTEM_CONFIG_SELECTOR_H_
#define MEDIA_BLINK_KEY_SYSTEM_CONFIG_SELECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/eme_constants.h"
#include "media/blink/media_blink_export.h"
#include "third_party/blink/public/platform/web_media_key_system_media_capability.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

struct WebMediaKeySystemConfiguration;
class WebString;

}  // namespace blink

namespace media {

struct CdmConfig;
class KeySystems;
class MediaPermission;

class MEDIA_BLINK_EXPORT KeySystemConfigSelector {
 public:
  KeySystemConfigSelector(KeySystems* key_systems,
                          MediaPermission* media_permission);

  ~KeySystemConfigSelector();

  // The unsupported statuses will be mapped to different rejection messages.
  // The statuses should not leak sensitive information, e.g. incognito mode or
  // user settings. See https://crbug.com/760720
  enum class Status {
    kSupported,
    kUnsupportedPlatform,
    kUnsupportedKeySystem,
    kUnsupportedConfigs,
  };

  // Callback for the result of `SelectConfig()`. The returned configs must be
  // non-null iff `status` is `kSupported`.
  using SelectConfigCB = base::OnceCallback<
      void(Status status, blink::WebMediaKeySystemConfiguration*, CdmConfig*)>;

  void SelectConfig(
      const blink::WebString& key_system,
      const blink::WebVector<blink::WebMediaKeySystemConfiguration>&
          candidate_configurations,
      SelectConfigCB cb);

  using IsSupportedMediaTypeCB =
      base::RepeatingCallback<bool(const std::string& container_mime_type,
                                   const std::string& codecs,
                                   bool use_aes_decryptor)>;

  void SetIsSupportedMediaTypeCBForTesting(IsSupportedMediaTypeCB cb) {
    is_supported_media_type_cb_ = std::move(cb);
  }

 private:
  struct SelectionRequest;
  class ConfigState;

  enum ConfigurationSupport {
    CONFIGURATION_NOT_SUPPORTED,
    CONFIGURATION_REQUIRES_PERMISSION,
    CONFIGURATION_SUPPORTED,
  };

  void SelectConfigInternal(std::unique_ptr<SelectionRequest> request);

  void OnPermissionResult(std::unique_ptr<SelectionRequest> request,
                          bool is_permission_granted);

  ConfigurationSupport GetSupportedConfiguration(
      const std::string& key_system,
      const blink::WebMediaKeySystemConfiguration& candidate,
      ConfigState* config_state,
      blink::WebMediaKeySystemConfiguration* accumulated_configuration);

  bool GetSupportedCapabilities(
      const std::string& key_system,
      EmeMediaType media_type,
      const blink::WebVector<blink::WebMediaKeySystemMediaCapability>&
          requested_media_capabilities,
      ConfigState* config_state,
      std::vector<blink::WebMediaKeySystemMediaCapability>*
          supported_media_capabilities);

  bool IsSupportedContentType(const std::string& key_system,
                              EmeMediaType media_type,
                              const std::string& container_mime_type,
                              const std::string& codecs,
                              ConfigState* config_state);

  EmeConfigRule GetEncryptionSchemeConfigRule(
      const std::string& key_system,
      const blink::WebMediaKeySystemMediaCapability::EncryptionScheme
          encryption_scheme);

  KeySystems* const key_systems_;
  MediaPermission* media_permission_;

  // A callback used to check whether a media type is supported. Only set in
  // tests. If null the implementation will check the support using MimeUtil.
  IsSupportedMediaTypeCB is_supported_media_type_cb_;

  base::WeakPtrFactory<KeySystemConfigSelector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(KeySystemConfigSelector);
};

}  // namespace media

#endif  // MEDIA_BLINK_KEY_SYSTEM_CONFIG_SELECTOR_H_
