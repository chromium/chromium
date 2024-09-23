// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/browser/speech/tts_environment_android_impl.h"
#include "content/common/buildflags.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/TtsPlatformImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace content {

TtsPlatformImplAndroid::TtsPlatformImplAndroid() : utterance_id_(0) {
  environment_android_ =
      GetContentClient()->browser()->CreateTtsEnvironmentAndroid();
  if (!environment_android_)
    environment_android_ = std::make_unique<TtsEnvironmentAndroidImpl>();
  TtsControllerImpl::GetInstance()->SetStopSpeakingWhenHidden(
      !environment_android_->CanSpeakUtterancesFromHiddenWebContents());
  environment_android_->SetCanSpeakNowChangedCallback(base::BindRepeating(
      &TtsPlatformImplAndroid::OnCanSpeakNowChanged, base::Unretained(this)));
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(
      Java_TtsPlatformImpl_create(env, reinterpret_cast<intptr_t>(this)));
}

TtsPlatformImplAndroid::~TtsPlatformImplAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_TtsPlatformImpl_destroy(env, java_ref_);
}

bool TtsPlatformImplAndroid::PlatformImplSupported() {
  return true;
}

bool TtsPlatformImplAndroid::PlatformImplInitialized() {
  return true;
}

void TtsPlatformImplAndroid::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> did_start_speaking_callback) {
  // Parse SSML and process speech.
  TtsController::GetInstance()->StripSSML(
      utterance,
      base::BindOnce(&TtsPlatformImplAndroid::ProcessSpeech,
                     weak_factory_.GetWeakPtr(), utterance_id, lang, voice,
                     params, std::move(did_start_speaking_callback)));
}

void TtsPlatformImplAndroid::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> did_start_speaking_callback,
    const std::string& parsed_utterance) {
  std::move(did_start_speaking_callback)
      .Run(StartSpeakingNow(utterance_id, lang, params, parsed_utterance,
                            voice.engine_id));
}

bool TtsPlatformImplAndroid::StartSpeakingNow(
    int utterance_id,
    const std::string& lang,
    const UtteranceContinuousParameters& params,
    const std::string& parsed_utterance,
    const std::string& engine_id) {
  if (!environment_android_->CanSpeakNow())
    return false;

  JNIEnv* env = AttachCurrentThread();
  const bool did_start = Java_TtsPlatformImpl_speak(
      env, java_ref_, utterance_id,
      base::android::ConvertUTF8ToJavaString(env, parsed_utterance),
      base::android::ConvertUTF8ToJavaString(env, lang),
      base::android::ConvertUTF8ToJavaString(env, engine_id), params.rate,
      params.pitch, params.volume);
  if (!did_start)
    return false;

  utterance_ = parsed_utterance;
  utterance_id_ = utterance_id;
  return true;
}

bool TtsPlatformImplAndroid::StopSpeaking() {
  JNIEnv* env = AttachCurrentThread();
  Java_TtsPlatformImpl_stop(env, java_ref_);
  utterance_id_ = 0;
  utterance_.clear();
  return true;
}

void TtsPlatformImplAndroid::Pause() {
  StopSpeaking();
}

void TtsPlatformImplAndroid::Resume() {}

bool TtsPlatformImplAndroid::IsSpeaking() {
  return (utterance_id_ != 0);
}

void TtsPlatformImplAndroid::GetVoices(std::vector<VoiceData>* out_voices) {
  JNIEnv* env = AttachCurrentThread();
  if (!Java_TtsPlatformImpl_isInitialized(env, java_ref_))
    return;

  int count = Java_TtsPlatformImpl_getVoiceCount(env, java_ref_);
  for (int i = 0; i < count; ++i) {
    out_voices->push_back(VoiceData());
    VoiceData& data = out_voices->back();
    data.native = true;
    data.name = base::android::ConvertJavaStringToUTF8(
        Java_TtsPlatformImpl_getVoiceName(env, java_ref_, i));
    data.lang = base::android::ConvertJavaStringToUTF8(
        Java_TtsPlatformImpl_getVoiceLanguage(env, java_ref_, i));
    data.events.insert(TTS_EVENT_START);
    data.events.insert(TTS_EVENT_END);
    data.events.insert(TTS_EVENT_ERROR);
  }
}

void TtsPlatformImplAndroid::VoicesChanged(JNIEnv* env) {
  TtsController::GetInstance()->VoicesChanged();
}

void TtsPlatformImplAndroid::OnEndEvent(JNIEnv* env,
                                        jint utterance_id) {
  SendFinalTtsEvent(utterance_id, TTS_EVENT_END,
                    static_cast<int>(utterance_.size()));
}

void TtsPlatformImplAndroid::OnErrorEvent(JNIEnv* env,
                                          jint utterance_id) {
  SendFinalTtsEvent(utterance_id, TTS_EVENT_ERROR, 0);
}

void TtsPlatformImplAndroid::OnStartEvent(JNIEnv* env,
                                          jint utterance_id) {
  if (utterance_id != utterance_id_)
    return;

  TtsController::GetInstance()->OnTtsEvent(utterance_id_, TTS_EVENT_START, 0,
                                           utterance_.size(), std::string());
}

void TtsPlatformImplAndroid::SendFinalTtsEvent(int utterance_id,
                                               TtsEventType event_type,
                                               int char_index) {
  if (utterance_id != utterance_id_)
    return;

  TtsController::GetInstance()->OnTtsEvent(utterance_id_, event_type,
                                           char_index, -1, std::string());
  utterance_id_ = 0;
  utterance_.clear();
}

// static
TtsPlatformImplAndroid* TtsPlatformImplAndroid::GetInstance() {
  return base::Singleton<
      TtsPlatformImplAndroid,
      base::LeakySingletonTraits<TtsPlatformImplAndroid>>::get();
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplAndroid::GetInstance();
}

void TtsPlatformImplAndroid::OnCanSpeakNowChanged() {
  if (!environment_android_->CanSpeakNow())
    TtsController::GetInstance()->Stop();
}

}  // namespace content
