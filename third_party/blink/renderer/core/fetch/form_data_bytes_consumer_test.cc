// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/testing/file_backed_blob_factory_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using Result = BytesConsumer::Result;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Return;
using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;
using MockBytesConsumer = BytesConsumerTestUtil::MockBytesConsumer;

class SimpleDataPipeGetter : public network::mojom::blink::DataPipeGetter {
 public:
  SimpleDataPipeGetter(
      const String& str,
      mojo::PendingReceiver<network::mojom::blink::DataPipeGetter> receiver)
      : str_(str) {
    receivers_.set_disconnect_handler(WTF::BindRepeating(
        &SimpleDataPipeGetter::OnMojoDisconnect, WTF::Unretained(this)));
    receivers_.Add(this, std::move(receiver));
  }
  SimpleDataPipeGetter(const SimpleDataPipeGetter&) = delete;
  SimpleDataPipeGetter& operator=(const SimpleDataPipeGetter&) = delete;
  ~SimpleDataPipeGetter() override = default;

  // network::mojom::DataPipeGetter implementation:
  void Read(mojo::ScopedDataPipeProducerHandle handle,
            ReadCallback callback) override {
    bool result = mojo::BlockingCopyFromString(str_.Utf8(), handle);
    ASSERT_TRUE(result);
    std::move(callback).Run(0 /* OK */, str_.length());
  }

  void Clone(mojo::PendingReceiver<network::mojom::blink::DataPipeGetter> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  void OnMojoDisconnect() {
    if (receivers_.empty())
      delete this;
  }

 private:
  String str_;
  mojo::ReceiverSet<network::mojom::blink::DataPipeGetter> receivers_;
};

scoped_refptr<EncodedFormData> ComplexFormData() {
  scoped_refptr<EncodedFormData> data = EncodedFormData::Create();

  data->AppendData("foo", 3);
  data->AppendFileRange("/foo/bar/baz", 3, 4,
                        base::Time::FromSecondsSinceUnixEpoch(5));
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendText("hello", false);
  auto size = blob_data->length();
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(blob_data), size);
  data->AppendBlob(blob_data_handle);
  Vector<char> boundary;
  boundary.Append("\0", 1);
  data->SetBoundary(boundary);
  return data;
}

