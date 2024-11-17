// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/android/media_drm_bridge.h"

#include <stddef.h>
#include <sys/system_properties.h>

#include <memory>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/android/android_util.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge_client.h"
#include "media/base/android/media_drm_bridge_delegate.h"
#include "media/base/cdm_key_information.h"
#include "media/base/logging_override_if_enabled.h"
#include "media/base/media_drm_key_type.h"
#include "media/base/media_switches.h"
#include "media/base/provision_fetcher.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/MediaDrmBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaByteArrayToByteVector;
using base::android::JavaByteArrayToString;
using base::android::JavaObjectArrayReader;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using jni_zero::AttachCurrentThread;

namespace media {

namespace {

using CreateMediaDrmBridgeCB = base::OnceCallback<scoped_refptr<MediaDrmBridge>(
    const std::string& /* origin_id */)>;

// These must be in sync with Android MediaDrm REQUEST_TYPE_XXX constants!
// https://developer.android.com/reference/android/media/MediaDrm.KeyRequest.html
enum class RequestType : uint32_t {
  REQUEST_TYPE_INITIAL = 0,
  REQUEST_TYPE_RENEWAL = 1,
  REQUEST_TYPE_RELEASE = 2,
};

// These must be in sync with Android MediaDrm KEY_STATUS_XXX constants:
// https://developer.android.com/reference/android/media/MediaDrm.KeyStatus.html
enum class KeyStatus : uint32_t {
  KEY_STATUS_USABLE = 0,
  KEY_STATUS_EXPIRED = 1,
  KEY_STATUS_OUTPUT_NOT_ALLOWED = 2,
  KEY_STATUS_PENDING = 3,
  KEY_STATUS_INTERNAL_ERROR = 4,
  KEY_STATUS_USABLE_IN_FUTURE = 5,  // Added in API level 29.
};

// Convert |init_data_type| to a string supported by MediaDRM.
// "audio"/"video" does not matter, so use "video".
std::string ConvertInitDataType(EmeInitDataType init_data_type) {
  // TODO(jrummell/xhwang): EME init data types like "webm" and "cenc" are
  // supported in API level >=21 for Widevine key system. Switch to use those
  // strings when they are officially supported in Android for all key systems.
  switch (init_data_type) {
    case EmeInitDataType::WEBM:
      return "video/webm";
    case EmeInitDataType::CENC:
      return "video/mp4";
    case EmeInitDataType::KEYIDS:
      return "keyids";
    case EmeInitDataType::UNKNOWN:
      NOTREACHED();
  }
  NOTREACHED();
}

// Convert CdmSessionType to MediaDrmKeyType supported by MediaDrm.
MediaDrmKeyType ConvertCdmSessionType(CdmSessionType session_type) {
  switch (session_type) {
    case CdmSessionType::kTemporary:
      return MediaDrmKeyType::STREAMING;
    case CdmSessionType::kPersistentLicense:
      return MediaDrmKeyType::OFFLINE;

    default:
      LOG(WARNING) << "Unsupported session type "
                   << static_cast<int>(session_type);
      return MediaDrmKeyType::STREAMING;
  }
}

CdmMessageType GetMessageType(RequestType request_type) {
  switch (request_type) {
    case RequestType::REQUEST_TYPE_INITIAL:
      return CdmMessageType::LICENSE_REQUEST;
    case RequestType::REQUEST_TYPE_RENEWAL:
      return CdmMessageType::LICENSE_RENEWAL;
    case RequestType::REQUEST_TYPE_RELEASE:
      return CdmMessageType::LICENSE_RELEASE;
  }

  NOTREACHED();
}

CdmKeyInformation::KeyStatus ConvertKeyStatus(KeyStatus key_status,
                                              bool is_key_release) {
  switch (key_status) {
    case KeyStatus::KEY_STATUS_USABLE:
      return CdmKeyInformation::USABLE;
    case KeyStatus::KEY_STATUS_EXPIRED:
      return is_key_release ? CdmKeyInformation::RELEASED
                            : CdmKeyInformation::EXPIRED;
    case KeyStatus::KEY_STATUS_OUTPUT_NOT_ALLOWED:
      return CdmKeyInformation::OUTPUT_RESTRICTED;
    case KeyStatus::KEY_STATUS_PENDING:
      // On pre-Q versions of Android, 'status-pending' really means "usable in
      // the future". Translate this to 'expired' as that's the only status that
      // makes sense in this case. Starting with Android Q, 'status-pending'
      // means what you expect. See crbug.com/889272 for explanation.
      // TODO(jrummell): "KEY_STATUS_PENDING" should probably be renamed to
      // "STATUS_PENDING".
      return (base::android::BuildInfo::GetInstance()->sdk_int() <=
              base::android::SDK_VERSION_P)
                 ? CdmKeyInformation::EXPIRED
                 : CdmKeyInformation::KEY_STATUS_PENDING;
    case KeyStatus::KEY_STATUS_INTERNAL_ERROR:
      return CdmKeyInformation::INTERNAL_ERROR;
    case KeyStatus::KEY_STATUS_USABLE_IN_FUTURE:
      // This was added in Android Q.
      // https://developer.android.com/reference/android/media/MediaDrm.KeyStatus.html#STATUS_USABLE_IN_FUTURE
      // notes this happens "because the start time is in the future." There is
      // no matching EME status, so returning EXPIRED as the closest match.
      return CdmKeyInformation::EXPIRED;
  }

  NOTREACHED();
}

class KeySystemManager {
 public:
  KeySystemManager();

