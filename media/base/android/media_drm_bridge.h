// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "media/base/android/android_util.h"
#include "media/base/android/media_crypto_context.h"
#include "media/base/android/media_crypto_context_impl.h"
#include "media/base/android/media_drm_storage.h"
#include "media/base/android/media_drm_storage_bridge.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_promise.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"
#include "media/base/player_tracker.h"
#include "media/base/provision_fetcher.h"
#include "media/cdm/player_tracker_impl.h"
#include "url/origin.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Implements a CDM using Android MediaDrm API.
//
// Thread Safety:
//
// This class lives on the thread where it is created. All methods must be
// called on the |task_runner_| except for the PlayerTracker methods and
// SetMediaCryptoReadyCB(), which can be called on any thread.

class MEDIA_EXPORT MediaDrmBridge : public ContentDecryptionModule,
                                    public CdmContext,
                                    public PlayerTracker {
 public:
  // TODO(ddorwin): These are specific to Widevine. http://crbug.com/459400
  enum SecurityLevel {
    SECURITY_LEVEL_DEFAULT = 0,
    SECURITY_LEVEL_1 = 1,
    SECURITY_LEVEL_3 = 3,
  };

  using MediaCryptoReadyCB = MediaCryptoContext::MediaCryptoReadyCB;

  // Checks whether MediaDRM is available and usable, including for decoding.
  // All other static methods check IsAvailable() or equivalent internally.
  // There is no need to check IsAvailable() explicitly before calling them.
  static bool IsAvailable();

  // Checks whether |key_system| is supported.
  static bool IsKeySystemSupported(const std::string& key_system);

  // Checks whether |key_system| is supported with |container_mime_type|.
  // |container_mime_type| must not be empty.
  static bool IsKeySystemSupportedWithType(
      const std::string& key_system,
      const std::string& container_mime_type);

  // Whether per-origin provisioning (setting "origin" property on MediaDrm) is
  // supported or not. If false, per-device provisioning is used.
  static bool IsPerOriginProvisioningSupported();

  // Returns true if this device supports per-application provisioning, false
  // otherwise.
  static bool IsPerApplicationProvisioningSupported();

  static bool IsPersistentLicenseTypeSupported(const std::string& key_system);

  // Returns the list of the platform-supported key system names that
  // are not handled by Chrome explicitly.
  static std::vector<std::string> GetPlatformKeySystemNames();

  // Returns the scheme UUID for |key_system|.
  static std::vector<uint8_t> GetUUID(const std::string& key_system);

  // Same as Create() except that no session callbacks are provided. This is
  // used when we need to use MediaDrmBridge without creating any sessions.
  //
  // |create_fetcher_cb| can be empty when we don't want origin provision
  // to happen, e.g. when unprovision the origin.
  static scoped_refptr<MediaDrmBridge> CreateWithoutSessionSupport(
      const std::string& key_system,
      const std::string& origin_id,
      SecurityLevel security_level,
      const CreateFetcherCB& create_fetcher_cb);

  // ContentDecryptionModule implementation.
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void LoadSession(
      CdmSessionType session_type,
      const std::string& session_id,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  CdmContext* GetCdmContext() override;
  void DeleteOnCorrectThread() const override;

  // CdmContext implementation.
  MediaCryptoContext* GetMediaCryptoContext() override;

  // Provision the origin bound with |this|. |provisioning_complete_cb| will be
  // called asynchronously to indicate whether this was successful or not.
  // MediaDrmBridge must be created with a valid origin ID.
  void Provision(base::OnceCallback<void(bool)> provisioning_complete_cb);

  // Unprovision the origin bound with |this|. This will remove the cert for
  // current origin and leave the offline licenses in invalid state (offline
  // licenses can't be used anymore).
  //
  // MediaDrmBridge must be created with a valid origin ID without session
  // support. This function won't touch persistent storage.
  void Unprovision();

  // PlayerTracker implementation. Can be called on any thread.
  // The registered callbacks will be fired on |task_runner_|. The caller
  // should make sure that the callbacks are posted to the correct thread.
  //
  // Note: RegisterPlayer() should be called before SetMediaCryptoReadyCB() to
  // avoid missing any new key notifications.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) override;
  void UnregisterPlayer(int registration_id) override;

  // Helper function to determine whether a secure decoder is required for the
  // video playback.
  bool IsSecureCodecRequired();

  // Helper functions to resolve promises.
  void ResolvePromise(uint32_t promise_id);
  void ResolvePromiseWithSession(uint32_t promise_id,
                                 const std::string& session_id);
  void RejectPromise(uint32_t promise_id, const std::string& error_message);

  // Registers a callback which will be called when MediaCrypto is ready.
  // Can be called on any thread. Only one callback should be registered.
  // The registered callbacks will be fired on |task_runner_|. The caller
  // should make sure that the callbacks are posted to the correct thread.
  // TODO(xhwang): Move this up to be close to RegisterPlayer().
  void SetMediaCryptoReadyCB(MediaCryptoReadyCB media_crypto_ready_cb);

  // All the OnXxx functions below are called from Java. The implementation must
  // only do minimal work and then post tasks to avoid reentrancy issues.

  // Called by Java after a MediaCrypto object is created.
  void OnMediaCryptoReady(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      const base::android::JavaParamRef<jobject>& j_media_crypto);

  // Called by Java when we need to send a provisioning request,
  void OnProvisionRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      const base::android::JavaParamRef<jstring>& j_default_url,
      const base::android::JavaParamRef<jbyteArray>& j_request_data);

  // Called by Java when provisioning is complete. This is only in response to a
  // provision() request.
  void OnProvisioningComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      bool success);

  // Callbacks to resolve the promise for |promise_id|.
  void OnPromiseResolved(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      jint j_promise_id);
  void OnPromiseResolvedWithSession(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      jint j_promise_id,
      const base::android::JavaParamRef<jbyteArray>& j_session_id);

  // Callback to reject the promise for |promise_id| with |error_message|.
  // Note: No |system_error| is available from MediaDrm.
  // TODO(xhwang): Implement Exception code.
  void OnPromiseRejected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      jint j_promise_id,
      const base::android::JavaParamRef<jstring>& j_error_message);

  // Session event callbacks.

  void OnSessionMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      const base::android::JavaParamRef<jbyteArray>& j_session_id,
      jint j_message_type,
      const base::android::JavaParamRef<jbyteArray>& j_message);
  void OnSessionClosed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      const base::android::JavaParamRef<jbyteArray>& j_session_id);

  // Called when key statuses of session are changed. |is_key_release| is set to
  // true when releasing keys. Some of the MediaDrm key status codes should be
  // mapped to CDM key status differently (e.g. EXPIRE -> RELEASED).
  void OnSessionKeysChange(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      const base::android::JavaParamRef<jbyteArray>& j_session_id,
      // List<KeyStatus>
      const base::android::JavaParamRef<jobjectArray>& j_keys_info,
      bool has_additional_usable_key,
      bool is_key_release);

  // |expiry_time_ms| is the new expiration time for the keys in the session.
  // The time is in milliseconds, relative to the Unix epoch. A time of 0
  // indicates that the keys never expire.
  void OnSessionExpirationUpdate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_media_drm,
      const base::android::JavaParamRef<jbyteArray>& j_session_id,
      jlong expiry_time_ms);

 private:
  friend class MediaDrmBridgeFactory;
  // For DeleteSoon() in DeleteOnCorrectThread().
  friend class base::DeleteHelper<MediaDrmBridge>;

  static scoped_refptr<MediaDrmBridge> CreateInternal(
      const std::vector<uint8_t>& scheme_uuid,
      const std::string& origin_id,
      SecurityLevel security_level,
      bool requires_media_crypto,
      std::unique_ptr<MediaDrmStorageBridge> storage,
      const CreateFetcherCB& create_fetcher_cb,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);

  // Constructs a MediaDrmBridge for |scheme_uuid| and |security_level|. The
  // default security level will be used if |security_level| is
  // SECURITY_LEVEL_DEFAULT.
  //
  // |origin_id| is a random string that can identify an origin.
  //
  // If |requires_media_crypto| is true, MediaCrypto is expected to be created
  // and notified via MediaCryptoReadyCB set in SetMediaCryptoReadyCB(). This
  // may trigger the provisioning process. Before MediaCrypto is notified, no
  // other methods should be called.
  // TODO(xhwang): It's odd to rely on MediaCryptoReadyCB. Maybe we should add a
  // dedicated Initialize() method.
  //
  // If |requires_media_crypto| is false, MediaCrypto will not be created. This
  // object cannot be used for playback, but can be used to unprovision the
  // device/origin via Unprovision(). Sessions are not created in this mode.
  MediaDrmBridge(const std::vector<uint8_t>& scheme_uuid,
                 const std::string& origin_id,
                 SecurityLevel security_level,
                 bool requires_media_crypto,
                 std::unique_ptr<MediaDrmStorageBridge> storage,
                 const CreateFetcherCB& create_fetcher_cb,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb);

  ~MediaDrmBridge() override;

  // Get the security level of the media.
  SecurityLevel GetSecurityLevel();

  // A helper method that is called when MediaCrypto is ready.
  void NotifyMediaCryptoReady(JavaObjectPtr j_media_crypto);

  // Sends HTTP provisioning request to a provisioning server.
  void SendProvisioningRequest(const std::string& default_url,
                               const std::string& request_data);

  // Process the data received by provisioning server.
  void ProcessProvisionResponse(bool success, const std::string& response);

  // Called on the |task_runner_| when there is additional usable key.
  void OnHasAdditionalUsableKey();

  // UUID of the key system.
  std::vector<uint8_t> scheme_uuid_;

  // Persistent storage for session ID map.
  std::unique_ptr<MediaDrmStorageBridge> storage_;

  // Java MediaDrm instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_drm_;

  // Java MediaCrypto instance. Possible values are:
  // !j_media_crypto_:
  //   MediaCrypto creation has not been notified via NotifyMediaCryptoReady().
  // !j_media_crypto_->is_null():
  //   MediaCrypto creation succeeded and it has been notified.
  // j_media_crypto_->is_null():
  //   MediaCrypto creation failed and it has been notified.
  JavaObjectPtr j_media_crypto_;

  // The callback to create a ProvisionFetcher.
  CreateFetcherCB create_fetcher_cb_;

  // The ProvisionFetcher that requests and receives provisioning data.
  // Non-null iff when a provision request is pending.
  std::unique_ptr<ProvisionFetcher> provision_fetcher_;

  // The callback to be called when provisioning is complete.
  base::OnceCallback<void(bool)> provisioning_complete_cb_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  MediaCryptoReadyCB media_crypto_ready_cb_;

  PlayerTrackerImpl player_tracker_;

  CdmPromiseAdapter cdm_promise_adapter_;

  // Default task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  MediaCryptoContextImpl media_crypto_context_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaDrmBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_H_
