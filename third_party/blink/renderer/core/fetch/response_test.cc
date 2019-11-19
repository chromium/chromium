// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/response.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/fetch/fetch_response_data.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

TEST(ServiceWorkerResponseTest, FromFetchResponseData) {
  auto page = std::make_unique<DummyPageHolder>(IntSize(1, 1));
  const KURL url("http://www.response.com");

  FetchResponseData* fetch_response_data = FetchResponseData::Create();
  Vector<KURL> url_list;
  url_list.push_back(url);
  fetch_response_data->SetURLList(url_list);
  Response* response =
      Response::Create(&page->GetDocument(), fetch_response_data);
  DCHECK(response);
  EXPECT_EQ(url, response->url());
}

void CheckResponseStream(ScriptState* script_state,
                         Response* response,
                         bool check_response_body_stream_buffer) {
  BodyStreamBuffer* original_internal = response->InternalBodyBuffer();
  if (check_response_body_stream_buffer) {
    EXPECT_EQ(response->BodyBuffer(), original_internal);
  } else {
    EXPECT_FALSE(response->BodyBuffer());
  }

  DummyExceptionStateForTesting exception_state;
  Response* cloned_response = response->clone(script_state, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (!response->InternalBodyBuffer())
    FAIL() << "internalBodyBuffer() must not be null.";
  if (!cloned_response->InternalBodyBuffer())
    FAIL() << "internalBodyBuffer() must not be null.";
  EXPECT_TRUE(response->InternalBodyBuffer());
  EXPECT_TRUE(cloned_response->InternalBodyBuffer());
  EXPECT_TRUE(response->InternalBodyBuffer());
  EXPECT_TRUE(cloned_response->InternalBodyBuffer());
  EXPECT_NE(response->InternalBodyBuffer(), original_internal);
  EXPECT_NE(cloned_response->InternalBodyBuffer(), original_internal);
  EXPECT_NE(response->InternalBodyBuffer(),
            cloned_response->InternalBodyBuffer());
  if (check_response_body_stream_buffer) {
    EXPECT_EQ(response->BodyBuffer(), response->InternalBodyBuffer());
    EXPECT_EQ(cloned_response->BodyBuffer(),
              cloned_response->InternalBodyBuffer());
  } else {
    EXPECT_FALSE(response->BodyBuffer());
    EXPECT_FALSE(cloned_response->BodyBuffer());
  }
  BytesConsumerTestUtil::MockFetchDataLoaderClient* client1 =
      MakeGarbageCollected<BytesConsumerTestUtil::MockFetchDataLoaderClient>();
  BytesConsumerTestUtil::MockFetchDataLoaderClient* client2 =
      MakeGarbageCollected<BytesConsumerTestUtil::MockFetchDataLoaderClient>();
  EXPECT_CALL(*client1, DidFetchDataLoadedString(String("Hello, world")));
  EXPECT_CALL(*client2, DidFetchDataLoadedString(String("Hello, world")));

  response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(
          TextResourceDecoderOptions::CreateUTF8Decode()),
      client1, ASSERT_NO_EXCEPTION);
  cloned_response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(
          TextResourceDecoderOptions::CreateUTF8Decode()),
      client2, ASSERT_NO_EXCEPTION);
  blink::test::RunPendingTasks();
}

BodyStreamBuffer* CreateHelloWorldBuffer(ScriptState* script_state) {
  using Command = ReplayingBytesConsumer::Command;
  auto* src = MakeGarbageCollected<ReplayingBytesConsumer>(
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kNetworking));
  src->Add(Command(Command::kData, "Hello, "));
  src->Add(Command(Command::kData, "world"));
  src->Add(Command(Command::kDone));
  return MakeGarbageCollected<BodyStreamBuffer>(script_state, src, nullptr);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneDefault) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL("http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, true);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneBasic) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL("http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  fetch_response_data = fetch_response_data->CreateBasicFilteredResponse();
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, true);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneCors) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL("http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  fetch_response_data = fetch_response_data->CreateCorsFilteredResponse({});
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, true);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneOpaque) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = CreateHelloWorldBuffer(scope.GetScriptState());
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL("http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  fetch_response_data = fetch_response_data->CreateOpaqueFilteredResponse();
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  EXPECT_EQ(response->InternalBodyBuffer(), buffer);
  CheckResponseStream(scope.GetScriptState(), response, false);
}

TEST(ServiceWorkerResponseTest, BodyStreamBufferCloneError) {
  V8TestingScope scope;
  BodyStreamBuffer* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      scope.GetScriptState(),
      BytesConsumer::CreateErrored(BytesConsumer::Error()), nullptr);
  FetchResponseData* fetch_response_data =
      FetchResponseData::CreateWithBuffer(buffer);
  Vector<KURL> url_list;
  url_list.push_back(KURL("http://www.response.com"));
  fetch_response_data->SetURLList(url_list);
  Response* response =
      Response::Create(scope.GetExecutionContext(), fetch_response_data);
  DummyExceptionStateForTesting exception_state;
  Response* cloned_response =
      response->clone(scope.GetScriptState(), exception_state);
  EXPECT_FALSE(exception_state.HadException());

  BytesConsumerTestUtil::MockFetchDataLoaderClient* client1 =
      MakeGarbageCollected<BytesConsumerTestUtil::MockFetchDataLoaderClient>();
  BytesConsumerTestUtil::MockFetchDataLoaderClient* client2 =
      MakeGarbageCollected<BytesConsumerTestUtil::MockFetchDataLoaderClient>();
  EXPECT_CALL(*client1, DidFetchDataLoadFailed());
  EXPECT_CALL(*client2, DidFetchDataLoadFailed());

  response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(
          TextResourceDecoderOptions::CreateUTF8Decode()),
      client1, ASSERT_NO_EXCEPTION);
  cloned_response->InternalBodyBuffer()->StartLoading(
      FetchDataLoader::CreateLoaderAsString(
          TextResourceDecoderOptions::CreateUTF8Decode()),
      client2, ASSERT_NO_EXCEPTION);
  blink::test::RunPendingTasks();
}

}  // namespace
}  // namespace blink
