// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/search_engine_utils.h"

#include "components/search_engines/search_engine_utils.h"
#include "url/gurl.h"

namespace blink {

bool IsKnownSearchEngine(const AtomicString& url) {
  GURL gurl(url.GetString().Utf8());

  return SearchEngineUtils::GetEngineType(gurl) > 0;
}

}  // namespace blink
