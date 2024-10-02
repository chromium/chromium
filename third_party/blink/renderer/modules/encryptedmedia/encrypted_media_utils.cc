// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"

#include "base/notreached.h"
#include "media/base/eme_constants.h"
#include "media/base/key_systems.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

namespace {

const char kTemporary[] = "temporary";
const char kPersistentLicense[] = "persistent-license";

}  // namespace

// static
media::EmeInitDataType EncryptedMediaUtils::ConvertToInitDataType(
    const String& init_data_type) {
  if (init_data_type == "cenc")
    return media::EmeInitDataType::CENC;
  if (init_data_type == "keyids")
    return media::EmeInitDataType::KEYIDS;
  if (init_data_type == "webm")
    return media::EmeInitDataType::WEBM;

  // |initDataType| is not restricted in the idl, so anything is possible.
  return media::EmeInitDataType::UNKNOWN;
}

// static
String EncryptedMediaUtils::ConvertFromInitDataType(
    media::EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case media::EmeInitDataType::CENC:
      return "cenc";
    case media::EmeInitDataType::KEYIDS:
      return "keyids";
    case media::EmeInitDataType::WEBM:
      return "webm";
    case media::EmeInitDataType::UNKNOWN:
      // Chromium should not use Unknown, but we use it in Blink when the
      // actual value has been blocked for non-same-origin or mixed content.
      return String();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

// static
WebEncryptedMediaSessionType EncryptedMediaUtils::ConvertToSessionType(
    const String& session_type) {
  if (session_type == kTemporary)
    return WebEncryptedMediaSessionType::kTemporary;
  if (session_type == kPersistentLicense)
    return WebEncryptedMediaSessionType::kPersistentLicense;

  // |sessionType| is not restricted in the idl, so anything is possible.
  return WebEncryptedMediaSessionType::kUnknown;
}

// static
String EncryptedMediaUtils::ConvertFromSessionType(
    WebEncryptedMediaSessionType session_type) {
  switch (session_type) {
    case WebEncryptedMediaSessionType::kTemporary:
      return kTemporary;
    case WebEncryptedMediaSessionType::kPersistentLicense:
      return kPersistentLicense;
    case WebEncryptedMediaSessionType::kUnknown:
      // Unexpected session type from Chromium.
      NOTREACHED_IN_MIGRATION();
      return String();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

// static
String EncryptedMediaUtils::ConvertKeyStatusToString(
    const WebEncryptedMediaKeyInformation::KeyStatus status) {
  switch (status) {
    case WebEncryptedMediaKeyInformation::KeyStatus::kUsable:
      return "usable";
    case WebEncryptedMediaKeyInformation::KeyStatus::kExpired:
      return "expired";
    case WebEncryptedMediaKeyInformation::KeyStatus::kReleased:
      return "released";
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputRestricted:
      return "output-restricted";
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputDownscaled:
      return "output-downscaled";
    case WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending:
      return "status-pending";
    case WebEncryptedMediaKeyInformation::KeyStatus::kInternalError:
      return "internal-error";
  }

  NOTREACHED_IN_MIGRATION();
  return "internal-error";
}

// static
V8MediaKeyStatus EncryptedMediaUtils::ConvertKeyStatusToEnum(
    const WebEncryptedMediaKeyInformation::KeyStatus status) {
  switch (status) {
    case WebEncryptedMediaKeyInformation::KeyStatus::kUsable:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kUsable);
    case WebEncryptedMediaKeyInformation::KeyStatus::kExpired:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kExpired);
    case WebEncryptedMediaKeyInformation::KeyStatus::kReleased:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kReleased);
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputRestricted:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kOutputRestricted);
    case WebEncryptedMediaKeyInformation::KeyStatus::kOutputDownscaled:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kOutputDownscaled);
    case WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kStatusPending);
    case WebEncryptedMediaKeyInformation::KeyStatus::kInternalError:
      return V8MediaKeyStatus(V8MediaKeyStatus::Enum::kInternalError);
  }
  NOTREACHED();
}

// static
WebMediaKeySystemConfiguration::Requirement
EncryptedMediaUtils::ConvertToMediaKeysRequirement(
    V8MediaKeysRequirement::Enum requirement) {
  switch (requirement) {
    case V8MediaKeysRequirement::Enum::kRequired:
      return WebMediaKeySystemConfiguration::Requirement::kRequired;
    case V8MediaKeysRequirement::Enum::kOptional:
      return WebMediaKeySystemConfiguration::Requirement::kOptional;
    case V8MediaKeysRequirement::Enum::kNotAllowed:
      return WebMediaKeySystemConfiguration::Requirement::kNotAllowed;
  }
  NOTREACHED();
}

// static
V8MediaKeysRequirement::Enum
EncryptedMediaUtils::ConvertMediaKeysRequirementToEnum(
    WebMediaKeySystemConfiguration::Requirement requirement) {
  switch (requirement) {
    case WebMediaKeySystemConfiguration::Requirement::kRequired:
      return V8MediaKeysRequirement::Enum::kRequired;
    case WebMediaKeySystemConfiguration::Requirement::kOptional:
      return V8MediaKeysRequirement::Enum::kOptional;
    case WebMediaKeySystemConfiguration::Requirement::kNotAllowed:
      return V8MediaKeysRequirement::Enum::kNotAllowed;
  }
  NOTREACHED();
}

// static
WebEncryptedMediaClient*
EncryptedMediaUtils::GetEncryptedMediaClientFromLocalDOMWindow(
    LocalDOMWindow* window) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(window->GetFrame());
  return web_frame->Client()->EncryptedMediaClient();
}

// static
void EncryptedMediaUtils::ReportUsage(EmeApiType api_type,
                                      ExecutionContext* execution_context,
                                      const String& key_system,
                                      bool use_hardware_secure_codecs,
                                      bool is_persistent_session) {
  if (!execution_context) {
    return;
  }

  ukm::builders::Media_EME_Usage builder(execution_context->UkmSourceID());
  builder.SetKeySystem(media::GetKeySystemIntForUKM(key_system.Ascii()));
  builder.SetUseHardwareSecureCodecs(use_hardware_secure_codecs);
  builder.SetApi(static_cast<int>(api_type));
  builder.SetIsPersistentSession(is_persistent_session);
  builder.Record(execution_context->UkmRecorder());
}

}  // namespace blink
