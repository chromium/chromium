// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_cdm_helper.h"

#include <memory>

#include "base/values.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/web_ui.h"
#include "media/base/audio_codecs.h"
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

base::Value VideoCodecProfilesToValue(
    const std::vector<media::VideoCodecProfile>& profiles) {
  base::Value list(base::Value::Type::LIST);
  for (const auto& profile : profiles)
    list.Append(media::GetProfileName(profile));
  return list;
}

base::Value CdmCapabilityToValue(const media::CdmCapability& cdm_capability) {
  base::Value dict(base::Value::Type::DICTIONARY);

  base::Value audio_codec_list(base::Value::Type::LIST);
  for (const auto& audio_codec : cdm_capability.audio_codecs)
    audio_codec_list.Append(media::GetCodecName(audio_codec));
  dict.SetKey("Audio Codecs", std::move(audio_codec_list));

  auto* video_codec_dict =
      dict.SetKey("Video Codecs", base::Value(base::Value::Type::DICTIONARY));
  for (const auto& video_codec_map : cdm_capability.video_codecs) {
    auto codec_name = media::GetCodecName(video_codec_map.first);
    auto& profiles = video_codec_map.second;
    video_codec_dict->SetPath(codec_name, VideoCodecProfilesToValue(profiles));
  }

  base::Value encryption_scheme_list(base::Value::Type::LIST);
  for (const auto& encryption_scheme : cdm_capability.encryption_schemes) {
    encryption_scheme_list.Append(
        media::GetEncryptionSchemeName(encryption_scheme));
  }
  dict.SetKey("Encryption Schemes", std::move(encryption_scheme_list));

  base::Value session_type_list(base::Value::Type::LIST);
  for (const auto& session_type : cdm_capability.session_types)
    session_type_list.Append(GetCdmSessionTypeName(session_type));
  dict.SetKey("Session Types", std::move(session_type_list));

  return dict;
}

base::Value CdmInfoToValue(const CdmInfo& cdm_info) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("key_system", cdm_info.key_system);
  dict.SetStringKey("robustness", cdm_info.name);
  dict.SetStringKey("name", GetCdmInfoRobustnessName(cdm_info.robustness));
  dict.SetStringKey("version", cdm_info.version.GetString());
  dict.SetStringKey("path", cdm_info.path.AsUTF8Unsafe());

  if (cdm_info.capability) {
    dict.SetKey("capability",
                CdmCapabilityToValue(cdm_info.capability.value()));
  } else {
    // This could happen if hardware secure capabilities are overridden or
    // hardware video decode is disabled from command line.
    dict.SetStringKey("capability", "No Capability");
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
  CdmRegistryImpl::GetInstance()->ObserveKeySystemCapabilities(
      base::BindRepeating(
          &MediaInternalsCdmHelper::OnKeySystemCapabilitiesUpdated,
          weak_factory_.GetWeakPtr()));
}

// Ignore results since we'll get them from CdmRegistryImpl directly.
void MediaInternalsCdmHelper::OnKeySystemCapabilitiesUpdated(
    KeySystemCapabilities /*capabilities*/) {
  auto cdms = CdmRegistryImpl::GetInstance()->GetRegisteredCdms();

  base::Value cdm_list(base::Value::Type::LIST);
  for (const auto& cdm_info : cdms) {
    cdm_list.Append(CdmInfoToValue(cdm_info));
  }

  return MediaInternals::GetInstance()->SendUpdate(
      SerializeUpdate("media.updateRegisteredCdms", &cdm_list));
}

}  // namespace content
