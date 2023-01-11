// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_STORAGE_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_STORAGE_BRIDGE_H_

#include <jni.h>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/android_util.h"
#include "media/base/media_drm_storage.h"
#include "url/origin.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// This class is the native version of MediaDrmStorageBridge in Java. It's used
// to talk to the concrete implementation for persistent data management.
class MediaDrmStorageBridge {
 public:
  using InitCB = base::OnceCallback<void(bool)>;

  MediaDrmStorageBridge();

  MediaDrmStorageBridge(const MediaDrmStorageBridge&) = delete;
  MediaDrmStorageBridge& operator=(const MediaDrmStorageBridge&) = delete;

  ~MediaDrmStorageBridge();

  // Once storage is initialized, |init_cb| will be called and it will have a
  // random generated origin id for later usage. If this function isn't called,
  // all the other functions will fail.
  void Initialize(const CreateStorageCB& create_storage_cb, InitCB init_cb);

  const std::string& origin_id() const { return origin_id_; }

  // The following OnXXX functions are called by Java. The functions will post
  // task on message loop immediately to avoid reentrancy issues.

  // Called by the java object when device provision is finished. Implementation
  // will record the time as provisioning time.
  void OnProvisioned(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& j_storage,
                     // Callback<Boolean>
                     const base::android::JavaParamRef<jobject>& j_callback);

  // Called by the java object to load session data into memory. |j_callback|
  // will return a null object if load fails.
  void OnLoadInfo(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& j_storage,
                  const base::android::JavaParamRef<jbyteArray>& j_session_id,
                  // Callback<PersistentInfo>
                  const base::android::JavaParamRef<jobject>& j_callback);

  // Called by the java object to persistent session data.
  void OnSaveInfo(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& j_storage,
                  // PersistentInfo
                  const base::android::JavaParamRef<jobject>& j_persist_info,
                  // Callback<Boolean>
                  const base::android::JavaParamRef<jobject>& j_callback);

  // Called by the java object to remove persistent session data.
  void OnClearInfo(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& j_storage,
                   const base::android::JavaParamRef<jbyteArray>& j_session_id,
                   // Callback<Boolean>
                   const base::android::JavaParamRef<jobject>& j_callback);

 private:
  void RunAndroidBoolCallback(JavaObjectPtr j_callback, bool success);
  void OnInitialized(InitCB init_cb,
                     bool success,
                     const MediaDrmStorage::MediaDrmOriginId& origin_id);
  void OnSessionDataLoaded(
      JavaObjectPtr j_callback,
      const std::string& session_id,
      std::unique_ptr<MediaDrmStorage::SessionData> session_data);

  std::unique_ptr<MediaDrmStorage> impl_;

  // Randomly generated ID for origin.
  std::string origin_id_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<MediaDrmStorageBridge> weak_factory_{this};
};

}  // namespace media
#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_STORAGE_BRIDGE_H_
