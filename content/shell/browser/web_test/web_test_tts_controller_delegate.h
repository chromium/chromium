// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_TTS_CONTROLLER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_TTS_CONTROLLER_DELEGATE_H_

#include "base/memory/singleton.h"
#include "content/public/browser/tts_controller_delegate.h"

// Dummy implementation of TtsControllerDelegate for web tests.
// Currently does nothing interesting but could be extended to enable more
// detailed testing.
class WebTestTtsControllerDelegate : public content::TtsControllerDelegate {
 public:
  static WebTestTtsControllerDelegate* GetInstance();

  // content::TtsControllerDelegate overrides.
  int GetMatchingVoice(content::TtsUtterance* utterance,
                       std::vector<content::VoiceData>& voices) override;
  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override;
  void SetTtsEngineDelegate(content::TtsEngineDelegate* delegate) override;
  content::TtsEngineDelegate* GetTtsEngineDelegate() override;

 private:
  WebTestTtsControllerDelegate();
  ~WebTestTtsControllerDelegate() override;

  friend struct base::DefaultSingletonTraits<WebTestTtsControllerDelegate>;

  DISALLOW_COPY_AND_ASSIGN(WebTestTtsControllerDelegate);
};

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_TTS_CONTROLLER_DELEGATE_H_
