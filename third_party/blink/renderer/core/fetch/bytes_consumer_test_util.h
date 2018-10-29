// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BYTES_CONSUMER_TEST_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BYTES_CONSUMER_TEST_UTIL_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;

class BytesConsumerTestUtil {
  STATIC_ONLY(BytesConsumerTestUtil);

 public:
  class MockBytesConsumer : public BytesConsumer {
   public:
    static MockBytesConsumer* Create() {
      return new testing::StrictMock<MockBytesConsumer>();
    }

    MOCK_METHOD2(BeginRead, Result(const char**, size_t*));
    MOCK_METHOD1(EndRead, Result(size_t));
    MOCK_METHOD1(DrainAsBlobDataHandle,
                 scoped_refptr<BlobDataHandle>(BlobSizePolicy));
    MOCK_METHOD0(DrainAsFormData, scoped_refptr<EncodedFormData>());
    MOCK_METHOD1(SetClient, void(Client*));
    MOCK_METHOD0(ClearClient, void());
    MOCK_METHOD0(Cancel, void());
    MOCK_CONST_METHOD0(GetPublicState, PublicState());
    MOCK_CONST_METHOD0(GetError, Error());

    String DebugName() const override { return "MockBytesConsumer"; }

   protected:
    MockBytesConsumer();
  };

  class MockFetchDataLoaderClient
      : public GarbageCollectedFinalized<MockFetchDataLoaderClient>,
        public FetchDataLoader::Client {
    USING_GARBAGE_COLLECTED_MIXIN(MockFetchDataLoaderClient);

   public:
    static testing::StrictMock<MockFetchDataLoaderClient>* Create() {
      return new testing::StrictMock<MockFetchDataLoaderClient>;
    }

    void Trace(blink::Visitor* visitor) override {
      FetchDataLoader::Client::Trace(visitor);
    }

    MOCK_METHOD1(DidFetchDataLoadedBlobHandleMock,
                 void(scoped_refptr<BlobDataHandle>));
    MOCK_METHOD1(DidFetchDataLoadedArrayBufferMock, void(DOMArrayBuffer*));
    MOCK_METHOD1(DidFetchDataLoadedFormDataMock, void(FormData*));
    MOCK_METHOD1(DidFetchDataLoadedString, void(const String&));
    MOCK_METHOD0(DidFetchDataLoadStream, void());
    MOCK_METHOD0(DidFetchDataLoadFailed, void());
    MOCK_METHOD0(Abort, void());

    void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
      DidFetchDataLoadedArrayBufferMock(array_buffer);
    }
    // TODO(yhirano): Remove DidFetchDataLoadedBlobHandleMock.
    void DidFetchDataLoadedBlobHandle(
        scoped_refptr<BlobDataHandle> blob_data_handle) override {
      DidFetchDataLoadedBlobHandleMock(std::move(blob_data_handle));
    }
    void DidFetchDataLoadedFormData(FormData* FormData) override {
      DidFetchDataLoadedFormDataMock(FormData);
    }
  };

  class Command final {
    DISALLOW_NEW();

   public:
    enum Name {
      kData,
      kDone,
      kError,
      kWait,
    };

    explicit Command(Name name) : name_(name) {}
    Command(Name name, const Vector<char>& body) : name_(name), body_(body) {}
    Command(Name name, const char* body, size_t size) : name_(name) {
      body_.Append(body, size);
    }
    Command(Name name, const char* body) : Command(name, body, strlen(body)) {}
    Name GetName() const { return name_; }
    const Vector<char>& Body() const { return body_; }

   private:
    const Name name_;
    Vector<char> body_;
  };

  // ReplayingBytesConsumer stores commands via |add| and replays the stored
  // commends when read.
  class ReplayingBytesConsumer final : public BytesConsumer {
   public:
    // The ExecutionContext is needed to get a base::SingleThreadTaskRunner.
    explicit ReplayingBytesConsumer(ExecutionContext*);
    ~ReplayingBytesConsumer() override;

    // Add a command to this handle. This function must be called BEFORE
    // any BytesConsumer methods are called.
    void Add(const Command& command) { commands_.push_back(command); }

    Result BeginRead(const char** buffer, size_t* available) override;
    Result EndRead(size_t read_size) override;

    void SetClient(Client*) override;
    void ClearClient() override;
    void Cancel() override;
    PublicState GetPublicState() const override;
    Error GetError() const override;
    String DebugName() const override { return "ReplayingBytesConsumer"; }

    bool IsCancelled() const { return is_cancelled_; }

    void Trace(blink::Visitor*) override;

   private:
    void NotifyAsReadable(int notification_token);
    void Close();
    void MakeErrored(const Error&);

    Member<ExecutionContext> execution_context_;
    Member<BytesConsumer::Client> client_;
    InternalState state_ = InternalState::kWaiting;
    Deque<Command> commands_;
    size_t offset_ = 0;
    BytesConsumer::Error error_;
    int notification_token_ = 0;
    bool is_cancelled_ = false;
  };

  class TwoPhaseReader final : public GarbageCollectedFinalized<TwoPhaseReader>,
                               public BytesConsumer::Client {
    USING_GARBAGE_COLLECTED_MIXIN(TwoPhaseReader);

   public:
    // |consumer| must not have a client when called.
    explicit TwoPhaseReader(BytesConsumer* /* consumer */);

    void OnStateChange() override;
    String DebugName() const override { return "TwoPhaseReader"; }
    std::pair<BytesConsumer::Result, Vector<char>> Run();

    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(consumer_);
      BytesConsumer::Client::Trace(visitor);
    }

   private:
    Member<BytesConsumer> consumer_;
    BytesConsumer::Result result_ = BytesConsumer::Result::kShouldWait;
    Vector<char> data_;
  };

  static String CharVectorToString(const Vector<char>&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BYTES_CONSUMER_TEST_UTIL_H_
