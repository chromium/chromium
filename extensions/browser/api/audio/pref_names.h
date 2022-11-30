// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUDIO_PREF_NAMES_H_
#define EXTENSIONS_BROWSER_API_AUDIO_PREF_NAMES_H_

namespace extensions {

// List of known audio device stable IDs. The list will be used to generate
// stable device ID exposed to apps via chrome.audio API (in order to reduce
// the possibility of apps globally identifying audio devices from their stable
// IDs).
extern const char kAudioApiStableDeviceIds[];

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUDIO_PREF_NAMES_H_
