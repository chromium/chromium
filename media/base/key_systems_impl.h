// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_KEY_SYSTEMS_IMPL_H_
#define MEDIA_BASE_KEY_SYSTEMS_IMPL_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "media/base/decrypt_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_info.h"
#include "media/base/key_systems.h"
#include "media/base/key_systems_support_registration.h"
#include "media/base/media_export.h"

namespace media {

// TODO(b/321307544): Rename this callback to more appropriate name e.g.
// GetSupportedKeySystemsCB.
using RegisterKeySystemsSupportCB =
    base::OnceCallback<std::unique_ptr<KeySystemSupportRegistration>(
        GetSupportedKeySystemsCB)>;

// An implementation of KeySystems that provides functionality to query
// registered key systems.
class MEDIA_EXPORT KeySystemsImpl : public KeySystems {
 public:
  explicit KeySystemsImpl(RegisterKeySystemsSupportCB cb);
  ~KeySystemsImpl() override;

  KeySystemsImpl(const KeySystemsImpl&) = delete;
  KeySystemsImpl& operator=(const KeySystemsImpl&) = delete;

  // Implementation of KeySystems interface.
  void UpdateIfNeeded(base::OnceClosure done_cb) override;
  std::string GetBaseKeySystemName(
      const std::string& key_system) const override;
  bool IsSupportedKeySystem(const std::string& key_system) const override;
  bool ShouldUseBaseKeySystemName(const std::string& key_system) const override;
  bool CanUseAesDecryptor(const std::string& key_system) const override;
  bool IsSupportedInitDataType(const std::string& key_system,
                               EmeInitDataType init_data_type) const override;
  EmeConfig::Rule GetEncryptionSchemeConfigRule(
      const std::string& key_system,
      EncryptionScheme encryption_scheme) const override;
  EmeConfig::Rule GetContentTypeConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& container_mime_type,
      const std::vector<std::string>& codecs) const override;
  EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* hw_secure_requirement) const override;
  EmeConfig::Rule GetPersistentLicenseSessionSupport(
      const std::string& key_system) const override;
  EmeFeatureSupport GetPersistentStateSupport(
      const std::string& key_system) const override;
  EmeFeatureSupport GetDistinctiveIdentifierSupport(
      const std::string& key_system) const override;

  // These functions are for testing purpose only.
  void AddCodecMaskForTesting(EmeMediaType media_type,
                              const std::string& codec,
                              uint32_t mask);
  void AddMimeTypeCodecMaskForTesting(const std::string& mime_type,
                                      uint32_t mask);
  void ResetForTesting();

 private:
  using MimeTypeToCodecsMap = std::unordered_map<std::string, SupportedCodecs>;
  using CodecMap = std::unordered_map<std::string, EmeCodec>;
  using InitDataTypesMap = std::unordered_map<std::string, EmeInitDataType>;

  void Initialize();

  void UpdateSupportedKeySystems();
  void OnSupportedKeySystemsUpdated(KeySystemInfos key_systems);
  void ProcessSupportedKeySystems(KeySystemInfos key_systems);

  const KeySystemInfo* GetKeySystemInfo(const std::string& key_system) const;

  void RegisterMimeType(const std::string& mime_type, SupportedCodecs codecs);
  bool IsValidMimeTypeCodecsCombination(const std::string& mime_type,
                                        SupportedCodecs codecs) const;

  // TODO(crbug.com/40386158): Separate container enum from codec mask value.
  // Potentially pass EmeMediaType and a container enum.
  SupportedCodecs GetCodecMaskForMimeType(
      const std::string& container_mime_type) const;

  // Converts a full `codec_string` (e.g. vp09.02.10.10) to an EmeCodec. Returns
  // EME_CODEC_NONE is the |codec_string| is invalid or not supported by EME.
  EmeCodec GetEmeCodecForString(EmeMediaType media_type,
                                const std::string& container_mime_type,
                                const std::string& codec_string) const;

  // Whether the supported key systems are still up to date.
  bool is_updating_ = false;

  // Pending callbacks for UpdateIfNeeded() calls.
  base::OnceClosureList update_callbacks_;

  // Vector of KeySystemInfo .
  KeySystemInfos key_system_info_vector_;

  // This member should only be modified by RegisterMimeType().
  MimeTypeToCodecsMap mime_type_to_codecs_map_;

  // For unit test only.
  CodecMap codec_map_for_testing_;

  SupportedCodecs audio_codec_mask_ = EME_CODEC_AUDIO_ALL;
  SupportedCodecs video_codec_mask_ = EME_CODEC_VIDEO_ALL;

  // Makes sure all methods are called from the same thread.
  base::ThreadChecker thread_checker_;

  std::unique_ptr<KeySystemSupportRegistration>
      key_system_support_registration_;

  // Callback that is used to initialize key systems.
  RegisterKeySystemsSupportCB register_key_systems_support_cb_;

  base::WeakPtrFactory<KeySystemsImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_KEY_SYSTEMS_IMPL_H_