  KeySystemManager(const KeySystemManager&) = delete;
  KeySystemManager& operator=(const KeySystemManager&) = delete;

  UUID GetUUID(const std::string& key_system);
  std::vector<std::string> GetPlatformKeySystemNames();

 private:
  using KeySystemUuidMap = MediaDrmBridgeClient::KeySystemUuidMap;

  KeySystemUuidMap key_system_uuid_map_;
};

KeySystemManager::KeySystemManager() {
  // Widevine is always supported in Android.
  key_system_uuid_map_[kWidevineKeySystem] =
      UUID(kWidevineUuid, kWidevineUuid + std::size(kWidevineUuid));
  // External Clear Key is supported only for testing.
  if (base::FeatureList::IsEnabled(kExternalClearKeyForTesting)) {
    key_system_uuid_map_[kExternalClearKeyKeySystem] =
        UUID(kClearKeyUuid, kClearKeyUuid + std::size(kClearKeyUuid));
  }
  MediaDrmBridgeClient* client = GetMediaDrmBridgeClient();
  if (client)
    client->AddKeySystemUUIDMappings(&key_system_uuid_map_);
}

UUID KeySystemManager::GetUUID(const std::string& key_system) {
  KeySystemUuidMap::iterator it = key_system_uuid_map_.find(key_system);
  if (it == key_system_uuid_map_.end())
    return UUID();
  return it->second;
}

std::vector<std::string> KeySystemManager::GetPlatformKeySystemNames() {
  std::vector<std::string> key_systems;
  for (KeySystemUuidMap::iterator it = key_system_uuid_map_.begin();
       it != key_system_uuid_map_.end(); ++it) {
    // Rule out the key system handled by Chrome explicitly.
    if (it->first != kWidevineKeySystem)
      key_systems.push_back(it->first);
  }
  return key_systems;
}

KeySystemManager* GetKeySystemManager() {
  static KeySystemManager* ksm = new KeySystemManager();
  return ksm;
}

// Checks whether |key_system| is supported with |container_mime_type|. Only
// checks |key_system| support if |container_mime_type| is empty.
// TODO(xhwang): The |container_mime_type| is not the same as contentType in
// the EME spec. Revisit this once the spec issue with initData type is
// resolved.
bool IsKeySystemSupportedWithTypeImpl(const std::string& key_system,
                                      const std::string& container_mime_type) {
  CHECK(!key_system.empty());

  UUID scheme_uuid = GetKeySystemManager()->GetUUID(key_system);
  if (scheme_uuid.empty()) {
    DVLOG(1) << "Cannot get UUID for key system " << key_system;
    return false;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_scheme_uuid =
      base::android::ToJavaByteArray(env, &scheme_uuid[0], scheme_uuid.size());
  ScopedJavaLocalRef<jstring> j_container_mime_type =
      ConvertUTF8ToJavaString(env, container_mime_type);
  bool supported = Java_MediaDrmBridge_isCryptoSchemeSupported(
      env, j_scheme_uuid, j_container_mime_type);
  DVLOG_IF(1, !supported) << "Crypto scheme not supported for " << key_system
                          << " with " << container_mime_type;
  return supported;
}

MediaDrmBridge::SecurityLevel GetSecurityLevelFromString(
    const std::string& security_level_str) {
  if (0 == security_level_str.compare("L1"))
    return MediaDrmBridge::SECURITY_LEVEL_1;
  if (0 == security_level_str.compare("L3"))
    return MediaDrmBridge::SECURITY_LEVEL_3;
  DCHECK(security_level_str.empty());
  return MediaDrmBridge::SECURITY_LEVEL_DEFAULT;
}

// Converts from String value returned from MediaDrm to an enum of HdcpVersion
// values. Refer to http://shortn/_eFj9y8KBgR for the list of Strings that could
// possibly be returned.
HdcpVersion ToEmeHdcpVersion(const std::string& hdcp_level_str) {
  if (hdcp_level_str == "Disconnected") {
    // This means no external device is connected and the default screen is
    // considered fully protected, so we return the max value possible.
    return HdcpVersion::kHdcpVersion2_3;
  }
  if (hdcp_level_str == "Unprotected" || hdcp_level_str == "" ||
      hdcp_level_str == "HDCP-LevelUnknown") {
    return HdcpVersion::kHdcpVersionNone;
  }
  if (hdcp_level_str == "HDCP-1.0") {
    return HdcpVersion::kHdcpVersion1_0;
  }
  if (hdcp_level_str == "HDCP-1.1") {
    return HdcpVersion::kHdcpVersion1_1;
  }
  if (hdcp_level_str == "HDCP-1.2") {
    return HdcpVersion::kHdcpVersion1_2;
  }
  if (hdcp_level_str == "HDCP-1.3") {
    return HdcpVersion::kHdcpVersion1_3;
  }
  if (hdcp_level_str == "HDCP-1.4") {
    return HdcpVersion::kHdcpVersion1_4;
  }
  if (hdcp_level_str == "HDCP-1.x") {
    // Older versions of MediaDrm might return 1.x. This is equivalent to 1.4.
    return HdcpVersion::kHdcpVersion1_4;
  }
  if (hdcp_level_str == "HDCP-2.0") {
    return HdcpVersion::kHdcpVersion2_0;
  }
  if (hdcp_level_str == "HDCP-2.1") {
    return HdcpVersion::kHdcpVersion2_1;
  }
  if (hdcp_level_str == "HDCP-2.2") {
    return HdcpVersion::kHdcpVersion2_2;
  }
  if (hdcp_level_str == "HDCP-2.3") {
    return HdcpVersion::kHdcpVersion2_3;
  }

  LOG(WARNING) << "Unexpected HdcpLevel " << hdcp_level_str
               << " from MediaDrm, returning lowest value.";
  return HdcpVersion::kHdcpVersionNone;
}

// Do not change the return values as they are part of Android MediaDrm API
// for Widevine.
std::string GetSecurityLevelString(
    MediaDrmBridge::SecurityLevel security_level) {
  switch (security_level) {
    case MediaDrmBridge::SECURITY_LEVEL_DEFAULT:
      return "";
    case MediaDrmBridge::SECURITY_LEVEL_1:
      return "L1";
    case MediaDrmBridge::SECURITY_LEVEL_3:
      return "L3";
  }
  return "";
}

int GetFirstApiLevel() {
  JNIEnv* env = AttachCurrentThread();
  int first_api_level = Java_MediaDrmBridge_getFirstApiLevel(env);
  return first_api_level;
}

CreateCdmTypedStatus ConvertMediaDrmCreateError(
    MediaDrmBridge::MediaDrmCreateError error,
    MediaDrmBridge::SecurityLevel security_level) {
  switch (error) {
    case MediaDrmBridge::MediaDrmCreateError::SUCCESS:
      return CreateCdmTypedStatus::Codes::kSuccess;
    case MediaDrmBridge::MediaDrmCreateError::UNSUPPORTED_DRM_SCHEME:
      return CreateCdmTypedStatus::Codes::kUnsupportedKeySystem;
    case MediaDrmBridge::MediaDrmCreateError::MEDIADRM_ILLEGAL_ARGUMENT:
      return CreateCdmTypedStatus::Codes::kAndroidMediaDrmIllegalArgument;
    case MediaDrmBridge::MediaDrmCreateError::MEDIADRM_ILLEGAL_STATE:
      return CreateCdmTypedStatus::Codes::kAndroidMediaDrmIllegalState;
    case MediaDrmBridge::MediaDrmCreateError::FAILED_SECURITY_LEVEL:
      return (security_level == MediaDrmBridge::SECURITY_LEVEL_1)
                 ? CreateCdmTypedStatus::Codes::kAndroidFailedL1SecurityLevel
                 : CreateCdmTypedStatus::Codes::kAndroidFailedL3SecurityLevel;
    case MediaDrmBridge::MediaDrmCreateError::FAILED_SECURITY_ORIGIN:
      return CreateCdmTypedStatus::Codes::kAndroidFailedSecurityOrigin;
    case MediaDrmBridge::MediaDrmCreateError::FAILED_MEDIA_CRYPTO_SESSION:
      return CreateCdmTypedStatus::Codes::kAndroidFailedMediaCryptoSession;
    case MediaDrmBridge::MediaDrmCreateError::FAILED_TO_START_PROVISIONING:
      return CreateCdmTypedStatus::Codes::kAndroidFailedToStartProvisioning;
    case MediaDrmBridge::MediaDrmCreateError::FAILED_MEDIA_CRYPTO_CREATE:
      return CreateCdmTypedStatus::Codes::kAndroidFailedMediaCryptoCreate;
    case MediaDrmBridge::MediaDrmCreateError::UNSUPPORTED_MEDIACRYPTO_SCHEME:
      return CreateCdmTypedStatus::Codes::kAndroidUnsupportedMediaCryptoScheme;
  }

  return CreateCdmTypedStatus::Codes::kUnknownError;
}

}  // namespace

// static
bool MediaDrmBridge::IsKeySystemSupported(const std::string& key_system) {
  return IsKeySystemSupportedWithTypeImpl(key_system, "");
}

// static
bool MediaDrmBridge::IsPerApplicationProvisioningSupported() {
  // Start by checking "ro.product.first_api_level", which may not exist.
  // If it is non-zero, then it is the API level.
  // Checking FirstApiLevel is known to be expensive (see crbug.com/1366106),
  // and thus is cached.
  static int first_api_level = GetFirstApiLevel();
  DVLOG(1) << "first_api_level = " << first_api_level;
  if (first_api_level >= base::android::SDK_VERSION_OREO)
    return true;

  // If "ro.product.first_api_level" does not match, then check build number.
  DVLOG(1) << "api_level = "
           << base::android::BuildInfo::GetInstance()->sdk_int();
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_OREO;
}

// static
bool MediaDrmBridge::IsPersistentLicenseTypeSupported(
    const std::string& /* key_system */) {
  // TODO(yucliu): Check |key_system| if persistent license is supported by
  // MediaDrm.
  return base::FeatureList::IsEnabled(kMediaDrmPersistentLicense);
}

// static
bool MediaDrmBridge::IsKeySystemSupportedWithType(
    const std::string& key_system,
    const std::string& container_mime_type) {
  DCHECK(!container_mime_type.empty()) << "Call IsKeySystemSupported instead";

  return IsKeySystemSupportedWithTypeImpl(key_system, container_mime_type);
}

// static
std::vector<std::string> MediaDrmBridge::GetPlatformKeySystemNames() {
  return GetKeySystemManager()->GetPlatformKeySystemNames();
}

// static
std::vector<uint8_t> MediaDrmBridge::GetUUID(const std::string& key_system) {
  return GetKeySystemManager()->GetUUID(key_system);
}

// static
base::Version MediaDrmBridge::GetVersion(const std::string& key_system) {
  auto media_drm_bridge = MediaDrmBridge::CreateWithoutSessionSupport(
      key_system, /* origin_id= */ "", MediaDrmBridge::SECURITY_LEVEL_DEFAULT,
      "GetVersion", base::NullCallback());
  if (!media_drm_bridge.has_value()) {
    DVLOG(1) << "Unable to create MediaDrmBridge for " << key_system
             << ", CreateCdmStatus: "
             << (media::StatusCodeType)media_drm_bridge.code();
    return base::Version();
  }

  std::string version_str = media_drm_bridge->GetVersionInternal();

  // Some devices return the version with an additional level (e.g. 18.0.0@1),
  // so simply replace any '@'.
  base::ReplaceChars(version_str, "@", ".", &version_str);

  auto version = base::Version(version_str);
  DVLOG_IF(1, !version.IsValid()) << "Unable to convert " << version_str;
  return version;
}

// static
MediaDrmBridge::CdmCreationResult MediaDrmBridge::CreateInternal(
    const std::vector<uint8_t>& scheme_uuid,
    const std::string& origin_id,
    SecurityLevel security_level,
    const std::string& message,
    bool requires_media_crypto,
    std::unique_ptr<MediaDrmStorageBridge> storage,
    CreateFetcherCB create_fetcher_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  // All paths requires the MediaDrmApis.
  DCHECK(!scheme_uuid.empty());

  // TODO(crbug.com/41433110): Check that |origin_id| is specified on devices
  // that support it.

  scoped_refptr<MediaDrmBridge> media_drm_bridge(new MediaDrmBridge(
      scheme_uuid, origin_id, security_level, message, requires_media_crypto,
      std::move(storage), std::move(create_fetcher_cb), session_message_cb,
      session_closed_cb, session_keys_change_cb, session_expiration_update_cb));

  if (!media_drm_bridge->j_media_drm_) {
    DCHECK_NE(media_drm_bridge->last_create_error_,
              MediaDrmCreateError::SUCCESS);
    return ConvertMediaDrmCreateError(media_drm_bridge->last_create_error_,
                                      security_level);
  }

  return media_drm_bridge;
}

// static
MediaDrmBridge::CdmCreationResult MediaDrmBridge::CreateWithoutSessionSupport(
    const std::string& key_system,
    const std::string& origin_id,
    SecurityLevel security_level,
    const std::string& message,
    CreateFetcherCB create_fetcher_cb) {
  DVLOG(1) << __func__;

  UUID scheme_uuid = GetKeySystemManager()->GetUUID(key_system);
  if (scheme_uuid.empty())
    return CreateCdmTypedStatus::Codes::kUnsupportedKeySystem;

  // When created without session support, MediaCrypto is not needed.
  const bool requires_media_crypto = false;

  return CreateInternal(
      scheme_uuid, origin_id, security_level, message, requires_media_crypto,
      std::make_unique<MediaDrmStorageBridge>(), std::move(create_fetcher_cb),
      SessionMessageCB(), SessionClosedCB(), SessionKeysChangeCB(),
      SessionExpirationUpdateCB());
}

void MediaDrmBridge::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__ << "(" << certificate.size() << " bytes)";

