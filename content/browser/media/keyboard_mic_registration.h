// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEYBOARD_MIC_REGISTRATION_H_
#define CONTENT_BROWSER_MEDIA_KEYBOARD_MIC_REGISTRATION_H_

#include "base/macros.h"

namespace content {

// Chrome OS keyboard mic stream registration. Used on UI thread only and owned
// by BrowserMainLoop; instance must be obtained through
// BrowserMainLoop::keyboard_mic_registration().
class KeyboardMicRegistration {
 public:
  KeyboardMicRegistration();

  KeyboardMicRegistration(const KeyboardMicRegistration&) = delete;
  KeyboardMicRegistration& operator=(const KeyboardMicRegistration&) = delete;

  ~KeyboardMicRegistration();

  void Register();
  void Deregister();

 private:
  int register_count_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEYBOARD_MIC_REGISTRATION_H_
