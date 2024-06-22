// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_cdm_helper.h"

#include <memory>
#include <string_view>

#include "base/values.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/web_ui.h"
#include "media/base/audio_codecs.h"
#include "media/base/cdm_capability.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"

namespace content {

namespace {

std::string GetCdmInfoCapabilityStatusName(CdmInfo::Status status) {
  switch (status) {
    case CdmInfo::Status::kUninitialized:
      return "Uninitialized";
    case CdmInfo::Status::kEnabled:
      return "Enabled";
    case CdmInfo::Status::kCommandLineOverridden:
      return "Overridden from command line";
    case CdmInfo::Status::kHardwareSecureDecryptionDisabled:
      return "Disabled because Hardware Secure Decryption is disabled";
    case CdmInfo::Status::kAcceleratedVideoDecodeDisabled:
      return "Disabled because Accelerated Video Decode is disabled";
    case CdmInfo::Status::kGpuFeatureDisabled:
      return "Disabled via GPU feature (e.g. bad GPU or driver)";
    case CdmInfo::Status::kGpuCompositionDisabled:
      return "Disabled because GPU (direct) composition is disabled";
    case CdmInfo::Status::kDisabledByPref:
      return "Disabled due to previous errors (stored in Local State)";
    case CdmInfo::Status::kDisabledOnError:
      return "Disabled after errors or crashes";
    case CdmInfo::Status::kDisabledBySoftwareEmulatedGpu:
      return "Disabled because software emulated GPU is enabled";
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

base::Value::List VideoCodecInfoToList(
    const media::VideoCodecInfo& video_codec_info) {
  auto& profiles = video_codec_info.supported_profiles;

  base::Value::List list;
  for (const auto& profile : profiles) {
    list.Append(media::GetProfileName(profile));
  }

  return list;
}

base::Value::Dict CdmCapabilityToDict(
    const media::CdmCapability& cdm_capability) {
  base::Value::Dict dict;

  base::Value::List audio_codec_list;
  for (const auto& audio_codec : cdm_capability.audio_codecs) {
    audio_codec_list.Append(media::GetCodecName(audio_codec));
  }
  dict.Set("Audio Codecs", std::move(audio_codec_list));

  auto* video_codec_dict = dict.EnsureDict("Video Codecs");
  for (const auto& [video_codec, video_codec_info] :
       cdm_capability.video_codecs) {
    auto codec_name = media::GetCodecName(video_codec);
    // Codecs marked with "*" signals clear lead not supported.
    if (!video_codec_info.supports_clear_lead) {
      codec_name += "*";
    }
    video_codec_dict->Set(codec_name, VideoCodecInfoToList(video_codec_info));
  }

  base::Value::List encryption_scheme_list;
  for (const auto& encryption_scheme : cdm_capability.encryption_schemes) {
    encryption_scheme_list.Append(
        media::GetEncryptionSchemeName(encryption_scheme));
  }
  dict.Set("Encryption Schemes", std::move(encryption_scheme_list));

  base::Value::List session_type_list;
  for (const auto& session_type : cdm_capability.session_types) {
    session_type_list.Append(GetCdmSessionTypeName(session_type));
  }
  dict.Set("Session Types", std::move(session_type_list));

  return dict;
}

base::Value::Dict CdmInfoToDict(const CdmInfo& cdm_info) {
  base::Value::Dict dict;
  dict.Set("key_system", cdm_info.key_system);
  dict.Set("robustness", GetCdmInfoRobustnessName(cdm_info.robustness));
  dict.Set("name", cdm_info.name);
  dict.Set("version",
           cdm_info.version.IsValid() ? cdm_info.version.GetString() : "N/A");
  dict.Set("path",
           cdm_info.path.empty() ? "N/A" : cdm_info.path.AsUTF8Unsafe());
  dict.Set("status", GetCdmInfoCapabilityStatusName(cdm_info.status));

  if (cdm_info.capability) {
    dict.Set("capability", CdmCapabilityToDict(cdm_info.capability.value()));
  } else {
    // This could happen if hardware secure capabilities are overridden or
    // hardware video decode is disabled from command line.
    dict.Set("capability", "No Capability");
  }

  return dict;
}

std::u16string SerializeUpdate(std::string_view function,
                               const base::Value::List& value) {
  base::ValueView args[] = {value};
  return content::WebUI::GetJavascriptCall(function, args);
}

}  // namespace

MediaInternalsCdmHelper::MediaInternalsCdmHelper() = default;

MediaInternalsCdmHelper::~MediaInternalsCdmHelper() = default;

void MediaInternalsCdmHelper::GetRegisteredCdms() {
  // Ok to trigger hw secure capability check since this page is for debugging
  // only and not part of the normal user flow.
  cb_subscription_ =
      CdmRegistryImpl::GetInstance()->ObserveKeySystemCapabilities(
          /*allow_hw_secure_capability_check=*/true,
          base::BindRepeating(
              &MediaInternalsCdmHelper::OnKeySystemCapabilitiesUpdated,
              weak_factory_.GetWeakPtr()));
}

// Ignore results since we'll get them from CdmRegistryImpl directly.
void MediaInternalsCdmHelper::OnKeySystemCapabilitiesUpdated(
    KeySystemCapabilities /*capabilities*/) {
  auto cdms = CdmRegistryImpl::GetInstance()->GetRegisteredCdms();

  base::Value::List cdm_list;
  for (const auto& cdm_info : cdms) {
    DCHECK(cdm_info.status != CdmInfo::Status::kUninitialized);
    cdm_list.Append(CdmInfoToDict(cdm_info));
  }

  return MediaInternals::GetInstance()->SendUpdate(
      SerializeUpdate("media.updateRegisteredCdms", cdm_list));
}

}  // namespace content