scoped_refptr<EncodedFormData> DataPipeFormData() {
  WebHTTPBody body;
  body.Initialize();
  // Add data.
  body.AppendData(WebData("foo", 3));

  // Add data pipe.
  mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
      data_pipe_getter_remote;
  // Object deletes itself.
  new SimpleDataPipeGetter(
      String(" hello world"),
      data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  body.AppendDataPipe(std::move(data_pipe_getter_remote));

  // Add another data pipe.
  mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
      data_pipe_getter_remote2;
  // Object deletes itself.
  new SimpleDataPipeGetter(
      String(" here's another data pipe "),
      data_pipe_getter_remote2.InitWithNewPipeAndPassReceiver());
  body.AppendDataPipe(std::move(data_pipe_getter_remote2));

  // Add some more data.
  body.AppendData(WebData("bar baz", 7));

  body.SetUniqueBoundary();
  return body;
}

class NoopClient final : public GarbageCollected<NoopClient>,
                         public BytesConsumer::Client {
 public:
  void OnStateChange() override {}
  String DebugName() const override { return "NoopClient"; }
};

class FormDataBytesConsumerTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    file_factory_helper_ = std::make_unique<FileBackedBlobFactoryTestHelper>(
        GetFrame().GetDocument()->GetExecutionContext());
  }

  String DrainAsString(scoped_refptr<EncodedFormData> input_form_data) {
    auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
        GetFrame().DomWindow(), input_form_data);
    auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(consumer);
    std::pair<BytesConsumer::Result, Vector<char>> result = reader->Run();
    EXPECT_EQ(Result::kDone, result.first);
    return String(result.second);
  }

  scoped_refptr<EncodedFormData> DrainAsFormData(
      scoped_refptr<EncodedFormData> input_form_data) {
    auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
        GetFrame().DomWindow(), input_form_data);
    return consumer->DrainAsFormData();
  }

  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      scoped_refptr<EncodedFormData> input_form_data) {
    auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
        GetFrame().DomWindow(), input_form_data);
    return consumer->DrainAsBlobDataHandle(
        BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize);
  }

 private:
  std::unique_ptr<FileBackedBlobFactoryTestHelper> file_factory_helper_;
};

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromString) {
  auto result =
      (MakeGarbageCollected<BytesConsumerTestReader>(
           MakeGarbageCollected<FormDataBytesConsumer>("hello, world")))
          ->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("hello, world", String(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromStringNonLatin) {
  constexpr UChar kCs[] = {0x3042, 0};
  auto result = (MakeGarbageCollected<BytesConsumerTestReader>(
                     MakeGarbageCollected<FormDataBytesConsumer>(String(kCs))))
                    ->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("\xe3\x81\x82", String(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromArrayBuffer) {
  constexpr unsigned char kData[] = {0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3,
                                     0x42, 0x30, 0x42, 0x99, 0x88};
  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(kData);
  auto result = (MakeGarbageCollected<BytesConsumerTestReader>(
                     MakeGarbageCollected<FormDataBytesConsumer>(buffer)))
                    ->Run();
  Vector<char> expected;
  expected.Append(kData, std::size(kData));

  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ(expected, result.second);
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromArrayBufferView) {
  constexpr unsigned char kData[] = {0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3,
                                     0x42, 0x30, 0x42, 0x99, 0x88};
  constexpr size_t kOffset = 1, kSize = 4;
  DOMArrayBuffer* buffer = DOMArrayBuffer::Create(kData);
  auto result = (MakeGarbageCollected<BytesConsumerTestReader>(
                     MakeGarbageCollected<FormDataBytesConsumer>(
                         DOMUint8Array::Create(buffer, kOffset, kSize))))
                    ->Run();
  Vector<char> expected;
  expected.AppendSpan(base::span(kData).subspan(kOffset, kSize));

  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ(expected, result.second);
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromSimpleFormData) {
  scoped_refptr<EncodedFormData> data = EncodedFormData::Create();
  data->AppendData("foo", 3);
  data->AppendData("hoge", 4);

  auto result = (MakeGarbageCollected<BytesConsumerTestReader>(
                     MakeGarbageCollected<FormDataBytesConsumer>(
                         GetFrame().DomWindow(), data)))
                    ->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("foohoge", String(result.second));
}

TEST_F(FormDataBytesConsumerTest, TwoPhaseReadFromComplexFormData) {
  scoped_refptr<EncodedFormData> data = ComplexFormData();
  auto* underlying = MakeGarbageCollected<MockBytesConsumer>();
  auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), data, underlying);
  Checkpoint checkpoint;

  base::span<const char> buffer;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, BeginRead(buffer)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*underlying, EndRead(0)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  checkpoint.Call(2);
  EXPECT_EQ(Result::kOk, consumer->EndRead(0));
  checkpoint.Call(3);
}

TEST_F(FormDataBytesConsumerTest, EndReadCanReturnDone) {
  BytesConsumer* consumer =
      MakeGarbageCollected<FormDataBytesConsumer>("hello, world");
  base::span<const char> buffer;
  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  ASSERT_EQ(12u, buffer.size());
  EXPECT_EQ("hello, world", String(base::as_bytes(buffer)));
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->EndRead(buffer.size()));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromString) {
  BytesConsumer* consumer =
      MakeGarbageCollected<FormDataBytesConsumer>("hello, world");
  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_EQ(String(), blob_data_handle->GetType());
  EXPECT_EQ(12u, blob_data_handle->size());
  EXPECT_FALSE(consumer->DrainAsFormData());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromArrayBuffer) {
  BytesConsumer* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      DOMArrayBuffer::Create(base::byte_span_from_cstring("foo")));
  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_EQ(String(), blob_data_handle->GetType());
  EXPECT_EQ(3u, blob_data_handle->size());
  EXPECT_FALSE(consumer->DrainAsFormData());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromSimpleFormData) {
  auto* data = MakeGarbageCollected<FormData>(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  scoped_refptr<EncodedFormData> input_form_data =
      data->EncodeMultiPartFormData();

  BytesConsumer* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data);
  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_EQ(String(), blob_data_handle->GetType());
  EXPECT_EQ(input_form_data->FlattenToString().Utf8().length(),
            blob_data_handle->size());
  EXPECT_FALSE(consumer->DrainAsFormData());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsBlobDataHandleFromComplexFormData) {
  scoped_refptr<EncodedFormData> input_form_data = ComplexFormData();

  BytesConsumer* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data);
  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer->DrainAsBlobDataHandle();
  ASSERT_TRUE(blob_data_handle);

  EXPECT_FALSE(consumer->DrainAsFormData());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromString) {
  BytesConsumer* consumer =
      MakeGarbageCollected<FormDataBytesConsumer>("hello, world");
  scoped_refptr<EncodedFormData> form_data = consumer->DrainAsFormData();
  ASSERT_TRUE(form_data);
  EXPECT_EQ("hello, world", form_data->FlattenToString());

  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromArrayBuffer) {
  BytesConsumer* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      DOMArrayBuffer::Create(base::byte_span_from_cstring("foo")));
  scoped_refptr<EncodedFormData> form_data = consumer->DrainAsFormData();
  ASSERT_TRUE(form_data);
  EXPECT_TRUE(form_data->IsSafeToSendToAnotherThread());
  EXPECT_EQ("foo", form_data->FlattenToString());

  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromSimpleFormData) {
  auto* data = MakeGarbageCollected<FormData>(UTF8Encoding());
  data->append("name1", "value1");
  data->append("name2", "value2");
  scoped_refptr<EncodedFormData> input_form_data =
      data->EncodeMultiPartFormData();

  BytesConsumer* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data);
  EXPECT_EQ(input_form_data, consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, DrainAsFormDataFromComplexFormData) {
  scoped_refptr<EncodedFormData> input_form_data = ComplexFormData();

  BytesConsumer* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data);
  EXPECT_EQ(input_form_data, consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  base::span<const char> buffer;
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, BeginReadAffectsDraining) {
  base::span<const char> buffer;
  BytesConsumer* consumer =
      MakeGarbageCollected<FormDataBytesConsumer>("hello, world");
  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_EQ("hello, world", String(base::as_bytes(buffer)));

  ASSERT_EQ(Result::kOk, consumer->EndRead(0));
  EXPECT_FALSE(consumer->DrainAsFormData());
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
}

