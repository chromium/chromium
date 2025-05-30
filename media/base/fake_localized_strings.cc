// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "media/base/localized_strings.h"

namespace media {

std::u16string FakeLocalizedStringProvider(MessageId message_id) {
  switch (message_id) {
    case DEFAULT_AUDIO_DEVICE_NAME:
      return u"Default";
#if BUILDFLAG(IS_WIN)
    case COMMUNICATIONS_AUDIO_DEVICE_NAME:
      return u"Communications";
#endif
#if BUILDFLAG(IS_ANDROID)
    case GENERIC_AUDIO_DEVICE_NAME:
      return u"Nameless audio device (generic)";
    case INTERNAL_SPEAKER_AUDIO_DEVICE_NAME:
      return u"Nameless audio device (internal speaker)";
    case INTERNAL_MIC_AUDIO_DEVICE_NAME:
      return u"Nameless audio device (internal mic)";
    case WIRED_HEADPHONES_AUDIO_DEVICE_NAME:
      return u"Nameless audio device (wired headphones)";
    case BLUETOOTH_AUDIO_DEVICE_NAME:
      return u"Nameless audio device (Bluetooth)";
    case USB_AUDIO_DEVICE_NAME:
      return u"Nameless audio device (USB)";
    case HDMI_AUDIO_DEVICE_NAME:
      return u"Nameless audio device (HDMI)";
#endif
  }
}

void SetUpFakeLocalizedStrings() {
  SetLocalizedStringProvider(FakeLocalizedStringProvider);
}

}  // namespace media
