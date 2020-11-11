// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/test_utils.h"

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_generic_reader.h"

namespace blink {

bool CreateDataPipeForWebTransportTests(
    mojo::ScopedDataPipeProducerHandle* producer,
    mojo::ScopedDataPipeConsumerHandle* consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = 0;  // 0 means the system default size.

  MojoResult result = mojo::CreateDataPipe(&options, producer, consumer);
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

  ScriptPromise read_promise = reader->read(script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester read_tester(script_state, read_promise);
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  v8::Local<v8::Value> result = read_tester.Value().V8Value();
  DCHECK(result->IsObject());
  v8::Local<v8::Value> v8value;
  bool done = false;
  EXPECT_TRUE(
      V8UnpackIteratorResult(script_state, result.As<v8::Object>(), &done)
          .ToLocal(&v8value));
  EXPECT_FALSE(done);
  return v8value;
}

}  // namespace blink
