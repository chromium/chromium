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

// Test that the window onload delay duration is recorded and the post task
// delay duration is NOT recorded when the delay timer is started before the
// start of the window onload event.
TEST(WebIdentityRequesterTest, StartDelayTimerBeforeOnload) {
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

  // Start window onload delay timer before the window onload event starts.
  web_identity_requester->StartDelayTimer(WrapPersistent(resolver));

  // Before the window onload event has started, histograms should not have been
  // recorded.
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.Timing.PostTaskDelayDuration",
                                    0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.IsAfterWindowOnload", 0);

  // Start the window onload event.
  resolver->DomWindow()->DispatchWindowLoadEvent();
  EXPECT_TRUE(scope.GetDocument().LoadEventFinished());

  // Since stopping the delay timer is done by posting a task, we wait for all
  // tasks to be processed before checking for histograms.
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 1);
  histogram_tester.ExpectTotalCount("Blink.FedCm.Timing.PostTaskDelayDuration",
                                    0);
  histogram_tester.ExpectUniqueSample("Blink.FedCm.IsAfterWindowOnload", false,
                                      1);
}

// Test that the window onload delay duration is NOT recorded and the post task
// delay duration is recorded when the delay timer is started after the start of
// the window onload event.
TEST(WebIdentityRequesterTest, StartDelayTimerAfterOnload) {
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  ScriptState* script_state = scope.GetScriptState();
  ExecutionContext* context = ExecutionContext::From(script_state);
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  WebIdentityRequester* web_identity_requester =
      MakeGarbageCollected<WebIdentityRequester>(WrapPersistent(context));

  // Before the delay timer has started, histograms should not have been
  // recorded.
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.Timing.PostTaskDelayDuration",
                                    0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.IsAfterWindowOnload", 0);

  // Start delay timer after the start of the window onload event.
  resolver->DomWindow()->DispatchWindowLoadEvent();
  EXPECT_TRUE(scope.GetDocument().LoadEventFinished());
  web_identity_requester->StartDelayTimer(WrapPersistent(resolver));

  // Since stopping the delay timer is done by posting a task, we wait for all
  // tasks to be processed before checking for histograms.
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.Timing.PostTaskDelayDuration",
                                    1);
  histogram_tester.ExpectUniqueSample("Blink.FedCm.IsAfterWindowOnload", true,
                                      1);
}

}  // namespace blink
