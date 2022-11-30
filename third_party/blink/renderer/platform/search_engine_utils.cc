// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/search_engine_utils.h"

#include "components/search_engines/search_engine_utils.h"
#include "url/gurl.h"

namespace blink {

bool IsKnownSearchEngine(const String& url) {
  GURL gurl(url.Utf8());
  if (!gurl.is_valid()) {
    return false;
  }

  return SearchEngineUtils::GetEngineType(gurl) > 0;
}

}  // namespace blink
