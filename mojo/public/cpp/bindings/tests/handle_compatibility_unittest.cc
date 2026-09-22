// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/handle_compatibility_unittest.test-mojom.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test::handle_compatibility {
namespace {

PlatformHandle CreateTestPlatformHandleWithContent(
    const base::ScopedTempDir& temp_dir,
    std::string_view name,
    std::string_view content) {
  base::FilePath path = temp_dir.GetPath().AppendASCII(name);
  base::File file(path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                            base::File::FLAG_WRITE);
  EXPECT_TRUE(file.IsValid());
  EXPECT_TRUE(file.WriteAndCheck(0, base::as_byte_span(content)));
  return PlatformHandle(base::ScopedPlatformFile(file.TakePlatformFile()));
}

std::string ReadContentFromPlatformHandle(PlatformHandle handle) {
  if (!handle.is_valid_platform_file()) {
    ADD_FAILURE() << "Expected valid platform file handle";
    return std::string();
  }
  base::File file(handle.TakePlatformFile());
  if (!file.IsValid()) {
    ADD_FAILURE() << "Expected valid base::File";
    return std::string();
  }
  int64_t length = file.GetLength();
  if (length < 0) {
    ADD_FAILURE() << "Failed to get file length";
    return std::string();
  }
  std::string buffer(static_cast<size_t>(length), '\0');
  EXPECT_TRUE(file.ReadAndCheck(0, base::as_writable_byte_span(buffer)));
  return buffer;
}

class PlatformHandleInterfaceImpl : public mojom::PlatformHandleInterface {
 public:
  explicit PlatformHandleInterfaceImpl(
      PendingReceiver<mojom::PlatformHandleInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Echo(PlatformHandle h,
            PlatformHandle nullable_h,
            mojom::PlatformHandleStructPtr s,
            EchoCallback callback) override {
    if (expect_valid_handles_) {
      EXPECT_TRUE(h.is_valid());
      EXPECT_FALSE(nullable_h.is_valid());
      EXPECT_TRUE(s->h.is_valid());
      EXPECT_FALSE(s->nullable_h.is_valid());
      EXPECT_EQ(2u, s->handle_array.size());
      EXPECT_TRUE(s->handle_array[0].is_valid());
      EXPECT_TRUE(s->handle_array[1].is_valid());
      std::move(callback).Run(std::move(h), std::move(nullable_h),
                              std::move(s));
    } else {
      EXPECT_FALSE(h.is_valid());
      EXPECT_FALSE(nullable_h.is_valid());
      EXPECT_FALSE(s->h.is_valid());
      EXPECT_FALSE(s->nullable_h.is_valid());
      EXPECT_EQ(1u, s->handle_array.size());
      EXPECT_FALSE(s->handle_array[0].is_valid());
      receiver_.reset();
      if (on_received_invalid_handles_) {
        std::move(on_received_invalid_handles_).Run();
      }
    }
  }

  void set_expect_valid_handles(bool expect_valid) {
    expect_valid_handles_ = expect_valid;
  }

  void set_on_received_invalid_handles(base::OnceClosure callback) {
    on_received_invalid_handles_ = std::move(callback);
  }

 private:
  Receiver<mojom::PlatformHandleInterface> receiver_;
  bool expect_valid_handles_ = true;
  base::OnceClosure on_received_invalid_handles_;
};

class UntypedHandleInterfaceImpl : public mojom::UntypedHandleInterface {
 public:
  explicit UntypedHandleInterfaceImpl(
      PendingReceiver<mojom::UntypedHandleInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Echo(ScopedHandle h,
            ScopedHandle nullable_h,
            mojom::UntypedHandleStructPtr s,
            EchoCallback callback) override {
    EXPECT_TRUE(h.is_valid());
    EXPECT_FALSE(nullable_h.is_valid());
    EXPECT_TRUE(s->h.is_valid());
    EXPECT_FALSE(s->nullable_h.is_valid());
    EXPECT_EQ(2u, s->handle_array.size());
    EXPECT_TRUE(s->handle_array[0].is_valid());
    EXPECT_TRUE(s->handle_array[1].is_valid());

    std::move(callback).Run(std::move(h), std::move(nullable_h), std::move(s));
  }

 private:
  Receiver<mojom::UntypedHandleInterface> receiver_;
};

class SharedBufferHandleParamInterfaceImpl
    : public mojom::SharedBufferHandleParamInterface {
 public:
  explicit SharedBufferHandleParamInterfaceImpl(
      PendingReceiver<mojom::SharedBufferHandleParamInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Echo(ScopedSharedBufferHandle h, EchoCallback callback) override {
    EXPECT_TRUE(h.is_valid());
    std::move(callback).Run(std::move(h));
  }

 private:
  Receiver<mojom::SharedBufferHandleParamInterface> receiver_;
};

class MessagePipeHandleParamInterfaceImpl
    : public mojom::MessagePipeHandleParamInterface {
 public:
  explicit MessagePipeHandleParamInterfaceImpl(
      PendingReceiver<mojom::MessagePipeHandleParamInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Echo(ScopedMessagePipeHandle h, EchoCallback callback) override {
    EXPECT_TRUE(h.is_valid());
    std::move(callback).Run(std::move(h));
  }

 private:
  Receiver<mojom::MessagePipeHandleParamInterface> receiver_;
};

class DataPipeConsumerHandleParamInterfaceImpl
    : public mojom::DataPipeConsumerHandleParamInterface {
 public:
  explicit DataPipeConsumerHandleParamInterfaceImpl(
      PendingReceiver<mojom::DataPipeConsumerHandleParamInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Echo(ScopedDataPipeConsumerHandle h, EchoCallback callback) override {
    EXPECT_TRUE(h.is_valid());
    std::move(callback).Run(std::move(h));
  }

 private:
  Receiver<mojom::DataPipeConsumerHandleParamInterface> receiver_;
};

class DataPipeProducerHandleParamInterfaceImpl
    : public mojom::DataPipeProducerHandleParamInterface {
 public:
  explicit DataPipeProducerHandleParamInterfaceImpl(
      PendingReceiver<mojom::DataPipeProducerHandleParamInterface> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Echo(ScopedDataPipeProducerHandle h, EchoCallback callback) override {
    EXPECT_TRUE(h.is_valid());
    std::move(callback).Run(std::move(h));
  }

 private:
  Receiver<mojom::DataPipeProducerHandleParamInterface> receiver_;
};

using HandleCompatibilityTest = BindingsTestBase;

TEST_P(HandleCompatibilityTest, UntypedRemoteToPlatformReceiver) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  Remote<mojom::UntypedHandleInterface> untyped_remote;
  PendingReceiver<mojom::UntypedHandleInterface> untyped_receiver =
      untyped_remote.BindNewPipeAndPassReceiver();

  // Bind the underlying message pipe to a PlatformHandleInterface receiver.
  PlatformHandleInterfaceImpl impl(
      PendingReceiver<mojom::PlatformHandleInterface>(
          untyped_receiver.PassPipe()));

  ScopedHandle h = WrapPlatformHandle(CreateTestPlatformHandleWithContent(
      temp_dir, "param_handle", "param handle"));
  ScopedHandle struct_h =
      WrapPlatformHandle(CreateTestPlatformHandleWithContent(
          temp_dir, "struct_handle", "struct handle"));
  std::vector<ScopedHandle> handle_array;
  handle_array.push_back(WrapPlatformHandle(CreateTestPlatformHandleWithContent(
      temp_dir, "array_element_0", "array element 0 handle")));
  handle_array.push_back(WrapPlatformHandle(CreateTestPlatformHandleWithContent(
      temp_dir, "array_element_1", "array element 1 handle")));

  auto s = mojom::UntypedHandleStruct::New(std::move(struct_h), ScopedHandle(),
                                           std::move(handle_array));

  base::RunLoop loop;
  untyped_remote->Echo(
      std::move(h), ScopedHandle(), std::move(s),
      base::BindLambdaForTesting([&](ScopedHandle h_out,
                                     ScopedHandle nullable_h_out,
                                     mojom::UntypedHandleStructPtr s_out) {
        EXPECT_FALSE(nullable_h_out.is_valid());
        EXPECT_FALSE(s_out->nullable_h.is_valid());
        EXPECT_EQ("param handle", ReadContentFromPlatformHandle(
                                      UnwrapPlatformHandle(std::move(h_out))));
        EXPECT_EQ("struct handle",
                  ReadContentFromPlatformHandle(
                      UnwrapPlatformHandle(std::move(s_out->h))));
        ASSERT_EQ(2u, s_out->handle_array.size());
        EXPECT_EQ("array element 0 handle",
                  ReadContentFromPlatformHandle(
                      UnwrapPlatformHandle(std::move(s_out->handle_array[0]))));
        EXPECT_EQ("array element 1 handle",
                  ReadContentFromPlatformHandle(
                      UnwrapPlatformHandle(std::move(s_out->handle_array[1]))));
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(HandleCompatibilityTest, PlatformRemoteToUntypedReceiver) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  Remote<mojom::PlatformHandleInterface> platform_remote;
  PendingReceiver<mojom::PlatformHandleInterface> platform_receiver =
      platform_remote.BindNewPipeAndPassReceiver();

  // Bind the underlying message pipe to an UntypedHandleInterface receiver.
  UntypedHandleInterfaceImpl impl(
      PendingReceiver<mojom::UntypedHandleInterface>(
          platform_receiver.PassPipe()));

  PlatformHandle h = CreateTestPlatformHandleWithContent(
      temp_dir, "param_handle", "param handle");
  PlatformHandle struct_h = CreateTestPlatformHandleWithContent(
      temp_dir, "struct_handle", "struct handle");
  std::vector<PlatformHandle> handle_array;
  handle_array.push_back(CreateTestPlatformHandleWithContent(
      temp_dir, "array_element_0", "array element 0 handle"));
  handle_array.push_back(CreateTestPlatformHandleWithContent(
      temp_dir, "array_element_1", "array element 1 handle"));

  auto s = mojom::PlatformHandleStruct::New(
      std::move(struct_h), PlatformHandle(), std::move(handle_array));

  base::RunLoop loop;
  platform_remote->Echo(
      std::move(h), PlatformHandle(), std::move(s),
      base::BindLambdaForTesting([&](PlatformHandle h_out,
                                     PlatformHandle nullable_h_out,
                                     mojom::PlatformHandleStructPtr s_out) {
        EXPECT_FALSE(nullable_h_out.is_valid());
        EXPECT_FALSE(s_out->nullable_h.is_valid());
        EXPECT_EQ("param handle",
                  ReadContentFromPlatformHandle(std::move(h_out)));
        EXPECT_EQ("struct handle",
                  ReadContentFromPlatformHandle(std::move(s_out->h)));
        ASSERT_EQ(2u, s_out->handle_array.size());
        EXPECT_EQ(
            "array element 0 handle",
            ReadContentFromPlatformHandle(std::move(s_out->handle_array[0])));
        EXPECT_EQ(
            "array element 1 handle",
            ReadContentFromPlatformHandle(std::move(s_out->handle_array[1])));
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(HandleCompatibilityTest, NonPlatformHandleSafelyUnwrapsToInvalid) {
  Remote<mojom::UntypedHandleInterface> untyped_remote;
  PendingReceiver<mojom::UntypedHandleInterface> untyped_receiver =
      untyped_remote.BindNewPipeAndPassReceiver();

  PlatformHandleInterfaceImpl impl(
      PendingReceiver<mojom::PlatformHandleInterface>(
          untyped_receiver.PassPipe()));
  impl.set_expect_valid_handles(false);

  // Send message pipe handles wrapped in ScopedHandle instead of platform
  // handles. When PlatformHandleInterface deserializes them via
  // UnwrapPlatformHandle, they should safely unwrap to invalid PlatformHandles
  // rather than crashing or leaking.
  MessagePipe param_pipe;
  MessagePipe struct_field_pipe;
  MessagePipe nullable_param_pipe;
  MessagePipe struct_nullable_field_pipe;
  MessagePipe array_element_pipe;
  std::vector<ScopedHandle> handle_array;
  handle_array.push_back(
      ScopedHandle::From(std::move(array_element_pipe.handle0)));
  auto s = mojom::UntypedHandleStruct::New(
      ScopedHandle::From(std::move(struct_field_pipe.handle0)),
      ScopedHandle::From(std::move(struct_nullable_field_pipe.handle0)),
      std::move(handle_array));

  base::RunLoop loop;
  impl.set_on_received_invalid_handles(loop.QuitClosure());
  untyped_remote->Echo(
      ScopedHandle::From(std::move(param_pipe.handle0)),
      ScopedHandle::From(std::move(nullable_param_pipe.handle0)), std::move(s),
      base::DoNothing());
  loop.Run();

  // Verify that all non-platform MojoHandles were properly closed when
  // UnwrapPlatformHandle failed, rather than leaking.
  for (const auto* pipe :
       {&param_pipe, &struct_field_pipe, &nullable_param_pipe,
        &struct_nullable_field_pipe, &array_element_pipe}) {
    EXPECT_TRUE(pipe->handle1->QuerySignalsState().peer_closed());
  }
}

TEST_P(HandleCompatibilityTest, UntypedRemoteToSharedBufferReceiver) {
  Remote<mojom::UntypedHandleParamInterface> untyped_remote;
  PendingReceiver<mojom::UntypedHandleParamInterface> untyped_receiver =
      untyped_remote.BindNewPipeAndPassReceiver();

  // Bind the underlying message pipe to a SharedBufferHandleParamInterface
  // receiver, which echoes the handle back as a `handle<shared_buffer>`.
  SharedBufferHandleParamInterfaceImpl impl(
      PendingReceiver<mojom::SharedBufferHandleParamInterface>(
          untyped_receiver.PassPipe()));

  constexpr std::string_view kContent = "shared buffer handle";
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(kContent.size());
  ASSERT_TRUE(mapped_region.IsValid());
  mapped_region.mapping.GetMemoryAsSpan<char>().copy_from(kContent);
  ScopedSharedBufferHandle buffer =
      WrapReadOnlySharedMemoryRegion(std::move(mapped_region.region));
  ASSERT_TRUE(buffer.is_valid());

  base::RunLoop loop;
  untyped_remote->Echo(
      ScopedHandle::From(std::move(buffer)),
      base::BindLambdaForTesting([&](ScopedHandle h_out) {
        ScopedSharedBufferHandle buffer_out =
            ScopedSharedBufferHandle::From(std::move(h_out));
        ASSERT_TRUE(buffer_out.is_valid());
        base::ReadOnlySharedMemoryRegion region_out =
            UnwrapReadOnlySharedMemoryRegion(std::move(buffer_out));
        ASSERT_TRUE(region_out.IsValid());
        base::ReadOnlySharedMemoryMapping mapping = region_out.Map();
        ASSERT_TRUE(mapping.IsValid());
        EXPECT_EQ(kContent,
                  base::as_string_view(mapping.GetMemoryAsSpan<const char>()));
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(HandleCompatibilityTest, UntypedRemoteToMessagePipeReceiver) {
  Remote<mojom::UntypedHandleParamInterface> untyped_remote;
  PendingReceiver<mojom::UntypedHandleParamInterface> untyped_receiver =
      untyped_remote.BindNewPipeAndPassReceiver();

  // Bind the underlying message pipe to a MessagePipeHandleParamInterface
  // receiver, which echoes the handle back as a `handle<message_pipe>`.
  MessagePipeHandleParamInterfaceImpl impl(
      PendingReceiver<mojom::MessagePipeHandleParamInterface>(
          untyped_receiver.PassPipe()));

  // Queue a message on the peer before transferring the pipe, so the round trip
  // can be verified to preserve the pipe's identity rather than merely its
  // validity.
  constexpr std::string_view kMessage = "message pipe handle";
  MessagePipe pipe;
  ASSERT_EQ(MOJO_RESULT_OK, WriteMessageRaw(pipe.handle1.get(), kMessage.data(),
                                            kMessage.size(), nullptr, 0,
                                            MOJO_WRITE_MESSAGE_FLAG_NONE));

  base::RunLoop loop;
  untyped_remote->Echo(
      ScopedHandle::From(std::move(pipe.handle0)),
      base::BindLambdaForTesting([&](ScopedHandle h_out) {
        ScopedMessagePipeHandle pipe_out =
            ScopedMessagePipeHandle::From(std::move(h_out));
        ASSERT_TRUE(pipe_out.is_valid());
        ASSERT_EQ(MOJO_RESULT_OK,
                  Wait(pipe_out.get(), MOJO_HANDLE_SIGNAL_READABLE));
        std::vector<uint8_t> payload;
        ASSERT_EQ(MOJO_RESULT_OK,
                  ReadMessageRaw(pipe_out.get(), &payload, nullptr,
                                 MOJO_READ_MESSAGE_FLAG_NONE));
        EXPECT_EQ(kMessage, base::as_string_view(payload));
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(HandleCompatibilityTest, UntypedRemoteToDataPipeConsumerReceiver) {
  Remote<mojom::UntypedHandleParamInterface> untyped_remote;
  PendingReceiver<mojom::UntypedHandleParamInterface> untyped_receiver =
      untyped_remote.BindNewPipeAndPassReceiver();

  // Bind the underlying message pipe to a DataPipeConsumerHandleParamInterface
  // receiver, which echoes the handle back as a `handle<data_pipe_consumer>`.
  DataPipeConsumerHandleParamInterfaceImpl impl(
      PendingReceiver<mojom::DataPipeConsumerHandleParamInterface>(
          untyped_receiver.PassPipe()));

  constexpr std::string_view kContent = "data pipe consumer handle";
  ScopedDataPipeProducerHandle producer;
  ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(kContent.size(), producer, consumer));

  // Write the data before transferring the consumer, so the round trip can be
  // verified to preserve the pipe's identity rather than merely its validity.
  size_t bytes_written = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer->WriteData(base::as_byte_span(kContent),
                                MOJO_WRITE_DATA_FLAG_NONE, bytes_written));
  ASSERT_EQ(kContent.size(), bytes_written);

  base::RunLoop loop;
  untyped_remote->Echo(
      ScopedHandle::From(std::move(consumer)),
      base::BindLambdaForTesting([&](ScopedHandle h_out) {
        ScopedDataPipeConsumerHandle consumer_out =
            ScopedDataPipeConsumerHandle::From(std::move(h_out));
        ASSERT_TRUE(consumer_out.is_valid());
        ASSERT_EQ(MOJO_RESULT_OK,
                  Wait(consumer_out.get(), MOJO_HANDLE_SIGNAL_READABLE));
        std::string buffer(kContent.size(), '\0');
        size_t bytes_read = 0;
        ASSERT_EQ(MOJO_RESULT_OK,
                  consumer_out->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                         base::as_writable_byte_span(buffer),
                                         bytes_read));
        EXPECT_EQ(kContent, std::string_view(buffer).substr(0, bytes_read));
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(HandleCompatibilityTest, UntypedRemoteToDataPipeProducerReceiver) {
  Remote<mojom::UntypedHandleParamInterface> untyped_remote;
  PendingReceiver<mojom::UntypedHandleParamInterface> untyped_receiver =
      untyped_remote.BindNewPipeAndPassReceiver();

  // Bind the underlying message pipe to a DataPipeProducerHandleParamInterface
  // receiver, which echoes the handle back as a `handle<data_pipe_producer>`.
  DataPipeProducerHandleParamInterfaceImpl impl(
      PendingReceiver<mojom::DataPipeProducerHandleParamInterface>(
          untyped_receiver.PassPipe()));

  constexpr std::string_view kContent = "data pipe producer handle";
  ScopedDataPipeProducerHandle producer;
  ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(kContent.size(), producer, consumer));

  base::RunLoop loop;
  untyped_remote->Echo(
      ScopedHandle::From(std::move(producer)),
      base::BindLambdaForTesting([&](ScopedHandle h_out) {
        // Writing through the returned producer must be observable on the
        // consumer retained here, which verifies the pipe survived the round
        // trip intact.
        ScopedDataPipeProducerHandle producer_out =
            ScopedDataPipeProducerHandle::From(std::move(h_out));
        ASSERT_TRUE(producer_out.is_valid());
        size_t bytes_written = 0;
        ASSERT_EQ(
            MOJO_RESULT_OK,
            producer_out->WriteData(base::as_byte_span(kContent),
                                    MOJO_WRITE_DATA_FLAG_NONE, bytes_written));
        ASSERT_EQ(kContent.size(), bytes_written);
        loop.Quit();
      }));
  loop.Run();

  ASSERT_EQ(MOJO_RESULT_OK, Wait(consumer.get(), MOJO_HANDLE_SIGNAL_READABLE));
  std::string buffer(kContent.size(), '\0');
  size_t bytes_read = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(buffer), bytes_read));
  EXPECT_EQ(kContent, std::string_view(buffer).substr(0, bytes_read));
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(HandleCompatibilityTest);

}  // namespace
}  // namespace mojo::test::handle_compatibility
