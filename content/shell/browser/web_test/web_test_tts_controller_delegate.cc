// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_tts_controller_delegate.h"

// static
WebTestTtsControllerDelegate* WebTestTtsControllerDelegate::GetInstance() {
  return base::Singleton<WebTestTtsControllerDelegate>::get();
}

int WebTestTtsControllerDelegate::GetMatchingVoice(
    content::TtsUtterance* utterance,
    std::vector<content::VoiceData>& voices) {
  return -1;
}

void WebTestTtsControllerDelegate::UpdateUtteranceDefaultsFromPrefs(
    content::TtsUtterance* utterance,
    double* rate,
    double* pitch,
    double* volume) {}

void WebTestTtsControllerDelegate::SetTtsEngineDelegate(
    content::TtsEngineDelegate* delegate) {}

content::TtsEngineDelegate*
WebTestTtsControllerDelegate::GetTtsEngineDelegate() {
  return nullptr;
}

WebTestTtsControllerDelegate::WebTestTtsControllerDelegate() {}

WebTestTtsControllerDelegate::~WebTestTtsControllerDelegate() {}
