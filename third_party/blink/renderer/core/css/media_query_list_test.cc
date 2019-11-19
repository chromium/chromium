// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

class TestListener : public MediaQueryListListener {
 public:
  void NotifyMediaQueryChanged() override {}
};

}  // anonymous namespace

TEST(MediaQueryListTest, CrashInStop) {
  auto* document = MakeGarbageCollected<Document>();
  auto* list = MakeGarbageCollected<MediaQueryList>(
      document, MakeGarbageCollected<MediaQueryMatcher>(*document),
      MediaQuerySet::Create());
  list->AddListener(MakeGarbageCollected<TestListener>());
  list->ContextDestroyed(document);
  // This test passes if it's not crashed.
}

}  // namespace blink
