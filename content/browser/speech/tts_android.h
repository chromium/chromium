// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_ANDROID_H_
#define CONTENT_BROWSER_SPEECH_TTS_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "content/browser/speech/tts_platform_impl.h"

namespace content {

class TtsEnvironmentAndroid;

class TtsPlatformImplAndroid : public TtsPlatformImpl {
 public:
  // TtsPlatform overrides.
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
  void Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params,
      base::OnceCallback<void(bool)> did_start_speaking_callback) override;
  bool StopSpeaking() override;
  void Pause() override;
  void Resume() override;
  bool IsSpeaking() override;
  void GetVoices(std::vector<VoiceData>* out_voices) override;

  // Methods called from Java via JNI.
  void VoicesChanged(JNIEnv* env);
  void OnEndEvent(JNIEnv* env, jint utterance_id);
  void OnErrorEvent(JNIEnv* env, jint utterance_id);
  void OnStartEvent(JNIEnv* env, jint utterance_id);

  // Static functions.
  static TtsPlatformImplAndroid* GetInstance();

  TtsPlatformImplAndroid(const TtsPlatformImplAndroid&) = delete;
  TtsPlatformImplAndroid& operator=(const TtsPlatformImplAndroid&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<TtsPlatformImplAndroid>;

  TtsPlatformImplAndroid();
  ~TtsPlatformImplAndroid() override;

  void SendFinalTtsEvent(int utterance_id,
                         TtsEventType event_type,
                         int char_index);

  // Called once TtsController has stripped ssml.
  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const VoiceData& voice,
                     const UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> did_start_speaking_callback,
                     const std::string& parsed_utterance);

  // Starts speaking the utterance now. Returns true if speech started, false
  // if it is not possible to speak now.
  bool StartSpeakingNow(int utterance_id,
                        const std::string& lang,
                        const UtteranceContinuousParameters& params,
                        const std::string& parsed_utterance,
                        const std::string& engine_id);

  // Called when TtsEnvironmentAndroid::CanSpeakNow() may have changed.
  void OnCanSpeakNowChanged();

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  int utterance_id_;
  std::string utterance_;
  std::unique_ptr<TtsEnvironmentAndroid> environment_android_;

  base::WeakPtrFactory<TtsPlatformImplAndroid> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_ANDROID_H_