  DCHECK(!certificate.empty());

  // using |cdm_promise_adapter_| for tracing.
  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_certificate =
      base::android::ToJavaByteArray(env, certificate);
  if (Java_MediaDrmBridge_setServerCertificate(env, j_media_drm_,
                                               j_certificate)) {
    ResolvePromise(promise_id);
  } else {
    RejectPromise(promise_id, CdmPromise::Exception::TYPE_ERROR,
                  MediaDrmSystemCode::SET_SERVER_CERTIFICATE_FAILED,
                  "Set server certificate failed.");
  }
}

void MediaDrmBridge::GetStatusForPolicy(
    HdcpVersion min_hdcp_version,
    std::unique_ptr<KeyStatusCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  if (!base::FeatureList::IsEnabled(kMediaDrmGetStatusForPolicy)) {
    promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                    "GetStatusForPolicy() is not supported.");
    return;
  }

  promise->resolve(min_hdcp_version <= GetCurrentHdcpLevel()
                       ? CdmKeyInformation::KeyStatus::USABLE
                       : CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED);
}

void MediaDrmBridge::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_init_data;
  ScopedJavaLocalRef<jobjectArray> j_optional_parameters;

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  MediaDrmBridgeClient* client = GetMediaDrmBridgeClient();
  if (client) {
    MediaDrmBridgeDelegate* delegate =
        client->GetMediaDrmBridgeDelegate(scheme_uuid_);
    if (delegate) {
      std::vector<uint8_t> init_data_from_delegate;
      std::vector<std::string> optional_parameters_from_delegate;
      if (!delegate->OnCreateSession(init_data_type, init_data,
                                     &init_data_from_delegate,
                                     &optional_parameters_from_delegate)) {
        RejectPromise(promise_id, CdmPromise::Exception::TYPE_ERROR,
                      MediaDrmSystemCode::CREATE_SESSION_FAILED,
                      "Invalid init data.");
        return;
      }
      if (!init_data_from_delegate.empty()) {
        j_init_data =
            base::android::ToJavaByteArray(env, init_data_from_delegate);
      }
      if (!optional_parameters_from_delegate.empty()) {
        j_optional_parameters = base::android::ToJavaArrayOfStrings(
            env, optional_parameters_from_delegate);
      }
    }
  }

  if (!j_init_data) {
    j_init_data = base::android::ToJavaByteArray(env, init_data);
  }

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, ConvertInitDataType(init_data_type));
  uint32_t key_type =
      static_cast<uint32_t>(ConvertCdmSessionType(session_type));
  Java_MediaDrmBridge_createSessionFromNative(
      env, j_media_drm_, j_init_data, j_mime, key_type, j_optional_parameters,
      promise_id);
}