TEST_F(FormDataBytesConsumerTest, BeginReadAffectsDrainingWithComplexFormData) {
  auto* underlying = MakeGarbageCollected<MockBytesConsumer>();
  BytesConsumer* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), ComplexFormData(), underlying);

  base::span<const char> buffer;
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, BeginRead(buffer)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(*underlying, EndRead(0)).WillOnce(Return(Result::kOk));
  EXPECT_CALL(checkpoint, Call(2));
  // drainAsFormData should not be called here.
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*underlying, DrainAsBlobDataHandle(_));
  EXPECT_CALL(checkpoint, Call(4));
  // |consumer| delegates the getPublicState call to |underlying|.
  EXPECT_CALL(*underlying, GetPublicState())
      .WillOnce(Return(BytesConsumer::PublicState::kReadableOrWaiting));
  EXPECT_CALL(checkpoint, Call(5));

  checkpoint.Call(1);
  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  ASSERT_EQ(Result::kOk, consumer->EndRead(0));
  checkpoint.Call(2);
  EXPECT_FALSE(consumer->DrainAsFormData());
  checkpoint.Call(3);
  EXPECT_FALSE(consumer->DrainAsBlobDataHandle());
  checkpoint.Call(4);
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
  checkpoint.Call(5);
}

TEST_F(FormDataBytesConsumerTest, SetClientWithComplexFormData) {
  scoped_refptr<EncodedFormData> input_form_data = ComplexFormData();

  auto* underlying = MakeGarbageCollected<MockBytesConsumer>();
  auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data, underlying);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, SetClient(_));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*underlying, ClearClient());
  EXPECT_CALL(checkpoint, Call(3));

  checkpoint.Call(1);
  consumer->SetClient(MakeGarbageCollected<NoopClient>());
  checkpoint.Call(2);
  consumer->ClearClient();
  checkpoint.Call(3);
}

TEST_F(FormDataBytesConsumerTest, CancelWithComplexFormData) {
  scoped_refptr<EncodedFormData> input_form_data = ComplexFormData();

  auto* underlying = MakeGarbageCollected<MockBytesConsumer>();
  auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data, underlying);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*underlying, Cancel());
  EXPECT_CALL(checkpoint, Call(2));

  checkpoint.Call(1);
  consumer->Cancel();
  checkpoint.Call(2);
}

// Tests consuming an EncodedFormData with data pipe elements.
TEST_F(FormDataBytesConsumerTest, DataPipeFormData) {
  scoped_refptr<EncodedFormData> input_form_data = DataPipeFormData();
  auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data);
  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(consumer);
  std::pair<BytesConsumer::Result, Vector<char>> result = reader->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ("foo hello world here's another data pipe bar baz",
            String(result.second));
}

// Tests DrainAsFormData() on an EncodedFormData with data pipe elements.
TEST_F(FormDataBytesConsumerTest, DataPipeFormData_DrainAsFormData) {
  scoped_refptr<EncodedFormData> input_form_data = DataPipeFormData();
  auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data);
  scoped_refptr<EncodedFormData> drained_form_data =
      consumer->DrainAsFormData();
  EXPECT_EQ(*input_form_data, *drained_form_data);
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

