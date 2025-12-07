// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/test/mojo_test_base.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace test {

MojoTestBase::MojoTestBase() = default;

MojoTestBase::~MojoTestBase() = default;

MojoTestBase::ClientController& MojoTestBase::StartClient(
    const std::string& client_name) {
  clients_.push_back(
      std::make_unique<ClientController>(client_name, this, launch_type_));
  return *clients_.back();
}

MojoTestBase::ClientController::ClientController(const std::string& client_name,
                                                 MojoTestBase* test,
                                                 LaunchType launch_type) {
#if BUILDFLAG(USE_BLINK)
  pipe_ = helper_.StartChild(client_name, launch_type);
#endif
}

MojoTestBase::ClientController::~ClientController() {
  CHECK(was_shutdown_)
      << "Test clients should be waited on explicitly with WaitForShutdown().";
}

int MojoTestBase::ClientController::WaitForShutdown() {
  was_shutdown_ = true;
#if BUILDFLAG(USE_BLINK)
  int retval = helper_.WaitForChildShutdown();
  return retval;
#else
  NOTREACHED();
#endif
}

// static
void MojoTestBase::CloseHandle(MojoHandle h) {
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

// static
void MojoTestBase::CreateMessagePipe(MojoHandle* p0, MojoHandle* p1) {
  MojoCreateMessagePipe(nullptr, p0, p1);
  CHECK_NE(*p0, MOJO_HANDLE_INVALID);
  CHECK_NE(*p1, MOJO_HANDLE_INVALID);
}

// static
void MojoTestBase::WriteMessageWithHandles(MojoHandle mp,
                                           const std::string& message,
                                           const MojoHandle* handles,
                                           uint32_t num_handles) {
  CHECK_EQ(WriteMessageRaw(MessagePipeHandle(mp), message.data(),
                           static_cast<uint32_t>(message.size()), handles,
                           num_handles, MOJO_WRITE_MESSAGE_FLAG_NONE),
           MOJO_RESULT_OK);
}

// static
void MojoTestBase::WriteMessage(MojoHandle mp, const std::string& message) {
  WriteMessageWithHandles(mp, message, nullptr, 0);
}

// static
std::string MojoTestBase::ReadMessageWithHandles(
    MojoHandle mp,
    MojoHandle* out_handles,
    uint32_t expected_num_handles) {
  for (;;) {
    CHECK_EQ(WaitForSignals(mp, MOJO_HANDLE_SIGNAL_READABLE), MOJO_RESULT_OK);

    std::vector<uint8_t> bytes;
    std::vector<ScopedHandle> handles;
    MojoResult result = ReadMessageRaw(MessagePipeHandle(mp), &bytes, &handles,
                                       MOJO_READ_MESSAGE_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // Spurious read signals are possible when MojoIpcz is used with lazily
      // serialized messages. Retry.
      continue;
    }

    CHECK_EQ(MOJO_RESULT_OK, result);
    CHECK_EQ(expected_num_handles, handles.size());
    for (size_t i = 0; i < handles.size(); ++i)
      out_handles[i] = handles[i].release().value();

    return std::string(bytes.begin(), bytes.end());
  }
}

// static
std::string MojoTestBase::ReadMessageWithOptionalHandle(MojoHandle mp,
                                                        MojoHandle* handle) {
  for (;;) {
    CHECK_EQ(WaitForSignals(mp, MOJO_HANDLE_SIGNAL_READABLE), MOJO_RESULT_OK);

    std::vector<uint8_t> bytes;
    std::vector<ScopedHandle> handles;
    MojoResult result = ReadMessageRaw(MessagePipeHandle(mp), &bytes, &handles,
                                       MOJO_READ_MESSAGE_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // Spurious read signals are possible when MojoIpcz is used with lazily
      // serialized messages. Retry.
      continue;
    }

    CHECK_EQ(MOJO_RESULT_OK, result);
    CHECK(handles.size() == 0 || handles.size() == 1);
    CHECK(handle);

    if (handles.size() == 1)
      *handle = handles[0].release().value();
    else
      *handle = MOJO_HANDLE_INVALID;

    return std::string(bytes.begin(), bytes.end());
  }
}

// static
std::string MojoTestBase::ReadMessage(MojoHandle mp) {
  return ReadMessageWithHandles(mp, nullptr, 0);
}

// static
void MojoTestBase::ReadMessage(MojoHandle mp, char* data, size_t num_bytes) {
  for (;;) {
    CHECK_EQ(WaitForSignals(mp, MOJO_HANDLE_SIGNAL_READABLE), MOJO_RESULT_OK);

    std::vector<uint8_t> bytes;
    std::vector<ScopedHandle> handles;
    MojoResult result = ReadMessageRaw(MessagePipeHandle(mp), &bytes, &handles,
                                       MOJO_READ_MESSAGE_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // Spurious read signals are possible when MojoIpcz is used with lazily
      // serialized messages. Retry.
      continue;
    }

    CHECK_EQ(MOJO_RESULT_OK, result);
    CHECK_EQ(0u, handles.size());
    CHECK_EQ(num_bytes, bytes.size());
    memcpy(data, bytes.data(), bytes.size());
  }
}

// static
void MojoTestBase::VerifyTransmission(MojoHandle source,
                                      MojoHandle dest,
                                      const std::string& message) {
  WriteMessage(source, message);

  // We don't use EXPECT_EQ; failures on really long messages make life hard.
  EXPECT_TRUE(message == ReadMessage(dest));
}

// static
void MojoTestBase::VerifyEcho(MojoHandle mp, const std::string& message) {
  VerifyTransmission(mp, mp, message);
}

// static
MojoHandle MojoTestBase::CreateBuffer(uint64_t size) {
  MojoHandle h;
  EXPECT_EQ(MojoCreateSharedBuffer(size, nullptr, &h), MOJO_RESULT_OK);
  return h;
}

// static
MojoHandle MojoTestBase::DuplicateBuffer(MojoHandle h, bool read_only) {
  MojoHandle new_handle;
  MojoDuplicateBufferHandleOptions options = {
      sizeof(MojoDuplicateBufferHandleOptions),
      MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_NONE};
  if (read_only)
    options.flags |= MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoDuplicateBufferHandle(h, &options, &new_handle));
  return new_handle;
}