void MediaDrmBridge::LoadSession(
    CdmSessionType session_type,
    const std::string& session_id,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  // Key system is not used, so just pass an empty string here.
  DCHECK(IsPersistentLicenseTypeSupported(""));

  // using |cdm_promise_adapter_| for tracing.
  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (session_type != CdmSessionType::kPersistentLicense) {
    RejectPromise(promise_id, CdmPromise::Exception::NOT_SUPPORTED_ERROR,
                  MediaDrmSystemCode::NOT_PERSISTENT_LICENSE,
                  "LoadSession() is only supported for 'persistent-license'.");
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_session_id =
      ToJavaByteArray(env, session_id);
  Java_MediaDrmBridge_loadSession(env, j_media_drm_, j_session_id, promise_id);
}

void MediaDrmBridge::UpdateSession(const std::string& session_id,
                                   const std::vector<uint8_t>& response,
                                   std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_response =
      base::android::ToJavaByteArray(env, response);
  ScopedJavaLocalRef<jbyteArray> j_session_id =
      ToJavaByteArray(env, session_id);
  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);
  Java_MediaDrmBridge_updateSession(env, j_media_drm_, j_session_id, j_response,
                                    promise_id);
}

void MediaDrmBridge::CloseSession(const std::string& session_id,
                                  std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_session_id =
      ToJavaByteArray(env, session_id);
  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);
  Java_MediaDrmBridge_closeSession(env, j_media_drm_, j_session_id, promise_id);
}

