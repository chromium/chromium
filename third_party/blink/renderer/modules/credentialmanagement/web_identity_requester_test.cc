// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/web_identity_requester.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_request_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// Test that the window onload delay duration is recorded and the post task
// delay duration is NOT recorded when the delay timer is started before the
// start of the window onload event.
TEST(WebIdentityRequesterTest, StartDelayTimerBeforeOnload) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  // Set the document ready state to before the window onload event.
  scope.GetDocument().SetReadyState(Document::kLoading);

  ScriptState* script_state = scope.GetScriptState();
  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);
  WebIdentityRequester* web_identity_requester =
      MakeGarbageCollected<WebIdentityRequester>(
          context, MediationRequirement::kOptional);

  // Start window onload delay timer before the window onload event starts.
  web_identity_requester->StartDelayTimer(resolver);

  // Before the window onload event has started, histograms should not have been
  // recorded.
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.Timing.PostTaskDelayDuration",
                                    0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.IsAfterWindowOnload", 0);

  // Start the window onload event.
  To<LocalDOMWindow>(resolver->GetExecutionContext())
      ->DispatchWindowLoadEvent();
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  ScriptState* script_state = scope.GetScriptState();
  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);
  WebIdentityRequester* web_identity_requester =
      MakeGarbageCollected<WebIdentityRequester>(
          context, MediationRequirement::kOptional);

  // Before the delay timer has started, histograms should not have been
  // recorded.
  histogram_tester.ExpectTotalCount(
      "Blink.FedCm.Timing.WindowOnloadDelayDuration", 0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.Timing.PostTaskDelayDuration",
                                    0);
  histogram_tester.ExpectTotalCount("Blink.FedCm.IsAfterWindowOnload", 0);

  // Start delay timer after the start of the window onload event.
  To<LocalDOMWindow>(resolver->GetExecutionContext())
      ->DispatchWindowLoadEvent();
  EXPECT_TRUE(scope.GetDocument().LoadEventFinished());
  web_identity_requester->StartDelayTimer(resolver);

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

// Tests that a get() with multiple IDPs can be successfully resolved when the
// selected IDP is not the first one.
TEST(WebIdentityRequesterTest, OnRequestTokenToSecondIdp) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  // Set the document ready state to before the window onload event.
  scope.GetDocument().SetReadyState(Document::kLoading);

  ScriptState* script_state = scope.GetScriptState();
  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<Credential>>>(
          script_state);
  WebIdentityRequester* web_identity_requester =
      MakeGarbageCollected<WebIdentityRequester>(
          context, MediationRequirement::kOptional);

  HeapVector<Member<IdentityProviderRequestOptions>> providers;
  auto* options1 = IdentityProviderRequestOptions::Create();
  options1->setClientId("123");
  options1->setConfigURL(KURL("https://idp1.example"));
  providers.push_back(options1);
  auto* options2 = IdentityProviderRequestOptions::Create();
  options2->setClientId("456");
  options2->setConfigURL(KURL("https://idp2.example"));
  providers.push_back(options2);

  web_identity_requester->AppendGetCall(resolver, providers,
                                        mojom::blink::RpContext::kSignIn,
                                        mojom::blink::RpMode::kWidget);

  // The promise should not be rejected.
  MockFunctionScope funcs(scope.GetScriptState());
  resolver->Promise().Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  web_identity_requester->OnRequestToken(
      mojom::blink::RequestTokenStatus::kSuccess, KURL("https://idp2.example"),
      "token", nullptr, /*is_auto_selected=*/false);

  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
}

}  // namespace blink
