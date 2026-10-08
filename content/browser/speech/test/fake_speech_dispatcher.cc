// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/test/fake_speech_dispatcher.h"

#include <stdlib.h>
#include <string.h>

#include <string_view>

#include "third_party/speech-dispatcher/libspeechd.h"

#define SPEECHD_TEST_EXPORT __attribute__((visibility("default")))

namespace {

SPDConnection connection;
char voice_name[] = "Test voice";
char voice_language[] = "en-US";

}  // namespace

extern "C" {

SPEECHD_TEST_EXPORT FakeSpeechDispatcherState speechd_test_state;

SPEECHD_TEST_EXPORT SPDConnection* spd_open(const char* client_name,
                                            const char* connection_name,
                                            const char* user_name,
                                            SPDConnectionMode mode) {
  ++speechd_test_state.open_calls;
  connection.mode = mode;
  return &connection;
}

SPEECHD_TEST_EXPORT void spd_close(SPDConnection* conn) {
  ++speechd_test_state.close_calls;
}

SPEECHD_TEST_EXPORT char** spd_list_modules(SPDConnection* conn) {
  ++speechd_test_state.list_modules_calls;
  static char* modules[2];
  modules[0] = strdup("test-module");
  return modules;
}

SPEECHD_TEST_EXPORT SPDVoice** spd_list_synthesis_voices(SPDConnection* conn) {
  ++speechd_test_state.list_voices_calls;
  static SPDVoice* voices[2];
  // The Linux backend frees the voice structs but not their strings or array.
  voices[0] = static_cast<SPDVoice*>(malloc(sizeof(SPDVoice)));
  if (!voices[0]) {
    return nullptr;
  }
  *voices[0] = {voice_name, voice_language, nullptr};
  return voices;
}

SPEECHD_TEST_EXPORT int spd_set_notification_on(SPDConnection* conn,
                                                SPDNotification notification) {
  speechd_test_state.notifications.fetch_or(notification);
  return 0;
}

SPEECHD_TEST_EXPORT int spd_set_output_module(SPDConnection* conn,
                                              const char* module) {
  ++speechd_test_state.set_output_module_calls;
  return std::string_view(module) == "test-module" ? 0 : -1;
}

SPEECHD_TEST_EXPORT int spd_set_synthesis_voice(SPDConnection* conn,
                                                const char* voice) {
  ++speechd_test_state.set_synthesis_voice_calls;
  return std::string_view(voice) == voice_name ? 0 : -1;
}

SPEECHD_TEST_EXPORT int spd_set_voice_rate(SPDConnection* conn, int rate) {
  ++speechd_test_state.set_rate_calls;
  return 0;
}

SPEECHD_TEST_EXPORT int spd_set_voice_pitch(SPDConnection* conn, int pitch) {
  ++speechd_test_state.set_pitch_calls;
  return 0;
}

SPEECHD_TEST_EXPORT int spd_set_language(SPDConnection* conn,
                                         const char* language) {
  ++speechd_test_state.set_language_calls;
  return std::string_view(language) == voice_language ? 0 : -1;
}

SPEECHD_TEST_EXPORT int spd_say(SPDConnection* conn,
                                SPDPriority priority,
                                const char* text) {
  const int message_id = ++speechd_test_state.message_id;
  ++speechd_test_state.say_calls;
  // Linux TTS queues these notifications until after spd_say returns.
  conn->callback_begin(message_id, 0, SPD_EVENT_BEGIN);
  if (speechd_test_state.finish_on_say.load()) {
    conn->callback_end(message_id, 0, SPD_EVENT_END);
  }
  return message_id;
}

SPEECHD_TEST_EXPORT int spd_pause(SPDConnection* conn) {
  ++speechd_test_state.pause_calls;
  conn->callback_pause(speechd_test_state.message_id.load(), 0,
                       SPD_EVENT_PAUSE);
  return 0;
}

SPEECHD_TEST_EXPORT int spd_resume(SPDConnection* conn) {
  ++speechd_test_state.resume_calls;
  conn->callback_resume(speechd_test_state.message_id.load(), 0,
                        SPD_EVENT_RESUME);
  return 0;
}

SPEECHD_TEST_EXPORT int spd_stop(SPDConnection* conn) {
  ++speechd_test_state.stop_calls;
  conn->callback_cancel(speechd_test_state.message_id.load(), 0,
                        SPD_EVENT_CANCEL);
  return 0;
}

}  // extern "C"
