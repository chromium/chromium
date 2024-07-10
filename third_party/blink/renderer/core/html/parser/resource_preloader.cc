// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/parser/resource_preloader.h"

#include <utility>

namespace blink {

void ResourcePreloader::TakeAndPreload(PreloadRequestStream& r) {
  PreloadRequestStream requests;
  requests.swap(r);

  for (PreloadRequestStream::iterator it = requests.begin();
       it != requests.end(); ++it)
    Preload(std::move(*it));
}

}  // namespace blink