void MediaDrmBridge::RemoveSession(const std::string& session_id,
                                   std::unique_ptr<SimpleCdmPromise> promise) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(2) << __func__;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_session_id =
      ToJavaByteArray(env, session_id);
  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);
  Java_MediaDrmBridge_removeSession(env, j_media_drm_, j_session_id,
                                    promise_id);
}

CdmContext* MediaDrmBridge::GetCdmContext() {
  DVLOG(2) << __func__;
  return this;
}

void MediaDrmBridge::DeleteOnCorrectThread() const {
  DVLOG(1) << __func__;

  if (!task_runner_->BelongsToCurrentThread()) {
    // When DeleteSoon returns false, |this| will be leaked, which is okay.
    task_runner_->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

std::unique_ptr<CallbackRegistration> MediaDrmBridge::RegisterEventCB(
    EventCB event_cb) {
  return event_callbacks_.Register(std::move(event_cb));
}

MediaCryptoContext* MediaDrmBridge::GetMediaCryptoContext() {
  DVLOG(2) << __func__;
  return &media_crypto_context_;
}

bool MediaDrmBridge::IsSecureCodecRequired() {
  // For Widevine, this depends on the security level.
  // TODO(xhwang): This is specific to Widevine. See http://crbug.com/459400.
  // To fix it, we could call MediaCrypto.requiresSecureDecoderComponent().
  // See http://crbug.com/727918.
  if (base::ranges::equal(scheme_uuid_, kWidevineUuid)) {
    return SECURITY_LEVEL_1 == GetSecurityLevel();
  }

  // If UUID is ClearKey, we should automatically return false since secure
  // codecs should not be required.
  if (base::ranges::equal(scheme_uuid_, kClearKeyUuid)) {
    return false;
  }

  // For other key systems, assume true.
  return true;
}

void MediaDrmBridge::Provision(
    base::OnceCallback<void(bool)> provisioning_complete_cb) {
  DVLOG(1) << __func__;

  // CreateFetcherCB needs to be specified in order to do provisioning. No need
  // to attempt provisioning if it's not specified.
  CHECK(create_fetcher_cb_);

  // Only one provisioning request at a time.
  DCHECK(provisioning_complete_cb);
  DCHECK(!provisioning_complete_cb_);
  provisioning_complete_cb_ = std::move(provisioning_complete_cb);

  JNIEnv* env = AttachCurrentThread();
  Java_MediaDrmBridge_provision(env, j_media_drm_);
}

void MediaDrmBridge::Unprovision() {
  DVLOG(1) << __func__;

  JNIEnv* env = AttachCurrentThread();
  Java_MediaDrmBridge_unprovision(env, j_media_drm_);
}

void MediaDrmBridge::ResolvePromise(uint32_t promise_id) {
  DVLOG(2) << __func__;
  cdm_promise_adapter_.ResolvePromise(promise_id);
}

void MediaDrmBridge::ResolvePromiseWithSession(uint32_t promise_id,
                                               const std::string& session_id) {
  DVLOG(2) << __func__;
  cdm_promise_adapter_.ResolvePromise(promise_id, session_id);
}

void MediaDrmBridge::ResolvePromiseWithKeyStatus(
    uint32_t promise_id,
    CdmKeyInformation::KeyStatus key_status) {
  DVLOG(2) << __func__;
  cdm_promise_adapter_.ResolvePromise(promise_id, key_status);
}

void MediaDrmBridge::RejectPromise(uint32_t promise_id,
                                   CdmPromise::Exception exception_code,
                                   MediaDrmSystemCode system_code,
                                   const std::string& error_message) {
  DVLOG(2) << __func__;
  cdm_promise_adapter_.RejectPromise(promise_id, exception_code,
                                     base::checked_cast<uint32_t>(system_code),
                                     error_message);
}

void MediaDrmBridge::SetMediaCryptoReadyCB(
    MediaCryptoReadyCB media_crypto_ready_cb) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaDrmBridge::SetMediaCryptoReadyCB,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(media_crypto_ready_cb)));
    return;
  }

  DVLOG(1) << __func__;

  if (!media_crypto_ready_cb) {
    media_crypto_ready_cb_.Reset();
    return;
  }

  DCHECK(!media_crypto_ready_cb_);
  media_crypto_ready_cb_ = std::move(media_crypto_ready_cb);

  if (!j_media_crypto_)
    return;

  std::move(media_crypto_ready_cb_)
      .Run(CreateJavaObjectPtr(j_media_crypto_->obj()),
           IsSecureCodecRequired());
}

