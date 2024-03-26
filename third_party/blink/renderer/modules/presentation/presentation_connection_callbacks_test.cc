// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection_callbacks.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

constexpr char kPresentationUrl[] = "https://example.com";
constexpr char kPresentationId[] = "xyzzy";

namespace blink {

using mojom::blink::PresentationConnectionResult;
using mojom::blink::PresentationConnectionResultPtr;
using mojom::blink::PresentationConnectionState;
using mojom::blink::PresentationError;
using mojom::blink::PresentationErrorType;
using mojom::blink::PresentationInfo;
using mojom::blink::PresentationInfoPtr;

namespace {

static PresentationRequest* MakeRequest(V8TestingScope* scope) {
  PresentationRequest* request =
      PresentationRequest::Create(scope->GetExecutionContext(),
                                  kPresentationUrl, scope->GetExceptionState());
  EXPECT_FALSE(scope->GetExceptionState().HadException());
  return request;
}

}  // namespace

TEST(PresentationConnectionCallbacksTest, HandleSuccess) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          scope.GetScriptState());
  resolver->Promise().Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  PresentationConnectionCallbacks callbacks(resolver, MakeRequest(&scope));

  // No connection currently exists.
  EXPECT_FALSE(callbacks.connection_);

  mojo::PendingRemote<mojom::blink::PresentationConnection> connection_remote;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      connection_receiver = connection_remote.InitWithNewPipeAndPassReceiver();
  PresentationConnectionResultPtr result = PresentationConnectionResult::New(
      PresentationInfo::New(url_test_helpers::ToKURL(kPresentationUrl),
                            kPresentationId),
      std::move(connection_remote), std::move(connection_receiver));

  callbacks.HandlePresentationResponse(std::move(result), nullptr);

  // New connection was created.
  ControllerPresentationConnection* connection = callbacks.connection_.Get();
  ASSERT_TRUE(connection);
  EXPECT_EQ(connection->GetState(), PresentationConnectionState::CONNECTING);

  // Connection must be closed before the next connection test.
  connection->close();
}

TEST(PresentationConnectionCallbacksTest, HandleReconnect) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PresentationInfoPtr info = PresentationInfo::New(
      url_test_helpers::ToKURL(kPresentationUrl), kPresentationId);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          scope.GetScriptState());
  resolver->Promise().Then(funcs.ExpectCall(), funcs.ExpectNoCall());

  auto* connection = ControllerPresentationConnection::Take(
      resolver, *info, MakeRequest(&scope));
  // Connection must be closed for reconnection to succeed.
  connection->close();

  PresentationConnectionCallbacks callbacks(resolver, connection);

  mojo::PendingRemote<mojom::blink::PresentationConnection> connection_remote;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      connection_receiver = connection_remote.InitWithNewPipeAndPassReceiver();
  PresentationConnectionResultPtr result = PresentationConnectionResult::New(
      std::move(info), std::move(connection_remote),
      std::move(connection_receiver));

  callbacks.HandlePresentationResponse(std::move(result), nullptr);

  // Previous connection was returned.
  ControllerPresentationConnection* new_connection =
      callbacks.connection_.Get();
  EXPECT_EQ(connection, new_connection);
  EXPECT_EQ(new_connection->GetState(),
            PresentationConnectionState::CONNECTING);

  // Connection must be closed before the next connection test.
  connection->close();
}

TEST(PresentationConnectionCallbacksTest, HandleError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          scope.GetScriptState());
  resolver->Promise().Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  PresentationConnectionCallbacks callbacks(resolver, MakeRequest(&scope));

  // No connection currently exists.
  EXPECT_FALSE(callbacks.connection_);

  callbacks.HandlePresentationResponse(
      nullptr,
      PresentationError::New(PresentationErrorType::NO_PRESENTATION_FOUND,
                             "Something bad happened"));

  // No connection was created.
  EXPECT_FALSE(callbacks.connection_);
}

}  // namespace blink
