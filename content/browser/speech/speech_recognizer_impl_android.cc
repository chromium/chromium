// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/SpeechRecognitionImpl_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaFloatArrayToFloatVector;
using base::android::JavaRef;

namespace content {

class SpeechRecognizerImplAndroid::UiPeer {
 public:
  explicit UiPeer(base::WeakPtr<SpeechRecognizerImplAndroid> recognizer)
      : recognizer_(std::move(recognizer)) {}

  UiPeer(const UiPeer&) = delete;
  UiPeer& operator=(const UiPeer&) = delete;

  ~UiPeer() {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!j_recognition_.is_null()) {
      Java_SpeechRecognitionImpl_onNativeDestroyed(AttachCurrentThread(),
                                                   j_recognition_);
    }
  }

  void StartRecognition(const std::string& language,
                        bool continuous,
                        bool interim_results) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    JNIEnv* env = AttachCurrentThread();
    if (!j_recognition_.is_null()) {
      Java_SpeechRecognitionImpl_onNativeDestroyed(env, j_recognition_);
    }
    j_recognition_.Reset(Java_SpeechRecognitionImpl_createSpeechRecognition(
        env, jni_zero::MakeRaw(this)));
    Java_SpeechRecognitionImpl_startRecognition(
        env, j_recognition_, ConvertUTF8ToJavaString(env, language), continuous,
        interim_results);
  }

  void AbortRecognition() {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!j_recognition_.is_null()) {
      Java_SpeechRecognitionImpl_abortRecognition(AttachCurrentThread(),
                                                  j_recognition_);
    }
  }

  void StopAudioCapture() {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!j_recognition_.is_null()) {
      Java_SpeechRecognitionImpl_stopRecognition(AttachCurrentThread(),
                                                 j_recognition_);
      return;
    }
    OnRecognitionEnd(nullptr);
  }

  void OnAudioStart(JNIEnv* env) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnAudioStartOnIOThread,
                       recognizer_));
  }

  void OnSoundStart(JNIEnv* env) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnSoundStartOnIOThread,
                       recognizer_));
  }

  void OnSoundEnd(JNIEnv* env) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnSoundEndOnIOThread,
                       recognizer_));
  }

  void OnAudioEnd(JNIEnv* env) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnAudioEndOnIOThread,
                       recognizer_));
  }

  void OnRecognitionResults(JNIEnv* env,
                            const JavaRef<jobjectArray>& strings,
                            const JavaRef<jfloatArray>& floats,
                            bool provisional) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    std::vector<std::u16string> options;
    AppendJavaStringArrayToStringVector(env, strings, &options);
    std::vector<float> scores(options.size(), 0.0);
    if (!floats.is_null()) {
      JavaFloatArrayToFloatVector(env, floats, &scores);
    }
    std::vector<media::mojom::WebSpeechRecognitionResultPtr> results;
    results.push_back(media::mojom::WebSpeechRecognitionResult::New());
    media::mojom::WebSpeechRecognitionResultPtr& result = results.back();
    CHECK_EQ(options.size(), scores.size());
    for (size_t i = 0; i < options.size(); ++i) {
      result->hypotheses.push_back(
          media::mojom::SpeechRecognitionHypothesis::New(
              options[i], static_cast<double>(scores[i])));
    }
    result->is_provisional = provisional;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SpeechRecognizerImplAndroid::OnRecognitionResultsOnIOThread,
            recognizer_, std::move(results)));
  }

  void OnRecognitionError(JNIEnv* env, int32_t error) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SpeechRecognizerImplAndroid::OnRecognitionErrorOnIOThread,
            recognizer_, error));
  }

  void OnRecognitionEnd(JNIEnv* env) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnRecognitionEndOnIOThread,
                       recognizer_));
  }

 private:
  base::WeakPtr<SpeechRecognizerImplAndroid> recognizer_;
  base::android::ScopedJavaGlobalRef<jobject> j_recognition_;
};