bool MediaDrmBridge::SetPropertyStringForTesting(
    const std::string& property_name,
    const std::string& property_value) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_property_name_string =
      ConvertUTF8ToJavaString(env, property_name);

  ScopedJavaLocalRef<jstring> j_property_value_string =
      ConvertUTF8ToJavaString(env, property_value);

  return Java_MediaDrmBridge_setPropertyStringForTesting(  // IN-TEST
      env, j_media_drm_, j_property_name_string, j_property_value_string);
}

//------------------------------------------------------------------------------
// The following OnXxx functions are called from Java. The implementation must
// only do minimal work and then post tasks to avoid reentrancy issues.

void MediaDrmBridge::OnMediaCryptoReady(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jobject>& j_media_crypto) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaDrmBridge::NotifyMediaCryptoReady,
                                weak_factory_.GetWeakPtr(),
                                CreateJavaObjectPtr(j_media_crypto.obj())));
}

void MediaDrmBridge::OnProvisionRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jstring>& j_default_url,
    const JavaParamRef<jbyteArray>& j_request_data) {
  DVLOG(1) << __func__;

  std::string request_data;
  JavaByteArrayToString(env, j_request_data, &request_data);
  std::string default_url;
  ConvertJavaStringToUTF8(env, j_default_url, &default_url);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaDrmBridge::SendProvisioningRequest,
                                weak_factory_.GetWeakPtr(), GURL(default_url),
                                std::move(request_data)));
}

void MediaDrmBridge::OnProvisioningComplete(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_media_drm,
    bool success) {
  DVLOG(1) << __func__;

  // This should only be called as result of a call to Provision().
  DCHECK(provisioning_complete_cb_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(provisioning_complete_cb_), success));
}

void MediaDrmBridge::OnPromiseResolved(JNIEnv* env,
                                       const JavaParamRef<jobject>& j_media_drm,
                                       jint j_promise_id) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaDrmBridge::ResolvePromise,
                                weak_factory_.GetWeakPtr(), j_promise_id));
}

