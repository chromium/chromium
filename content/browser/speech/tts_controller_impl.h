// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_TTS_CONTROLLER_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_controller_delegate.h"
#include "content/public/browser/tts_platform.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;

// Singleton class that manages text-to-speech for all TTS engines and
// APIs, maintaining a queue of pending utterances and keeping
// track of all state.
class CONTENT_EXPORT TtsControllerImpl : public TtsController {
 public:
  // Get the single instance of this class.
  static TtsControllerImpl* GetInstance();

  // TtsController methods
  bool IsSpeaking() override;
  void SpeakOrEnqueue(std::unique_ptr<TtsUtterance> utterance) override;
  void Stop() override;
  void Stop(const GURL& source_url) override;
  void Pause() override;
  void Resume() override;
  void OnTtsEvent(int utterance_id,
                  TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;
  void GetVoices(BrowserContext* browser_context,
                 std::vector<VoiceData>* out_voices) override;
  void VoicesChanged() override;
  void AddVoicesChangedDelegate(VoicesChangedDelegate* delegate) override;
  void RemoveVoicesChangedDelegate(VoicesChangedDelegate* delegate) override;
  void RemoveUtteranceEventDelegate(UtteranceEventDelegate* delegate) override;
  void SetTtsEngineDelegate(TtsEngineDelegate* delegate) override;
  TtsEngineDelegate* GetTtsEngineDelegate() override;

  // Testing methods
  void SetTtsPlatform(TtsPlatform* tts_platform) override;
  int QueueSize() override;

  // Strips SSML so that tags are not output by speech engine.
  void StripSSML(
      const std::string& utterance,
      base::OnceCallback<void(const std::string&)> callback) override;

 protected:
  TtsControllerImpl();
  ~TtsControllerImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest, TestTtsControllerShutdown);
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest, TestGetMatchingVoice);
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest,
                           TestTtsControllerUtteranceDefaults);

  friend struct base::DefaultSingletonTraits<TtsControllerImpl>;

  // Get the platform TTS implementation (or injected mock).
  TtsPlatform* GetTtsPlatform();

  // Start speaking the given utterance. Will either take ownership of
  // |utterance| or delete it if there's an error. Returns true on success.
  void SpeakNow(std::unique_ptr<TtsUtterance> utterance);

  // Clear the utterance queue. If send_events is true, will send
  // TTS_EVENT_CANCELLED events on each one.
  void ClearUtteranceQueue(bool send_events);

  // Finalize and delete the current utterance.
  void FinishCurrentUtterance();

  // Start speaking the next utterance in the queue.
  void SpeakNextUtterance();

  // Updates the utterance to have default values for rate, pitch, and
  // volume if they have not yet been set. On Chrome OS, defaults are
  // pulled from user prefs, and may not be the same as other platforms.
  void UpdateUtteranceDefaults(TtsUtterance* utterance);

  // Passed to Speak() as a callback.
  void OnSpeakFinished(int utterance_id, bool success);

  // Static helper methods for StripSSML.
  static void StripSSMLHelper(
      const std::string& utterance,
      base::OnceCallback<void(const std::string&)> on_ssml_parsed,
      data_decoder::DataDecoder::ValueOrError result);
  static void PopulateParsedText(std::string* parsed_text,
                                 const base::Value* element);

  TtsControllerDelegate* GetTtsControllerDelegate();

  TtsControllerDelegate* delegate_;

  // A set of delegates that want to be notified when the voices change.
  base::ObserverList<VoicesChangedDelegate> voices_changed_delegates_;

  // The current utterance being spoken.
  std::unique_ptr<TtsUtterance> current_utterance_;

  // Whether the queue is paused or not.
  bool paused_;

  // A pointer to the platform implementation of text-to-speech, for
  // dependency injection.
  TtsPlatform* tts_platform_;

  // A queue of utterances to speak after the current one finishes.
  base::queue<std::unique_ptr<TtsUtterance>> utterance_queue_;

  DISALLOW_COPY_AND_ASSIGN(TtsControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_CONTROLLER_IMPL_H_
