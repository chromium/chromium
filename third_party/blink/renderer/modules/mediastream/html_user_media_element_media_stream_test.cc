// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class HTMLUserMediaElementMediaStreamTest : public PageTestBase {};

TEST_F(HTMLUserMediaElementMediaStreamTest, StreamInitializationAndRetrieval) {
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  // Should lazily initialize and return nullptr stream initially
  MediaStream* stream = HTMLUserMediaElementMediaStream::stream(*element);
  EXPECT_EQ(stream, nullptr);

  // Set stream
  auto* new_stream = MediaStream::Create(GetDocument().GetExecutionContext());
  HTMLUserMediaElementMediaStream::From(*element).SetMediaStream(new_stream);

  // Retrieve via helper
  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), new_stream);
}

}  // namespace blink