void MediaDrmBridge::OnPromiseResolvedWithSession(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    jint j_promise_id,
    const JavaParamRef<jbyteArray>& j_session_id) {
  std::string session_id;
  JavaByteArrayToString(env, j_session_id, &session_id);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaDrmBridge::ResolvePromiseWithSession,
                                weak_factory_.GetWeakPtr(), j_promise_id,
                                std::move(session_id)));
}

void MediaDrmBridge::OnPromiseRejected(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    jint j_promise_id,
    jint j_system_code,
    const JavaParamRef<jstring>& j_error_message) {
  CHECK(j_system_code >= static_cast<jint>(MediaDrmSystemCode::MIN_VALUE) &&
        j_system_code <= static_cast<jint>(MediaDrmSystemCode::MAX_VALUE));
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaDrmBridge::RejectPromise, weak_factory_.GetWeakPtr(),
                     j_promise_id, CdmPromise::Exception::NOT_SUPPORTED_ERROR,
                     static_cast<MediaDrmSystemCode>(j_system_code),
                     ConvertJavaStringToUTF8(env, j_error_message)));
}

void MediaDrmBridge::OnSessionMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id,
    jint j_message_type,
    const JavaParamRef<jbyteArray>& j_message) {
  DVLOG(2) << __func__;

  std::vector<uint8_t> message;
  JavaByteArrayToByteVector(env, j_message, &message);
  CdmMessageType message_type =
      GetMessageType(static_cast<RequestType>(j_message_type));

  std::string session_id;
  JavaByteArrayToString(env, j_session_id, &session_id);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(session_message_cb_, std::move(session_id),
                                message_type, message));
}

void MediaDrmBridge::OnSessionClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id) {
  DVLOG(2) << __func__;
  std::string session_id;
  JavaByteArrayToString(env, j_session_id, &session_id);
  // TODO(crbug.com/40181810): Support other closed reasons.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(session_closed_cb_, std::move(session_id),
                                CdmSessionClosedReason::kClose));
}

void MediaDrmBridge::OnSessionKeysChange(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id,
    const JavaParamRef<jobjectArray>& j_keys_info,
    bool has_additional_usable_key,
    bool is_key_release) {
  DVLOG(2) << __func__;

  CdmKeysInfo cdm_keys_info;

  JavaObjectArrayReader<jobject> j_keys_info_array(j_keys_info);
  DCHECK_GT(j_keys_info_array.size(), 0);

  for (auto j_key_status : j_keys_info_array) {
    ScopedJavaLocalRef<jbyteArray> j_key_id =
        Java_KeyStatus_getKeyId(env, j_key_status);
    std::vector<uint8_t> key_id;
    JavaByteArrayToByteVector(env, j_key_id, &key_id);
    DCHECK(!key_id.empty());

    jint j_status_code = Java_KeyStatus_getStatusCode(env, j_key_status);
    CdmKeyInformation::KeyStatus key_status =
        ConvertKeyStatus(static_cast<KeyStatus>(j_status_code), is_key_release);

    DVLOG(2) << __func__ << "Key status change: " << base::HexEncode(key_id)
             << ", " << key_status;

    cdm_keys_info.push_back(
        std::make_unique<CdmKeyInformation>(key_id, key_status, 0));
  }

  std::string session_id;
  JavaByteArrayToString(env, j_session_id, &session_id);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(session_keys_change_cb_, std::move(session_id),
                     has_additional_usable_key, std::move(cdm_keys_info)));

  if (has_additional_usable_key) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaDrmBridge::OnHasAdditionalUsableKey,
                                  weak_factory_.GetWeakPtr()));
  }
}

// According to MediaDrm documentation [1], zero |expiry_time_ms| means the keys
// will never expire. This will be translated into a NULL base::Time() [2],
// which will then be mapped to a zero Java time [3]. The zero Java time is
// passed to Blink which will then be translated to NaN [4], which is what the
// spec uses to indicate that the license will never expire [5].
// [1]
// http://developer.android.com/reference/android/media/MediaDrm.OnExpirationUpdateListener.html
// [2] See base::Time::FromSecondsSinceUnixEpoch()
// [3] See base::Time::InMillisecondsSinceUnixEpoch()
// [4] See MediaKeySession::expirationChanged()
// [5] https://github.com/w3c/encrypted-media/issues/58
void MediaDrmBridge::OnSessionExpirationUpdate(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_media_drm,
    const JavaParamRef<jbyteArray>& j_session_id,
    jlong expiry_time_ms) {
  DVLOG(2) << __func__ << ": " << expiry_time_ms << " ms";
  std::string session_id;
  JavaByteArrayToString(env, j_session_id, &session_id);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          session_expiration_update_cb_, std::move(session_id),
          base::Time::FromMillisecondsSinceUnixEpoch(expiry_time_ms)));
}

void MediaDrmBridge::OnCreateError(JNIEnv* env, jint j_error_code) {
  CHECK(j_error_code >= static_cast<jint>(MediaDrmCreateError::MIN_VALUE) &&
        j_error_code <= static_cast<jint>(MediaDrmCreateError::MAX_VALUE));

  last_create_error_ = static_cast<MediaDrmCreateError>(j_error_code);
}

