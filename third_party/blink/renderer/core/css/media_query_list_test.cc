// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class TestListener : public MediaQueryListListener {
 public:
  void NotifyMediaQueryChanged() override {}
};

}  // anonymous namespace

TEST(MediaQueryListTest, CrashInStop) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* list = MakeGarbageCollected<MediaQueryList>(
      document->GetExecutionContext(),
      MakeGarbageCollected<MediaQueryMatcher>(*document),
      MediaQuerySet::Create());
  list->AddListener(MakeGarbageCollected<TestListener>());
  list->ContextDestroyed();
  // This test passes if it's not crashed.
}

}  // namespace blink
