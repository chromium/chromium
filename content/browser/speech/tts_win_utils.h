// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_WIN_UTILS_H_
#define CONTENT_BROWSER_SPEECH_TTS_WIN_UTILS_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

// SAPI has had known issues with handling XML and SSML that have caused
// security issues, so remove related characters here.
// static
CONTENT_EXPORT void RemoveXml(std::wstring& utterance);

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_WIN_UTILS_H_
