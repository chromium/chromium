// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/test_utils.h"

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_generic_reader.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

bool CreateDataPipeForWebTransportTests(
    mojo::ScopedDataPipeProducerHandle* producer,
    mojo::ScopedDataPipeConsumerHandle* consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = 0;  // 0 means the system default size.

  MojoResult result = mojo::CreateDataPipe(&options, *producer, *consumer);
  if (result != MOJO_RESULT_OK) {
    ADD_FAILURE() << "CreateDataPipe() returned " << result;
    return false;
  }
  return true;
}

v8::Local<v8::Value> ReadValueFromStream(const V8TestingScope& scope,
                                         ReadableStream* stream) {
  auto* script_state = scope.GetScriptState();
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseUntyped read_promise =
      reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester read_tester(script_state, read_promise);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  v8::Local<v8::Value> result = read_tester.Value().V8Value();
  DCHECK(result->IsObject());
  v8::Local<v8::Value> v8value;
  bool done = false;
  EXPECT_TRUE(V8UnpackIterationResult(script_state, result.As<v8::Object>(),
                                      &v8value, &done));
  EXPECT_FALSE(done);
  return v8value;
}

TestWebTransportCreator::TestWebTransportCreator() = default;

void TestWebTransportCreator::Init(ScriptState* script_state,
                                   CreateStubCallback create_stub) {
  browser_interface_broker_ =
      &ExecutionContext::From(script_state)->GetBrowserInterfaceBroker();
  create_stub_ = std::move(create_stub);
  browser_interface_broker_->SetBinderForTesting(
      mojom::blink::WebTransportConnector::Name_,
      WTF::BindRepeating(&TestWebTransportCreator::BindConnector,
                         weak_ptr_factory_.GetWeakPtr()));
  web_transport_ = WebTransport::Create(
      script_state, "https://example.com/",
      MakeGarbageCollected<WebTransportOptions>(), ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();
}

TestWebTransportCreator::~TestWebTransportCreator() {
  browser_interface_broker_->SetBinderForTesting(
      mojom::blink::WebTransportConnector::Name_, {});
}

// Implementation of mojom::blink::WebTransportConnector.
void TestWebTransportCreator::Connect(
    const KURL&,
    Vector<network::mojom::blink::WebTransportCertificateFingerprintPtr>,
    mojo::PendingRemote<network::mojom::blink::WebTransportHandshakeClient>
        pending_handshake_client) {
  mojo::Remote<network::mojom::blink::WebTransportHandshakeClient>
      handshake_client(std::move(pending_handshake_client));

  mojo::PendingRemote<network::mojom::blink::WebTransport>
      web_transport_to_pass;

  create_stub_.Run(web_transport_to_pass);

  mojo::PendingRemote<network::mojom::blink::WebTransportClient> client_remote;
  handshake_client->OnConnectionEstablished(
      std::move(web_transport_to_pass),
      client_remote.InitWithNewPipeAndPassReceiver(),
      network::mojom::blink::HttpResponseHeaders::New(),
      network::mojom::blink::WebTransportStats::New());
  client_remote_.Bind(std::move(client_remote));
}

void TestWebTransportCreator::BindConnector(
    mojo::ScopedMessagePipeHandle handle) {
  connector_receiver_.Bind(
      mojo::PendingReceiver<mojom::blink::WebTransportConnector>(
          std::move(handle)));
}

void TestWebTransportCreator::Reset() {
  client_remote_.reset();
  connector_receiver_.reset();
}

}  // namespace blink
