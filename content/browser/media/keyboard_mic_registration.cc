// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/keyboard_mic_registration.h"

#include "ash/components/audio/cras_audio_handler.h"
#include "content/public/browser/browser_thread.h"

namespace content {

KeyboardMicRegistration::KeyboardMicRegistration() = default;

KeyboardMicRegistration::~KeyboardMicRegistration() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(0, register_count_);
}

void KeyboardMicRegistration::Register() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (++register_count_ == 1)
    ash::CrasAudioHandler::Get()->SetKeyboardMicActive(true);
}

void KeyboardMicRegistration::Deregister() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (--register_count_ == 0)
    ash::CrasAudioHandler::Get()->SetKeyboardMicActive(false);
}

}  // namespace content