// static
void MojoTestBase::WriteToBuffer(MojoHandle h,
                                 size_t offset,
                                 const std::string_view& s) {
  char* data;
  EXPECT_EQ(MOJO_RESULT_OK, MojoMapBuffer(h, offset, s.size(), nullptr,
                                          reinterpret_cast<void**>(&data)));
  memcpy(data, s.data(), s.size());
  EXPECT_EQ(MOJO_RESULT_OK, MojoUnmapBuffer(static_cast<void*>(data)));
}

// static
void MojoTestBase::ExpectBufferContents(MojoHandle h,
                                        size_t offset,
                                        const std::string_view& s) {
  char* data;
  EXPECT_EQ(MOJO_RESULT_OK, MojoMapBuffer(h, offset, s.size(), nullptr,
                                          reinterpret_cast<void**>(&data)));
  EXPECT_EQ(s, std::string_view(data, s.size()));
  EXPECT_EQ(MOJO_RESULT_OK, MojoUnmapBuffer(static_cast<void*>(data)));
}

// static
void MojoTestBase::CreateDataPipe(MojoHandle* p0,
                                  MojoHandle* p1,
                                  size_t capacity) {
  MojoCreateDataPipeOptions options;
  options.struct_size = static_cast<uint32_t>(sizeof(options));
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = static_cast<uint32_t>(capacity);

  MojoCreateDataPipe(&options, p0, p1);
  CHECK_NE(*p0, MOJO_HANDLE_INVALID);
  CHECK_NE(*p1, MOJO_HANDLE_INVALID);
}

// static
void MojoTestBase::WriteData(MojoHandle producer, const std::string& data) {
  CHECK_EQ(WaitForSignals(producer, MOJO_HANDLE_SIGNAL_WRITABLE),
           MOJO_RESULT_OK);
  uint32_t num_bytes = static_cast<uint32_t>(data.size());
  MojoWriteDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_WRITE_DATA_FLAG_ALL_OR_NONE;
  CHECK_EQ(MojoWriteData(producer, data.data(), &num_bytes, &options),
           MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, static_cast<uint32_t>(data.size()));
}

// static
std::string MojoTestBase::ReadData(MojoHandle consumer, size_t size) {
  CHECK_EQ(WaitForSignals(consumer, MOJO_HANDLE_SIGNAL_READABLE),
           MOJO_RESULT_OK);
  std::vector<char> buffer(size);
  uint32_t num_bytes = static_cast<uint32_t>(size);
  MojoReadDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  CHECK_EQ(MojoReadData(consumer, &options, buffer.data(), &num_bytes),
           MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, static_cast<uint32_t>(size));

  return std::string(buffer.begin(), buffer.end());
}

// static
MojoHandleSignalsState MojoTestBase::GetSignalsState(MojoHandle handle) {
  MojoHandleSignalsState signals_state;
  CHECK_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(handle, &signals_state));
  return signals_state;
}

// static
MojoResult MojoTestBase::WaitForSignals(MojoHandle handle,
                                        MojoHandleSignals signals,
                                        MojoTriggerCondition condition,
                                        MojoHandleSignalsState* state) {
  return Wait(Handle(handle), signals, condition, state);
}

// static
MojoResult MojoTestBase::WaitForSignals(MojoHandle handle,
                                        MojoHandleSignals signals,
                                        MojoHandleSignalsState* state) {
  return Wait(Handle(handle), signals, MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
              state);
}

// static
constexpr size_t MojoTestBase::kMaxMessageSizeInTests;

}  // namespace test
}  // namespace core
}  // namespace mojo
