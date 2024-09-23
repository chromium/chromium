// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_listener.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/android/media_player_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/MediaPlayerListener_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {

MediaPlayerListener::MediaPlayerListener(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<MediaPlayerBridge> media_player)
    : task_runner_(task_runner), media_player_(media_player) {
  DCHECK(task_runner_.get());
  DCHECK(media_player_);
}

MediaPlayerListener::~MediaPlayerListener() {}

void MediaPlayerListener::CreateMediaPlayerListener(
    const JavaRef<jobject>& media_player) {
  JNIEnv* env = AttachCurrentThread();
  if (j_media_player_listener_.is_null()) {
    j_media_player_listener_.Reset(Java_MediaPlayerListener_create(
        env, reinterpret_cast<intptr_t>(this), media_player));
  }
}


void MediaPlayerListener::ReleaseMediaPlayerListenerResources() {
  j_media_player_listener_.Reset();
}

void MediaPlayerListener::OnMediaError(JNIEnv* /* env */,
                                       const JavaParamRef<jobject>& /* obj */,
                                       jint error_type) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaPlayerBridge::OnMediaError, media_player_,
                                error_type));
}

void MediaPlayerListener::OnVideoSizeChanged(
    JNIEnv* /* env */,
    const JavaParamRef<jobject>& /* obj */,
    jint width,
    jint height) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&MediaPlayerBridge::OnVideoSizeChanged,
                                        media_player_, width, height));
}

void MediaPlayerListener::OnPlaybackComplete(
    JNIEnv* /* env */,
    const JavaParamRef<jobject>& /* obj */) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaPlayerBridge::OnPlaybackComplete, media_player_));
}

void MediaPlayerListener::OnMediaPrepared(
    JNIEnv* /* env */,
    const JavaParamRef<jobject>& /* obj */) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaPlayerBridge::OnMediaPrepared, media_player_));
}

}  // namespace media
