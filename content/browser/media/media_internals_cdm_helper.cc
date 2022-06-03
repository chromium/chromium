// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_cdm_helper.h"

#include <memory>
#include <vector>

#include "base/values.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/cdm_info.h"
#include "media/base/audio_codecs.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"

namespace content {

namespace {

std::string GetCdmInfoRobustnessName(CdmInfo::Robustness robustness) {
  switch (robustness) {
    case CdmInfo::Robustness::kHardwareSecure:
      return "hardware-secure";
    case CdmInfo::Robustness::kSoftwareSecure:
      return "software-secure";
  }
}

std::string GetCdmSessionTypeName(media::CdmSessionType session_type) {
  switch (session_type) {
    case media::CdmSessionType::kTemporary:
      return "temporary";
    case media::CdmSessionType::kPersistentLicense:
      return "persistent-license";
  }
}

std::unique_ptr<base::ListValue> VideoCodecProfilesToValue(
    const std::vector<media::VideoCodecProfile>& profiles) {
  auto list = std::make_unique<base::ListValue>();
  for (const auto& profile : profiles)
    list->Append(media::GetProfileName(profile));
  return list;
}

std::unique_ptr<base::DictionaryValue> CdmCapabilityToValue(
    const media::CdmCapability& cdm_capability) {
  auto dict = std::make_unique<base::DictionaryValue>();

  auto audio_codec_list = std::make_unique<base::ListValue>();
  for (const auto& audio_codec : cdm_capability.audio_codecs)
    audio_codec_list->Append(media::GetCodecName(audio_codec));
  dict->SetList("Audio Codecs", std::move(audio_codec_list));

  auto video_codec_dict = std::make_unique<base::DictionaryValue>();
  for (const auto& video_codec_map : cdm_capability.video_codecs) {
    auto codec_name = media::GetCodecName(video_codec_map.first);
    auto& profiles = video_codec_map.second;
    video_codec_dict->SetList(codec_name, VideoCodecProfilesToValue(profiles));
  }
  dict->SetDictionary("Video Codecs", std::move(video_codec_dict));

  auto encryption_scheme_list = std::make_unique<base::ListValue>();
  for (const auto& encryption_scheme : cdm_capability.encryption_schemes) {
    encryption_scheme_list->Append(
        media::GetEncryptionSchemeName(encryption_scheme));
  }
  dict->SetList("Encryption Schemes", std::move(encryption_scheme_list));

  auto session_type_list = std::make_unique<base::ListValue>();
  for (const auto& session_type : cdm_capability.session_types)
    session_type_list->Append(GetCdmSessionTypeName(session_type));
  dict->SetList("Session Types", std::move(session_type_list));

  return dict;
}

std::unique_ptr<base::DictionaryValue> CdmInfoToValue(const CdmInfo& cdm_info) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString("key_system", cdm_info.key_system);
  dict->SetString("robustness", cdm_info.name);
  dict->SetString("name", GetCdmInfoRobustnessName(cdm_info.robustness));
  dict->SetString("version", cdm_info.version.GetString());
  dict->SetString("path", cdm_info.path.AsUTF8Unsafe());

  if (cdm_info.capability) {
    dict->SetDictionary("capability",
                        CdmCapabilityToValue(cdm_info.capability.value()));
  } else {
    // This could happen if hardware secure capabilities are overridden or
    // hardware video decode is disabled from command line.
    dict->SetString("capability", "No Capability");
  }

  return dict;
}

std::u16string SerializeUpdate(const std::string& function,
                               const base::Value* value) {
  return content::WebUI::GetJavascriptCall(
      function, std::vector<const base::Value*>(1, value));
}

}  // namespace

MediaInternalsCdmHelper::MediaInternalsCdmHelper() = default;

MediaInternalsCdmHelper::~MediaInternalsCdmHelper() = default;

void MediaInternalsCdmHelper::GetRegisteredCdms() {
  if (!pending_key_systems_.empty())
    return;

  auto cdms = CdmRegistryImpl::GetInstance()->GetRegisteredCdms();

  // Trigger IsKeySystemSupported() for each key system so lazy initialized
  // CdmInfo will be finalized.
  for (const auto& cdm_info : cdms) {
    const auto& key_system = cdm_info.key_system;
    if (pending_key_systems_.count(key_system))
      continue;

    pending_key_systems_.insert(key_system);

    // BindToCurrentLoop() is needed in case the callback called synchronously.
    key_system_support_impl_.IsKeySystemSupported(
        key_system, media::BindToCurrentLoop(base::BindOnce(
                        &MediaInternalsCdmHelper::OnCdmInfoFinalized,
                        weak_factory_.GetWeakPtr(), key_system)));
  }
}

// Ignore results since we'll get them from CdmRegistryImpl directly.
void MediaInternalsCdmHelper::OnCdmInfoFinalized(
    const std::string& key_system,
    bool /*success*/,
    media::mojom::KeySystemCapabilityPtr /*capability*/) {
  DCHECK(pending_key_systems_.count(key_system));
  pending_key_systems_.erase(key_system);

  // Send update when all registered key systems are finalized.
  if (pending_key_systems_.empty())
    SendCdmUpdate();
}

void MediaInternalsCdmHelper::SendCdmUpdate() {
  auto cdms = CdmRegistryImpl::GetInstance()->GetRegisteredCdms();

  base::ListValue cdm_list;
  for (const auto& cdm_info : cdms) {
    auto cdm_info_dict = CdmInfoToValue(cdm_info);
    cdm_list.Append(std::move(cdm_info_dict));
  }

  return MediaInternals::GetInstance()->SendUpdate(
      SerializeUpdate("media.updateRegisteredCdms", &cdm_list));
}

}  // namespace content
