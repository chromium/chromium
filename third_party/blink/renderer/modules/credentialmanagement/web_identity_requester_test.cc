// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_requester.h"

namespace blink {

// Test that the window onload delay duration is recorded upon the execution of
// the window onload event.
TEST(WebIdentityRequesterTest, StartWindowOnloadDelayTimerBeforeOnload) {
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  // Set the document ready state to before the window onload event.
  scope.GetDocument().SetReadyState(Document::kLoading);

  ScriptState* script_state = scope.GetScriptState();
  ExecutionContext* context = ExecutionContext::From(script_state);
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  WebIdentityRequester* web_identity_requester =
      MakeGarbageCollected<WebIdentityRequester>(WrapPersistent(context));

  // Start window onload delay timer before the window onload event.
  web_identity_requester->StartWindowOnloadDelayTimer(WrapPersistent(resolver));

  // Before the window onload event is fired, the histogram should not have been
  // recorded.
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 0);
  resolver->DomWindow()->DispatchWindowLoadEvent();
  EXPECT_TRUE(scope.GetDocument().LoadEventFinished());

  // Since stopping the window onload delay timer is done by posting a task, we
  // wait for all tasks to be processed before checking for histogram presence.
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 1);
  histogram_tester.ExpectUniqueSample("Blink.FedCm.IsAfterWindowOnload", false,
                                      1);
}

// Test that the window onload delay duration is NOT recorded when timer is
// started after the window onload event.
TEST(WebIdentityRequesterTest, StartWindowOnloadDelayTimerAfterOnload) {
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  ScriptState* script_state = scope.GetScriptState();
  ExecutionContext* context = ExecutionContext::From(script_state);
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  WebIdentityRequester* web_identity_requester =
      MakeGarbageCollected<WebIdentityRequester>(WrapPersistent(context));

  // Start window onload delay timer after the window onload event.
  resolver->DomWindow()->DispatchWindowLoadEvent();
  EXPECT_TRUE(scope.GetDocument().LoadEventFinished());
  web_identity_requester->StartWindowOnloadDelayTimer(WrapPersistent(resolver));

  // Since stopping the window onload delay timer is done by posting a task, we
  // wait for all tasks to be processed before checking for histogram absence.
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 0);
  histogram_tester.ExpectUniqueSample("Blink.FedCm.IsAfterWindowOnload", true,
                                      1);
}

}  // namespace blink