SpeechRecognizerImplAndroid::SpeechRecognizerImplAndroid(
    SpeechRecognitionEventListener* listener,
    int session_id)
    : SpeechRecognizer(listener, session_id), state_(STATE_IDLE) {
  ui_peer_.reset(new UiPeer(io_weak_factory_.GetWeakPtr()));
}

SpeechRecognizerImplAndroid::~SpeechRecognizerImplAndroid() = default;

void SpeechRecognizerImplAndroid::StartRecognition(
    const std::string& device_id) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  // TODO(xians): Open the correct device for speech on Android.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionEventListener::OnRecognitionStart,
                     base::Unretained(listener()), session_id()));
  SpeechRecognitionSessionConfig config =
      SpeechRecognitionManager::GetInstance()->GetSessionConfig(session_id());
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UiPeer::StartRecognition,
                     base::Unretained(ui_peer_.get()), config.language,
                     config.continuous, config.interim_results));
}

void SpeechRecognizerImplAndroid::UpdateRecognitionContext(
    const media::SpeechRecognitionRecognitionContext& recognition_context) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::UpdateRecognitionContext,
                       this, recognition_context));
    return;
  }
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  listener()->OnRecognitionError(
      session_id(),
      media::mojom::SpeechRecognitionError(
          media::mojom::SpeechRecognitionErrorCode::kPhrasesNotSupported,
          media::mojom::SpeechAudioErrorDetails::kNone));
}

void SpeechRecognizerImplAndroid::AbortRecognition() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    state_ = STATE_IDLE;
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&UiPeer::AbortRecognition,
                                  base::Unretained(ui_peer_.get())));
    return;
  }
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  ui_peer_->AbortRecognition();
}

void SpeechRecognizerImplAndroid::StopAudioCapture() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    if (state_ == STATE_AWAITING_FINAL_RESULT || state_ == STATE_ENDED) {
      return;
    }
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&UiPeer::StopAudioCapture,
                                  base::Unretained(ui_peer_.get())));
    return;
  }
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  ui_peer_->StopAudioCapture();
}

bool SpeechRecognizerImplAndroid::IsActive() const {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  return state_ != STATE_IDLE && state_ != STATE_ENDED;
}

bool SpeechRecognizerImplAndroid::IsCapturingAudio() const {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  return state_ == STATE_CAPTURING_AUDIO;
}

void SpeechRecognizerImplAndroid::OnAudioStartOnIOThread() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  state_ = STATE_CAPTURING_AUDIO;
  listener()->OnAudioStart(session_id());
}

void SpeechRecognizerImplAndroid::OnSoundStartOnIOThread() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  listener()->OnSoundStart(session_id());
}

void SpeechRecognizerImplAndroid::OnSoundEndOnIOThread() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  listener()->OnSoundEnd(session_id());
}

void SpeechRecognizerImplAndroid::OnAudioEndOnIOThread() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  if (state_ == STATE_CAPTURING_AUDIO)
    state_ = STATE_AWAITING_FINAL_RESULT;
  listener()->OnAudioEnd(session_id());
}

void SpeechRecognizerImplAndroid::OnRecognitionResultsOnIOThread(
    std::vector<media::mojom::WebSpeechRecognitionResultPtr> results) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  listener()->OnRecognitionResults(session_id(), results);
}

void SpeechRecognizerImplAndroid::OnRecognitionErrorOnIOThread(int32_t error) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  listener()->OnRecognitionError(
      session_id(),
      media::mojom::SpeechRecognitionError(
          static_cast<media::mojom::SpeechRecognitionErrorCode>(error),
          media::mojom::SpeechAudioErrorDetails::kNone));
}

void SpeechRecognizerImplAndroid::OnRecognitionEndOnIOThread() {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  if (state_ == STATE_ENDED) {
    return;
  }
  state_ = STATE_ENDED;
  listener()->OnRecognitionEnd(session_id());
}

}  // namespace content

DEFINE_JNI(SpeechRecognitionImpl)