// Tests DrainAsFormData() on an EncodedFormData with data pipe elements after
// starting to read.
TEST_F(FormDataBytesConsumerTest,
       DataPipeFormData_DrainAsFormDataWhileReading) {
  // Create the consumer and start reading.
  scoped_refptr<EncodedFormData> input_form_data = DataPipeFormData();
  auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
      GetFrame().DomWindow(), input_form_data);
  base::span<const char> buffer;
  EXPECT_EQ(BytesConsumer::Result::kOk, consumer->BeginRead(buffer));
  EXPECT_EQ("foo", String(base::as_bytes(buffer)));

  // Try to drain form data. It should return null since we started reading.
  scoped_refptr<EncodedFormData> drained_form_data =
      consumer->DrainAsFormData();
  EXPECT_FALSE(drained_form_data);
  EXPECT_EQ(BytesConsumer::PublicState::kReadableOrWaiting,
            consumer->GetPublicState());
  EXPECT_EQ(BytesConsumer::Result::kOk, consumer->EndRead(buffer.size()));

  // The consumer should still be readable. Finish reading.
  auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(consumer);
  std::pair<BytesConsumer::Result, Vector<char>> result = reader->Run();
  EXPECT_EQ(Result::kDone, result.first);
  EXPECT_EQ(" hello world here's another data pipe bar baz",
            String(result.second));
}

void AppendDataPipe(scoped_refptr<EncodedFormData> data, String content) {
  mojo::PendingRemote<network::mojom::blink::DataPipeGetter> data_pipe_getter;
  // Object deletes itself.
  new SimpleDataPipeGetter(content,
                           data_pipe_getter.InitWithNewPipeAndPassReceiver());
  auto wrapped =
      base::MakeRefCounted<WrappedDataPipeGetter>(std::move(data_pipe_getter));
  data->AppendDataPipe(std::move(wrapped));
}

scoped_refptr<BlobDataHandle> CreateBlobHandle(const String& content) {
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendText(content, false);
  auto size = blob_data->length();
  return BlobDataHandle::Create(std::move(blob_data), size);
}

scoped_refptr<EncodedFormData> CreateDataPipeData() {
  scoped_refptr<EncodedFormData> data = EncodedFormData::Create();
  Vector<char> boundary;
  boundary.Append("\0", 1);
  data->SetBoundary(boundary);

  data->AppendData("foo", 3);
  AppendDataPipe(data, " hello world");
  return data;
}

TEST_F(FormDataBytesConsumerTest, InvalidType1) {
  const String kExpected = "foo hello world";
  ASSERT_EQ(kExpected, DrainAsString(CreateDataPipeData()));

  scoped_refptr<EncodedFormData> data = CreateDataPipeData();
  data->AppendBlob(CreateBlobHandle("bar"));
  ASSERT_EQ(EncodedFormData::FormDataType::kInvalid, data->GetType());

  // sizeof("foo" + "bar") ignoring the mid "hello world" datapipe.
  // TODO(crbug.com/374124998): Unfortunately BytesConsumerTestReader can not
  // work with blob to drain string. We should fix it.
  EXPECT_EQ(6u, DrainAsBlobDataHandle(data)->size());
}

scoped_refptr<EncodedFormData> CreateBlobData() {
  scoped_refptr<EncodedFormData> data = EncodedFormData::Create();
  Vector<char> boundary;
  boundary.Append("\0", 1);
  data->SetBoundary(boundary);

  data->AppendData("foo", 3);
  data->AppendBlob(CreateBlobHandle("bar"));
  return data;
}

TEST_F(FormDataBytesConsumerTest, InvalidType2) {
  scoped_refptr<EncodedFormData> data = CreateBlobData();
  AppendDataPipe(data, " datapipe");
  ASSERT_EQ(EncodedFormData::FormDataType::kInvalid, data->GetType());

  auto* consumer =
      MakeGarbageCollected<FormDataBytesConsumer>(GetFrame().DomWindow(), data);
  Vector<char> str;
  {
    base::span<const char> buffer;
    EXPECT_EQ(BytesConsumer::Result::kOk, consumer->BeginRead(buffer));
    str.AppendSpan(buffer);
    EXPECT_EQ(BytesConsumer::Result::kOk, consumer->EndRead(buffer.size()));
  }
  EXPECT_EQ("foo", String(str));

  {
    base::span<const char> buffer;
    EXPECT_EQ(BytesConsumer::Result::kError, consumer->BeginRead(buffer));
  }
}
// TODO(crbug.com/374124998): We should add more testing.

}  // namespace
}  // namespace blink
