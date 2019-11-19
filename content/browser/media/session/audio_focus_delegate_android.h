// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "content/browser/media/session/audio_focus_delegate.h"

namespace media_session {
namespace mojom {
enum class AudioFocusType;
}  // namespace mojom
}  // namespace media_session

namespace content {

// AudioFocusDelegateAndroid handles the audio focus at a system level on
// Android. It is also proxying the JNI calls.
class AudioFocusDelegateAndroid : public AudioFocusDelegate {
 public:
  explicit AudioFocusDelegateAndroid(MediaSessionImpl* media_session);
  ~AudioFocusDelegateAndroid() override;

  void Initialize();

  AudioFocusResult RequestAudioFocus(
      media_session::mojom::AudioFocusType audio_focus_type) override;
  void AbandonAudioFocus() override;
  base::Optional<media_session::mojom::AudioFocusType> GetCurrentFocusType()
      const override;
  const base::UnguessableToken& request_id() const override;

  // Called when the Android system requests the MediaSession to be suspended.
  // Called by Java through JNI.
  void OnSuspend(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called when the Android system requests the MediaSession to be resumed.
  // Called by Java through JNI.
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called when the Android system requests the MediaSession to start ducking.
  // Called by Java through JNI.
  void OnStartDucking(JNIEnv* env, jobject obj);

  // Called when the Android system requests the MediaSession to stop ducking.
  // Called by Java through JNI.
  void OnStopDucking(JNIEnv* env, jobject obj);

  // Record when the Android system requests the MediaSession to duck.
  // Called by Java through JNI.
  void RecordSessionDuck(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  // This is not used by this delegate.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr) override {}

 private:
  // Weak pointer because |this| is owned by |media_session_|.
  MediaSessionImpl* media_session_;
  base::android::ScopedJavaGlobalRef<jobject> j_media_session_delegate_;
  DISALLOW_COPY_AND_ASSIGN(AudioFocusDelegateAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_ANDROID_H_
