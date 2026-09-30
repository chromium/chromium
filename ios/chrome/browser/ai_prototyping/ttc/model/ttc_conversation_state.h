// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_STATE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_STATE_H_

// States for the TTC conversation, mirroring Desktop's State enum.
enum class TTCConversationState {
  // Conversation is inactive; audio capture is stopped.
  kStopped,
  // Microphone is actively capturing; awaiting user speech or model response.
  kListening,
  // Assistant is actively speaking / rendering response audio.
  kTalking,
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_CONVERSATION_STATE_H_
