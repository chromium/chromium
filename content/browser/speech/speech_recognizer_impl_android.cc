// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/public/android/content_jni_headers/SpeechRecognitionImpl_jni.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaFloatArrayToFloatVector;
using base::android::JavaParamRef;

namespace content {

SpeechRecognizerImplAndroid::SpeechRecognizerImplAndroid(
    SpeechRecognitionEventListener* listener,
    int session_id)
    : SpeechRecognizer(listener, session_id),
      state_(STATE_IDLE) {
}

SpeechRecognizerImplAndroid::~SpeechRecognizerImplAndroid() { }

void SpeechRecognizerImplAndroid::StartRecognition(
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(xians): Open the correct device for speech on Android.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SpeechRecognitionEventListener::OnRecognitionStart,
                     base::Unretained(listener()), session_id()));
  SpeechRecognitionSessionConfig config =
      SpeechRecognitionManager::GetInstance()->GetSessionConfig(session_id());
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &content::SpeechRecognizerImplAndroid::StartRecognitionOnUIThread,
          this, config.language, config.continuous, config.interim_results));
}

void SpeechRecognizerImplAndroid::StartRecognitionOnUIThread(
    const std::string& language,
    bool continuous,
    bool interim_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  j_recognition_.Reset(Java_SpeechRecognitionImpl_createSpeechRecognition(
      env, reinterpret_cast<intptr_t>(this)));
  Java_SpeechRecognitionImpl_startRecognition(
      env, j_recognition_, ConvertUTF8ToJavaString(env, language), continuous,
      interim_results);
}

void SpeechRecognizerImplAndroid::AbortRecognition() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    state_ = STATE_IDLE;
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&content::SpeechRecognizerImplAndroid::AbortRecognition,
                       this));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  if (!j_recognition_.is_null())
    Java_SpeechRecognitionImpl_abortRecognition(env, j_recognition_);
}

void SpeechRecognizerImplAndroid::StopAudioCapture() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&content::SpeechRecognizerImplAndroid::StopAudioCapture,
                       this));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  if (!j_recognition_.is_null())
    Java_SpeechRecognitionImpl_stopRecognition(env, j_recognition_);
}

bool SpeechRecognizerImplAndroid::IsActive() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return state_ != STATE_IDLE;
}

bool SpeechRecognizerImplAndroid::IsCapturingAudio() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return state_ == STATE_CAPTURING_AUDIO;
}

void SpeechRecognizerImplAndroid::OnAudioStart(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&SpeechRecognizerImplAndroid::OnAudioStart,
                                  this, nullptr, nullptr));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state_ = STATE_CAPTURING_AUDIO;
  listener()->OnAudioStart(session_id());
}

void SpeechRecognizerImplAndroid::OnSoundStart(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&SpeechRecognizerImplAndroid::OnSoundStart,
                                  this, nullptr, nullptr));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  listener()->OnSoundStart(session_id());
}

void SpeechRecognizerImplAndroid::OnSoundEnd(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&SpeechRecognizerImplAndroid::OnSoundEnd,
                                  this, nullptr, nullptr));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  listener()->OnSoundEnd(session_id());
}

void SpeechRecognizerImplAndroid::OnAudioEnd(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&SpeechRecognizerImplAndroid::OnAudioEnd,
                                  this, nullptr, nullptr));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == STATE_CAPTURING_AUDIO)
    state_ = STATE_AWAITING_FINAL_RESULT;
  listener()->OnAudioEnd(session_id());
}

void SpeechRecognizerImplAndroid::OnRecognitionResults(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobjectArray>& strings,
    const JavaParamRef<jfloatArray>& floats,
    jboolean provisional) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<base::string16> options;
  AppendJavaStringArrayToStringVector(env, strings, &options);
  std::vector<float> scores(options.size(), 0.0);
  if (floats != NULL)
    JavaFloatArrayToFloatVector(env, floats, &scores);
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  CHECK_EQ(options.size(), scores.size());
  for (size_t i = 0; i < options.size(); ++i) {
    result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
        options[i], static_cast<double>(scores[i])));
  }
  result->is_provisional = provisional;
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &SpeechRecognizerImplAndroid::OnRecognitionResultsOnIOThread, this,
          std::move(results)));
}

void SpeechRecognizerImplAndroid::OnRecognitionResultsOnIOThread(
    std::vector<blink::mojom::SpeechRecognitionResultPtr> results) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  listener()->OnRecognitionResults(session_id(), results);
}

void SpeechRecognizerImplAndroid::OnRecognitionError(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint error) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&SpeechRecognizerImplAndroid::OnRecognitionError, this,
                       nullptr, nullptr, error));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  listener()->OnRecognitionError(
      session_id(),
      blink::mojom::SpeechRecognitionError(
          static_cast<blink::mojom::SpeechRecognitionErrorCode>(error),
          blink::mojom::SpeechAudioErrorDetails::kNone));
}

void SpeechRecognizerImplAndroid::OnRecognitionEnd(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&SpeechRecognizerImplAndroid::OnRecognitionEnd, this,
                       nullptr, nullptr));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state_ = STATE_IDLE;
  listener()->OnRecognitionEnd(session_id());
}

}  // namespace content
