// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_ANDROID_H_
#define CONTENT_BROWSER_SPEECH_TTS_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/browser/speech/tts_platform_impl.h"

namespace content {

class TtsPlatformImplAndroid : public TtsPlatformImpl {
 public:
  // TtsPlatform overrides.
  bool PlatformImplAvailable() override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;
  bool StopSpeaking() override;
  void Pause() override;
  void Resume() override;
  bool IsSpeaking() override;
  void GetVoices(std::vector<VoiceData>* out_voices) override;

  // Methods called from Java via JNI.
  void RequestTtsStop(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void VoicesChanged(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void OnEndEvent(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jint utterance_id);
  void OnErrorEvent(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint utterance_id);
  void OnStartEvent(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint utterance_id);

  // Static functions.
  static TtsPlatformImplAndroid* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<TtsPlatformImplAndroid>;

  TtsPlatformImplAndroid();
  ~TtsPlatformImplAndroid() override;

  void SendFinalTtsEvent(int utterance_id,
                         TtsEventType event_type,
                         int char_index);

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const VoiceData& voice,
                     const UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  int utterance_id_;
  std::string utterance_;

  base::WeakPtrFactory<TtsPlatformImplAndroid> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_ANDROID_H_
