// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/testing/internals_fetch.h"

#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Vector<String> InternalsFetch::getInternalResponseURLList(Internals& internals,
                                                          Response* response) {
  if (!response)
    return Vector<String>();
  Vector<String> url_list;
  url_list.reserve(response->InternalURLList().size());
  for (const auto& url : response->InternalURLList())
    url_list.push_back(url);
  return url_list;
}

}  // namespace blink
