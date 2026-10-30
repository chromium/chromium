// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_manager_delegate.h"

#include "build/build_config.h"
#include "url/origin.h"

namespace content {

#if !BUILDFLAG(IS_ANDROID)
base::flat_map<std::string, std::string>
SpeechRecognitionManagerDelegate::GetRoutingParams(const url::Origin& origin) {
  return {};
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