//------------------------------------------------------------------------------
// The following are private methods.

MediaDrmBridge::MediaDrmBridge(
    const std::vector<uint8_t>& scheme_uuid,
    const std::string& origin_id,
    SecurityLevel security_level,
    const std::string& message,
    bool requires_media_crypto,
    std::unique_ptr<MediaDrmStorageBridge> storage,
    const CreateFetcherCB& create_fetcher_cb,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : scheme_uuid_(scheme_uuid),
      storage_(std::move(storage)),
      create_fetcher_cb_(create_fetcher_cb),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      media_crypto_context_(this) {
  DVLOG(1) << __func__;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jbyteArray> j_scheme_uuid =
      base::android::ToJavaByteArray(env, &scheme_uuid[0], scheme_uuid.size());

  std::string security_level_str = GetSecurityLevelString(security_level);
  ScopedJavaLocalRef<jstring> j_security_level =
      ConvertUTF8ToJavaString(env, security_level_str);

  // origin id can be empty when MediaDrmBridge is created by
  // CreateWithoutSessionSupport, which is used for unprovisioning, or for
  // some key systems (like Clear Key) that don't support origin isolated
  // storage.
  ScopedJavaLocalRef<jstring> j_security_origin =
      ConvertUTF8ToJavaString(env, origin_id);
  ScopedJavaLocalRef<jstring> j_message = ConvertUTF8ToJavaString(env, message);

  j_media_drm_.Reset(Java_MediaDrmBridge_create(
      env, j_scheme_uuid, j_security_origin, j_security_level, j_message,
      requires_media_crypto, reinterpret_cast<intptr_t>(this),
      reinterpret_cast<intptr_t>(storage_.get())));
}

MediaDrmBridge::~MediaDrmBridge() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  JNIEnv* env = AttachCurrentThread();

  // After the call to Java_MediaDrmBridge_destroy() Java won't call native
  // methods anymore, this is ensured by MediaDrmBridge.java.
  if (j_media_drm_)
    Java_MediaDrmBridge_destroy(env, j_media_drm_);

  if (media_crypto_ready_cb_) {
    std::move(media_crypto_ready_cb_).Run(CreateJavaObjectPtr(nullptr), false);
  }

  // Rejects all pending promises.
  cdm_promise_adapter_.Clear(CdmPromiseAdapter::ClearReason::kDestruction);
}

MediaDrmBridge::SecurityLevel MediaDrmBridge::GetSecurityLevel() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_security_level =
      Java_MediaDrmBridge_getSecurityLevel(env, j_media_drm_);
  std::string security_level_str =
      ConvertJavaStringToUTF8(env, j_security_level.obj());
  return GetSecurityLevelFromString(security_level_str);
}

std::string MediaDrmBridge::GetVersionInternal() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_version =
      Java_MediaDrmBridge_getVersion(env, j_media_drm_);
  return ConvertJavaStringToUTF8(env, j_version.obj());
}

HdcpVersion MediaDrmBridge::GetCurrentHdcpLevel() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_current_hdcp_level =
      Java_MediaDrmBridge_getCurrentHdcpLevel(env, j_media_drm_);
  std::string current_hdcp_level_str =
      ConvertJavaStringToUTF8(env, j_current_hdcp_level.obj());
  return ToEmeHdcpVersion(current_hdcp_level_str);
}

void MediaDrmBridge::NotifyMediaCryptoReady(JavaObjectPtr j_media_crypto) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(j_media_crypto);
  DCHECK(!j_media_crypto_);

  j_media_crypto_ = std::move(j_media_crypto);

  UMA_HISTOGRAM_BOOLEAN("Media.EME.MediaCryptoAvailable",
                        !j_media_crypto_->is_null());

  if (!media_crypto_ready_cb_)
    return;

  // We have to use scoped_ptr to pass ScopedJavaGlobalRef with a callback.
  std::move(media_crypto_ready_cb_)
      .Run(CreateJavaObjectPtr(j_media_crypto_->obj()),
           IsSecureCodecRequired());
}

void MediaDrmBridge::SendProvisioningRequest(const GURL& default_url,
                                             const std::string& request_data) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  DCHECK(!provision_fetcher_) << "At most one provision request at any time.";

  provision_fetcher_ = create_fetcher_cb_.Run();
  provision_fetcher_->Retrieve(
      default_url, request_data,
      base::BindOnce(&MediaDrmBridge::ProcessProvisionResponse,
                     weak_factory_.GetWeakPtr()));
}

void MediaDrmBridge::ProcessProvisionResponse(bool success,
                                              const std::string& response) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  DCHECK(provision_fetcher_) << "No provision request pending.";
  provision_fetcher_.reset();

  if (!success)
    VLOG(1) << "Device provision failure: can't get server response";

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> j_response = ToJavaByteArray(env, response);

  Java_MediaDrmBridge_processProvisionResponse(env, j_media_drm_, success,
                                               j_response);
}

void MediaDrmBridge::OnHasAdditionalUsableKey() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  event_callbacks_.Notify(Event::kHasAdditionalUsableKey);
}

}  // namespace media
