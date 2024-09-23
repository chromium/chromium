// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_ANDROID_H_

#include <jni.h>
#include <memory>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

class MediaSessionImpl;

// This class is interlayer between native MediaSession and Java
// MediaSession. This class is owned by the native MediaSession and will
// teardown Java MediaSession when the native MediaSession is destroyed.
// Java MediaSessionObservers are also proxied via this class.
class MediaSessionAndroid final
    : public media_session::mojom::MediaSessionObserver {
 public:
  // Helper class for calling GetJavaObject() in a static method, in order to
  // avoid leaking the Java object outside.
  struct JavaObjectGetter;

  explicit MediaSessionAndroid(MediaSessionImpl* session);

  MediaSessionAndroid(const MediaSessionAndroid&) = delete;
  MediaSessionAndroid& operator=(const MediaSessionAndroid&) = delete;

  ~MediaSessionAndroid() override;

  // media_session::mojom::MediaSessionObserver implementation:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override;
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  // MediaSession method wrappers.
  void Resume(JNIEnv* env, const base::android::JavaParamRef<jobject>& j_obj);
  void Suspend(JNIEnv* env, const base::android::JavaParamRef<jobject>& j_obj);
  void Stop(JNIEnv* env, const base::android::JavaParamRef<jobject>& j_obj);
  void Seek(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& j_obj,
            const jlong millis);
  void SeekTo(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& j_obj,
              const jlong millis);
  void DidReceiveAction(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& j_obj,
                        jint action);
  void RequestSystemAudioFocus(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_obj);

 private:
  friend class WebContentsObserverProxy;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // The linked Java object. The strong reference is hold by Java WebContensImpl
  // to avoid introducing a new GC root.
  JavaObjectWeakGlobalRef j_media_session_;
  // WebContentsAndroid corresponding to the Java WebContentsImpl that holds a
  // strong reference to |j_media_session_|.
  raw_ptr<WebContentsAndroid, DanglingUntriaged> web_contents_android_;

  const raw_ptr<MediaSessionImpl, DanglingUntriaged> media_session_;

  bool is_paused_ = false;
  bool is_controllable_ = false;

  mojo::Receiver<media_session::mojom::MediaSessionObserver> observer_receiver_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_ANDROID_H_
