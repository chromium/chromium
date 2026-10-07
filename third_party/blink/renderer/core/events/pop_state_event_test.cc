// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pop_state_event.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/history.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using PopStateEventTest = PageTestBase;

TEST_F(PopStateEventTest, PopStateEventStateWithMutatedHistory) {
  base::HistogramTester histogram_tester;
  NavigateTo(KURL("https://example.com/"));
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;
  LocalDOMWindow* window = GetFrame().DomWindow();
  History* history = window->history();
  history->replaceState(
      script_state,
      ScriptValue(script_state->GetIsolate(),
                  v8::Integer::New(script_state->GetIsolate(), 42)),
      String(), String(), exception_state);
  ASSERT_FALSE(exception_state.HadException());
  HistoryItem* history_item =
      GetFrame().Loader().GetDocumentLoader()->GetHistoryItem();
  ASSERT_TRUE(history_item);
  scoped_refptr<SerializedScriptValue> event_state =
      base::WrapRefCounted(history_item->StateObject());
  ASSERT_TRUE(event_state);

  // Change history state after creating popstate event. This should not affect
  // event's state.
  PopStateEvent* event = PopStateEvent::Create(
      std::move(event_state), history, /*has_ua_visual_transition=*/false,
      UserNavigationInvolvement::kNone);
  history->replaceState(
      script_state,
      ScriptValue(script_state->GetIsolate(),
                  v8::Integer::New(script_state->GetIsolate(), 84)),
      String(), String(), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptValue state = event->state(script_state, exception_state);

  EXPECT_EQ(
      42, state.V8Value()->Int32Value(script_state->GetContext()).ToChecked());
  histogram_tester.ExpectBucketCount(
      "Blink.ScriptValueDeserialization.HistoryState",
      /* kAttempted */ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.ScriptValueDeserialization.HistoryState",
      /* kSucceeded */ 1, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.ScriptValueDeserialization.HistoryState", 2);
}

}  // namespace blink
