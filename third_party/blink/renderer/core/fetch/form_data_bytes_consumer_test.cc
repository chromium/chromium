// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom.h"
#include "third_party/blink/public/platform/platform.h"
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
#include "third_party/blink/renderer/platform/blob/testing/fake_blob_registry.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
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

class DataPipeGetterImpl : public network::mojom::blink::DataPipeGetter {
 public:
  explicit DataPipeGetterImpl(
      mojo::PendingReceiver<network::mojom::blink::DataPipeGetter> receiver) {
    receivers_.set_disconnect_handler(
        BindRepeating(&DataPipeGetterImpl::OnMojoDisconnect, Unretained(this)));
    receivers_.Add(this, std::move(receiver));
  }
  DataPipeGetterImpl(const DataPipeGetterImpl&) = delete;
  DataPipeGetterImpl& operator=(const DataPipeGetterImpl&) = delete;
  ~DataPipeGetterImpl() override = default;

  // network::mojom::DataPipeGetter implementation:
  void Read(mojo::ScopedDataPipeProducerHandle handle,
            ReadCallback callback) override {
    handle_ = std::move(handle);
    callback_ = std::move(callback);
    if (read_callback_) {
      std::move(read_callback_).Run();
    }
  }

  void SetReadCallback(base::OnceCallback<void()> read_callback) {
    read_callback_ = std::move(read_callback);
  }

  void Write(String str) {
    bool result = mojo::BlockingCopyFromString(str.Utf8(), handle_);
    ASSERT_TRUE(result);
  }

  void Done(int status, uint64_t size) {
    handle_.reset();
    std::move(callback_).Run(status, size);
  }

  void Clone(mojo::PendingReceiver<network::mojom::blink::DataPipeGetter>
                 receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  void OnMojoDisconnect() {
    if (receivers_.empty()) {
      delete this;
    }
  }

 private:
  mojo::ReceiverSet<network::mojom::blink::DataPipeGetter> receivers_;
  mojo::ScopedDataPipeProducerHandle handle_;
  ReadCallback callback_;
  base::OnceCallback<void()> read_callback_;
};

class SimpleDataPipeGetter : public DataPipeGetterImpl {
 public:
  SimpleDataPipeGetter(
      const String& str,
      mojo::PendingReceiver<network::mojom::blink::DataPipeGetter> receiver)
      : DataPipeGetterImpl(std::move(receiver)), str_(str) {}
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

 private:
  String str_;
};

scoped_refptr<EncodedFormData> ComplexFormData() {
  scoped_refptr<EncodedFormData> data = EncodedFormData::Create();

  data->AppendData(base::span_from_cstring("foo"));
  data->AppendFileRange("/foo/bar/baz", 3, 4,
                        base::Time::FromSecondsSinceUnixEpoch(5));
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendText("hello", false);
  auto size = blob_data->length();
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(blob_data), size);
  data->AppendBlob(blob_data_handle);
  Vector<char> boundary;
  boundary.push_back('\0');
  data->SetBoundary(boundary);
  return data;
}

scoped_refptr<EncodedFormData> DataPipeFormData() {
  WebHTTPBody body;
  body.Initialize();
  // Add data.
  body.AppendData(WebData(base::byte_span_from_cstring("foo")));

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
  body.AppendData(WebData(base::byte_span_from_cstring("bar baz")));

  body.SetUniqueBoundary();
  return body;
}

class NoopClient final : public GarbageCollected<NoopClient>,
                         public BytesConsumer::Client {
 public:
  void OnStateChange() override {}
  String DebugName() const override { return "NoopClient"; }
};

class FileUtilitiesHostImpl : public blink::mojom::FileUtilitiesHost {
 public:
  static void Bind(mojo::ScopedMessagePipeHandle handle) {
    mojo::PendingReceiver<blink::mojom::FileUtilitiesHost> receiver(
        std::move(handle));
    mojo::MakeSelfOwnedReceiver(std::make_unique<FileUtilitiesHostImpl>(),
                                std::move(receiver));
  }

  void GetFileInfo(const base::FilePath& path,
                   GetFileInfoCallback callback) override {
    base::File::Info info;
    if (base::GetFileInfo(path, &info)) {
      std::move(callback).Run(info);
    } else {
      std::move(callback).Run(std::nullopt);
    }
  }
};

class FormDataBytesConsumerTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    file_factory_helper_ = std::make_unique<FileBackedBlobFactoryTestHelper>(
        GetFrame().GetDocument()->GetExecutionContext());

    GetFrame()
        .GetDocument()
        ->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(mojom::FileUtilitiesHost::Name_,
                             base::BindRepeating(&FileUtilitiesHostImpl::Bind));

    auto fake_blob_registry = std::make_unique<FakeBlobRegistry>(
        /*support_binary_blob_bodies=*/true);
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_blob_registry),
        blob_registry_remote_.BindNewPipeAndPassReceiver(),
        Platform::Current()->GetIOTaskRunner());
    BlobDataHandle::SetBlobRegistryForTesting(blob_registry_remote_.get());

    CHECK(scoped_temp_dir_.CreateUniqueTempDir());
  }
  void TearDown() override {
    BlobDataHandle::SetBlobRegistryForTesting(nullptr);
  }

  void AppendFile(scoped_refptr<EncodedFormData> data, String content) {
    base::FilePath file_path;
    CHECK(
        base::CreateTemporaryFileInDir(scoped_temp_dir_.GetPath(), &file_path));
    CHECK(base::WriteFile(file_path, content.Utf8()));
    String file_name = String::FromUTF8(file_path.AsUTF8Unsafe());
    data->AppendFile(file_name, std::nullopt);
  }

  String DrainAsString(scoped_refptr<EncodedFormData> input_form_data) {
    auto* consumer = MakeGarbageCollected<FormDataBytesConsumer>(
        GetFrame().DomWindow(), input_form_data);
    auto* reader = MakeGarbageCollected<BytesConsumerTestReader>(consumer);
    // Force to read in small chunk to test Begin/EndRead().
    reader->set_max_chunk_size(2u);
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
  mojo::Remote<mojom::blink::BlobRegistry> blob_registry_remote_;
  base::ScopedTempDir scoped_temp_dir_;
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
  expected.AppendSpan(base::span(kData));

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
  data->AppendData(base::span_from_cstring("foo"));
  data->AppendData(base::span_from_cstring("hoge"));

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
  auto* data = MakeGarbageCollected<FormData>(Utf8Encoding());
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
  auto* data = MakeGarbageCollected<FormData>(Utf8Encoding());
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

scoped_refptr<EncodedFormData> CreateDataWithBoundary() {
  scoped_refptr<EncodedFormData> data = EncodedFormData::Create();
  Vector<char> boundary;
  boundary.push_back('\0');
  data->SetBoundary(boundary);
  return data;
}

void AppendData(scoped_refptr<EncodedFormData> data, const String& content) {
  FormDataElement element;
  element.data_.AppendSpan(content.RawByteSpan());
  data->MutableElements().push_back(element);
}

TEST_F(FormDataBytesConsumerTest, Data2) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  AppendData(data, "foo");
  AppendData(data, " bar");
  EXPECT_EQ("foo bar", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, DataAndFile) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  data->AppendData(base::span_from_cstring("foo"));
  AppendFile(data, " hello world");
  EXPECT_EQ("foo hello world", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, Blob) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  data->AppendBlob(CreateBlobHandle("baz"));
  data->AppendBlob(CreateBlobHandle("bar"));
  EXPECT_EQ("bazbar", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, DataFileAndBlob) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  data->AppendData(base::span_from_cstring("foo"));
  AppendFile(data, " bar");
  data->AppendBlob(CreateBlobHandle(" baz"));
  EXPECT_EQ("foo bar baz", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, DataAndDataPipe) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  AppendData(data, "foo");
  AppendData(data, " bar");
  AppendDataPipe(data, " hello");
  AppendDataPipe(data, " world");
  EXPECT_EQ("foo bar hello world", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, DataAndDataPipeAsync) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  data->AppendData(base::span_from_cstring("foo"));
  mojo::PendingRemote<network::mojom::blink::DataPipeGetter> data_pipe_getter;
  // Object deletes itself.
  DataPipeGetterImpl* data_pipe =
      new DataPipeGetterImpl(data_pipe_getter.InitWithNewPipeAndPassReceiver());
  auto wrapped =
      base::MakeRefCounted<WrappedDataPipeGetter>(std::move(data_pipe_getter));
  data->AppendDataPipe(std::move(wrapped));

  data_pipe->SetReadCallback(base::BindLambdaForTesting([&]() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          data_pipe->Write(" hello");
          data_pipe->Write(" world");
          data_pipe->Done(0 /* OK */, 12u);
        }));
  }));

  EXPECT_EQ("foo hello world", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, DataPipeAndBlob) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  data->AppendData(base::span_from_cstring("foo"));
  AppendDataPipe(data, " hello world");
  data->AppendBlob(CreateBlobHandle("bar"));
  EXPECT_EQ("foo hello worldbar", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, BlobAndDataPipe) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  data->AppendData(base::span_from_cstring("foo"));
  data->AppendBlob(CreateBlobHandle("blob"));
  AppendDataPipe(data, " datapipe");
  EXPECT_EQ("fooblob datapipe", DrainAsString(data));
}

TEST_F(FormDataBytesConsumerTest, DataPipeAndFile) {
  scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
  AppendDataPipe(data, "foo");
  AppendFile(data, " bar");
  EXPECT_EQ("foo bar", DrainAsString(data));
}

// Any element type combination should be consumed properly.
TEST_F(FormDataBytesConsumerTest, Any) {
  FormDataElement::Type types[] = {
      FormDataElement::kData,
      FormDataElement::kEncodedFile,
      FormDataElement::kEncodedBlob,
      FormDataElement::kDataPipe,
  };

  auto append = base::BindRepeating(
      [](FormDataBytesConsumerTest* test, scoped_refptr<EncodedFormData> data,
         FormDataElement::Type type, String content) {
        switch (type) {
          case FormDataElement::kData:
            data->AppendData(content.RawByteSpan());
            break;
          case FormDataElement::kEncodedFile:
            test->AppendFile(data, content);
            break;
          case FormDataElement::kEncodedBlob:
            data->AppendBlob(CreateBlobHandle(content));
            break;
          case FormDataElement::kDataPipe:
            AppendDataPipe(data, content);
            break;
        }
      },
      base::Unretained(this));

  for (auto type1 : types) {
    for (auto type2 : types) {
      for (auto type3 : types) {
        scoped_refptr<EncodedFormData> data = CreateDataWithBoundary();
        append.Run(data, type1, "foo");
        append.Run(data, type2, " bar");
        append.Run(data, type3, " baz");
        EXPECT_EQ("foo bar baz", DrainAsString(data))
            << type1 << ", " << type2 << ", " << type3;
      }
    }
  }
}

}  // namespace
}  // namespace blink
