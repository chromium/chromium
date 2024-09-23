// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_IDNA_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_IDNA_UTIL_H_

#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Returns a console message if the hostname of `url` contains IDNA 2008
// deviation characters. Returns empty string otherwise.
String GetConsoleWarningForIDNADeviationCharacters(const KURL& url);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_IDNA_UTIL_H_
