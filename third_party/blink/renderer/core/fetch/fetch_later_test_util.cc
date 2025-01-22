// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_later_test_util.h"

namespace blink {

FetchLaterTestingScope::FetchLaterTestingScope(LocalFrameClient* frame_client,
                                               const String& source_page_url)
    : V8TestingScope(DummyPageHolder::CreateAndCommitNavigation(
          KURL(source_page_url),
          /*initial_view_size=*/gfx::Size(),
          /*chrome_client=*/nullptr,
          frame_client)) {}

}  // namespace blink
