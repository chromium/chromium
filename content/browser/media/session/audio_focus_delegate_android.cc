// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/unguessable_token.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/public/android/content_jni_headers/AudioFocusDelegate_jni.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

using base::android::JavaParamRef;

namespace content {

AudioFocusDelegateAndroid::AudioFocusDelegateAndroid(
    MediaSessionImpl* media_session)
    : media_session_(media_session) {}

AudioFocusDelegateAndroid::~AudioFocusDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  Java_AudioFocusDelegate_tearDown(env, j_media_session_delegate_);
}

void AudioFocusDelegateAndroid::Initialize() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  j_media_session_delegate_.Reset(
      Java_AudioFocusDelegate_create(env, reinterpret_cast<intptr_t>(this)));
}

AudioFocusDelegate::AudioFocusResult
AudioFocusDelegateAndroid::RequestAudioFocus(
    media_session::mojom::AudioFocusType audio_focus_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  bool success = Java_AudioFocusDelegate_requestAudioFocus(
      env, j_media_session_delegate_,
      audio_focus_type ==
          media_session::mojom::AudioFocusType::kGainTransientMayDuck);
  return success ? AudioFocusDelegate::AudioFocusResult::kSuccess
                 : AudioFocusDelegate::AudioFocusResult::kFailed;
}

void AudioFocusDelegateAndroid::AbandonAudioFocus() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  Java_AudioFocusDelegate_abandonAudioFocus(env, j_media_session_delegate_);
}

base::Optional<media_session::mojom::AudioFocusType>
AudioFocusDelegateAndroid::GetCurrentFocusType() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  return Java_AudioFocusDelegate_isFocusTransient(env,
                                                  j_media_session_delegate_)
             ? media_session::mojom::AudioFocusType::kGainTransientMayDuck
             : media_session::mojom::AudioFocusType::kGain;
}

const base::UnguessableToken& AudioFocusDelegateAndroid::request_id() const {
  return base::UnguessableToken::Null();
}

void AudioFocusDelegateAndroid::OnSuspend(JNIEnv*,
                                          const JavaParamRef<jobject>&) {
  if (!media_session_->IsActive() ||
      !base::FeatureList::IsEnabled(media::kAudioFocusLossSuspendMediaSession))
    return;

  media_session_->Suspend(MediaSession::SuspendType::kSystem);
}

void AudioFocusDelegateAndroid::OnResume(JNIEnv*,
                                         const JavaParamRef<jobject>&) {
  if (!media_session_->IsSuspended())
    return;

  media_session_->Resume(MediaSession::SuspendType::kSystem);
}

void AudioFocusDelegateAndroid::OnStartDucking(JNIEnv*, jobject) {
  media_session_->StartDucking();
}

void AudioFocusDelegateAndroid::OnStopDucking(JNIEnv*, jobject) {
  media_session_->StopDucking();
}

void AudioFocusDelegateAndroid::RecordSessionDuck(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  media_session_->RecordSessionDuck();
}

// static
std::unique_ptr<AudioFocusDelegate> AudioFocusDelegate::Create(
    MediaSessionImpl* media_session) {
  AudioFocusDelegateAndroid* delegate =
      new AudioFocusDelegateAndroid(media_session);
  delegate->Initialize();
  return std::unique_ptr<AudioFocusDelegate>(delegate);
}

}  // namespace content
