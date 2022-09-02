// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/core_ipcz.h"

#include <cstring>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/thunks.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::core {
namespace {

struct InvitationDetails {
  MojoPlatformProcessHandle process;
  MojoPlatformHandle handle;
  MojoInvitationTransportEndpoint endpoint;
};

// Basic smoke tests for the Mojo Core API as implemented over ipcz.
class CoreIpczTest : public test::MojoTestBase {
 public:
  const MojoSystemThunks2& mojo() const { return *mojo_; }
  const IpczAPI& ipcz() const { return GetIpczAPI(); }
  IpczHandle node() const { return GetIpczNode(); }

  CoreIpczTest() : CoreIpczTest(/*is_broker=*/true) {}

  enum { kForClient };
  explicit CoreIpczTest(decltype(kForClient))
      : CoreIpczTest(/*is_broker=*/false) {}

  ~CoreIpczTest() override { DestroyIpczNodeForProcess(); }

  MojoMessageHandle CreateMessage(base::StringPiece contents,
                                  base::span<MojoHandle> handles = {}) {
    MojoMessageHandle message;
    EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessage(nullptr, &message));

    void* buffer;
    uint32_t buffer_size;
    MojoAppendMessageDataOptions options = {.struct_size = sizeof(options)};
    options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo().AppendMessageData(message, contents.size(), handles.data(),
                                       handles.size(), &options, &buffer,
                                       &buffer_size));
    EXPECT_GE(buffer_size, contents.size());
    memcpy(buffer, contents.data(), contents.size());
    return message;
  }

  // Unwraps and re-wraps a Mojo shared buffer handle, extracting some of its
  // serialized details in the process.
  struct SharedBufferDetails {
    uint64_t size;
    MojoPlatformSharedMemoryRegionAccessMode mode;
  };
  SharedBufferDetails PeekSharedBuffer(MojoHandle& buffer) {
    SharedBufferDetails details;
    uint32_t num_platform_handles = 2;
    MojoPlatformHandle platform_handles[2];
    MojoSharedBufferGuid guid;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo().UnwrapPlatformSharedMemoryRegion(
                  buffer, nullptr, platform_handles, &num_platform_handles,
                  &details.size, &guid, &details.mode));
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo().WrapPlatformSharedMemoryRegion(
                  platform_handles, num_platform_handles, details.size, &guid,
                  details.mode, nullptr, &buffer));
    return details;
  }

  static void CreateAndShareInvitationTransport(MojoHandle pipe,
                                                const base::Process& process,
                                                InvitationDetails& details) {
    PlatformChannel channel;
    MojoHandle handle_for_client =
        WrapPlatformHandle(channel.TakeRemoteEndpoint().TakePlatformHandle())
            .release()
            .value();
    WriteMessageWithHandles(pipe, "", &handle_for_client, 1);

    details.process.struct_size = sizeof(details.process);
#if BUILDFLAG(IS_WIN)
    details.process.value =
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(process.Handle()));
#else
    details.process.value = static_cast<uint64_t>(process.Handle());
#endif

    details.handle.struct_size = sizeof(details.handle);
    PlatformHandle::ToMojoPlatformHandle(
        channel.TakeLocalEndpoint().TakePlatformHandle(), &details.handle);
    details.endpoint = {
        .struct_size = sizeof(details.endpoint),
        .type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
        .num_platform_handles = 1,
        .platform_handles = &details.handle,
    };
  }

  static void ReceiveInvitationTransport(MojoHandle pipe,
                                         InvitationDetails& details) {
    MojoHandle handle;
    ReadMessageWithHandles(pipe, &handle, 1);

    details.handle.struct_size = sizeof(details.handle);
    PlatformHandle::ToMojoPlatformHandle(
        UnwrapPlatformHandle(ScopedHandle(Handle(handle))), &details.handle);
    details.endpoint = {
        .struct_size = sizeof(details.endpoint),
        .type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
        .num_platform_handles = 1,
        .platform_handles = &details.handle,
    };
  }

  void WriteToMessagePipe(MojoHandle pipe, base::StringPiece contents) {
    MojoMessageHandle message = CreateMessage(contents);
    EXPECT_EQ(MOJO_RESULT_OK, mojo().WriteMessage(pipe, message, nullptr));
  }

  std::string ReadFromMessagePipe(MojoHandle pipe) {
    base::WaitableEvent ready;
    MojoHandle trap;
    auto handler = +[](const MojoTrapEvent* event) {
      if (event->result == MOJO_RESULT_OK) {
        reinterpret_cast<base::WaitableEvent*>(event->trigger_context)
            ->Signal();
      }
    };
    EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateTrap(handler, nullptr, &trap));
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo().AddTrigger(trap, pipe, MOJO_HANDLE_SIGNAL_READABLE,
                                MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                                reinterpret_cast<uintptr_t>(&ready), nullptr));
    const MojoResult result = mojo().ArmTrap(trap, nullptr, nullptr, nullptr);
    if (result == MOJO_RESULT_OK) {
      ready.Wait();
    }
    EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(trap));

    MojoMessageHandle message;
    EXPECT_EQ(MOJO_RESULT_OK, mojo().ReadMessage(pipe, nullptr, &message));
    EXPECT_NE(MOJO_MESSAGE_HANDLE_INVALID, message);

    void* buffer;
    uint32_t buffer_size;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo().GetMessageData(message, nullptr, &buffer, &buffer_size,
                                    nullptr, nullptr));

    std::string contents(static_cast<char*>(buffer), buffer_size);
    EXPECT_EQ(MOJO_RESULT_OK, mojo().DestroyMessage(message));
    return contents;
  }

 private:
  explicit CoreIpczTest(bool is_broker) {
    CHECK(InitializeIpczNodeForProcess({.is_broker = is_broker}));
  }

  const MojoSystemThunks2* const mojo_{GetMojoIpczImpl()};
};

// Watches a PlatformChannel endpoint handle for its peer's closure.
class ChannelPeerClosureListener {
 public:
  explicit ChannelPeerClosureListener(PlatformHandle handle)
      : transport_(base::MakeRefCounted<ipcz_driver::Transport>(
            ipcz_driver::Transport::kToBroker,
            PlatformChannelEndpoint(std::move(handle)))) {
    transport_->Activate(
        reinterpret_cast<uintptr_t>(this),
        [](IpczHandle self, const void*, size_t, const IpczDriverHandle*,
           size_t, IpczTransportActivityFlags flags, const void*) {
          reinterpret_cast<ChannelPeerClosureListener*>(self)->OnEvent(flags);
          return IPCZ_RESULT_OK;
        });
  }

  void WaitForPeerClosure() { disconnected_.Wait(); }

 private:
  void OnEvent(IpczTransportActivityFlags flags) {
    if (flags & IPCZ_TRANSPORT_ACTIVITY_ERROR) {
      transport_->Deactivate();
    } else if (flags & IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED) {
      disconnected_.Signal();
    }
  }

  base::WaitableEvent disconnected_;
  scoped_refptr<ipcz_driver::Transport> transport_;
};

class CoreIpczTestClient : public CoreIpczTest {
 public:
  CoreIpczTestClient() : CoreIpczTest(kForClient) {}
};

TEST_F(CoreIpczTest, Close) {
  // With ipcz-based Mojo Core, Mojo handles are ipcz handles. So Mojo Close()
  // forwards to ipcz Close().

  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node(), IPCZ_NO_FLAGS, nullptr, &a, &b));

  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_FALSE(status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_TRUE(status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(b));
}

TEST_F(CoreIpczTest, BasicMessageUsage) {
  MojoHandle a, b;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));

  constexpr base::StringPiece kMessage = "hellllooooo";
  MojoMessageHandle message = CreateMessage(kMessage, {&b, 1});

  void* buffer;
  uint32_t num_bytes;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            mojo().GetMessageData(message, nullptr, &buffer, &num_bytes,
                                  nullptr, nullptr));

  const MojoGetMessageDataOptions options = {
      .struct_size = sizeof(options),
      .flags = MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES,
  };
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().GetMessageData(message, &options, &buffer, &num_bytes,
                                  nullptr, nullptr));
  EXPECT_EQ(kMessage,
            base::StringPiece(static_cast<const char*>(buffer), num_bytes));

  b = MOJO_HANDLE_INVALID;
  uint32_t num_handles = 1;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().GetMessageData(message, nullptr, &buffer, &num_bytes, &b,
                                  &num_handles));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().DestroyMessage(message));

  MojoHandleSignalsState signals_state;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(0u,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(b));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));
}

TEST_F(CoreIpczTest, MessageDestruction) {
  MojoHandle a, b;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));

  constexpr base::StringPiece kMessage = "hellllooooo";
  MojoMessageHandle message = CreateMessage(kMessage, {&b, 1});

  // Destroying the message must also close the attached pipe.
  MojoHandleSignalsState signals_state;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(0u,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().DestroyMessage(message));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(a, &signals_state));
  EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            signals_state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));
}

TEST_F(CoreIpczTest, MessagePipes) {
  MojoHandle a, b;
  MojoHandle c, d;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &c, &d));

  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT, mojo().ReadMessage(a, nullptr, &message));

  constexpr base::StringPiece kMessage = "bazongo";
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().WriteMessage(a, CreateMessage(kMessage), nullptr));

  MojoHandleSignalsState state;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(b, &state));
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().FuseMessagePipes(b, c, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().QueryHandleSignalsState(d, &state));
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_FALSE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_TRUE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().ReadMessage(d, nullptr, &message));
  EXPECT_NE(MOJO_MESSAGE_HANDLE_INVALID, message);

  void* buffer;
  uint32_t buffer_size;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().GetMessageData(message, nullptr, &buffer, &buffer_size,
                                  nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));

  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo().WriteMessage(d, message, nullptr));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo().ReadMessage(d, nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(d));
}

TEST_F(CoreIpczTest, Traps) {
  MojoHandle a, b;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));

  // A simple trap event handler which treats its event context as a
  // MojoTrapEvent pointer, where the fired event will be copied.
  auto handler = [](const MojoTrapEvent* event) {
    *reinterpret_cast<MojoTrapEvent*>(event->trigger_context) = *event;
  };
  MojoHandle trap;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateTrap(handler, nullptr, &trap));

  // Initialize these events with an impossible result code.
  MojoTrapEvent readable_event = {.result = MOJO_RESULT_UNKNOWN};
  MojoTrapEvent writable_event = {.result = MOJO_RESULT_UNKNOWN};
  uintptr_t kReadableContext = reinterpret_cast<uintptr_t>(&readable_event);
  uintptr_t kWritableContext = reinterpret_cast<uintptr_t>(&writable_event);
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().AddTrigger(trap, b, MOJO_HANDLE_SIGNAL_READABLE,
                              MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                              kReadableContext, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().AddTrigger(trap, b, MOJO_HANDLE_SIGNAL_WRITABLE,
                              MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                              kWritableContext, nullptr));

  // Arming should fail because the pipe is always writable.
  uint32_t num_events = 1;
  MojoTrapEvent event = {.struct_size = sizeof(event)};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo().ArmTrap(trap, nullptr, &num_events, &event));
  EXPECT_EQ(kWritableContext, event.trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, event.result);

  // But we should be able to arm after removing that trigger. Trigger removal
  // should also notify the writable trigger of cancellation.
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().RemoveTrigger(trap, kWritableContext, nullptr));
  EXPECT_EQ(MOJO_RESULT_CANCELLED, writable_event.result);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().ArmTrap(trap, nullptr, nullptr, nullptr));

  // Making `b` readable by writing to `a` should immediately activate the
  // remaining trigger.
  EXPECT_EQ(MOJO_RESULT_UNKNOWN, readable_event.result);
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().WriteMessage(a, CreateMessage("lol"), nullptr));
  EXPECT_EQ(MOJO_RESULT_CANCELLED, writable_event.result);
  EXPECT_EQ(MOJO_RESULT_OK, readable_event.result);

  // Clear the pipe and re-arm the trap.
  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().ReadMessage(b, nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().DestroyMessage(message));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().ArmTrap(trap, nullptr, nullptr, nullptr));

  // Closing `a` should activate the readable trigger again, this time to signal
  // its permanent unsatisfiability.
  EXPECT_EQ(MOJO_RESULT_OK, readable_event.result);
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, readable_event.result);

  // Closing `b` itself should elicit one final cancellation event.
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(b));
  EXPECT_EQ(MOJO_RESULT_CANCELLED, readable_event.result);

  // Finally, closing the trap with an active trigger should also elicit a
  // cancellation event.
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateMessagePipe(nullptr, &a, &b));
  readable_event.result = MOJO_RESULT_UNKNOWN;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().AddTrigger(trap, b, MOJO_HANDLE_SIGNAL_READABLE,
                              MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                              kReadableContext, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(trap));
  EXPECT_EQ(MOJO_RESULT_CANCELLED, readable_event.result);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(b));
}

TEST_F(CoreIpczTest, WrapPlatformHandle) {
  PlatformChannel channel;

  // We can wrap and unwrap a handle intact.
  MojoHandle wrapped_handle;
  MojoPlatformHandle mojo_handle = {.struct_size = sizeof(mojo_handle)};
  PlatformHandle::ToMojoPlatformHandle(
      channel.TakeLocalEndpoint().TakePlatformHandle(), &mojo_handle);
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().WrapPlatformHandle(&mojo_handle, nullptr, &wrapped_handle));
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().UnwrapPlatformHandle(wrapped_handle, nullptr, &mojo_handle));

  ChannelPeerClosureListener listener(
      PlatformHandle::FromMojoPlatformHandle(&mojo_handle));

  // Closing a handle wrapper closes the underlying handle.
  PlatformHandle::ToMojoPlatformHandle(
      channel.TakeRemoteEndpoint().TakePlatformHandle(), &mojo_handle);
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().WrapPlatformHandle(&mojo_handle, nullptr, &wrapped_handle));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(wrapped_handle));

  listener.WaitForPeerClosure();
}

TEST_F(CoreIpczTest, BasicSharedBuffer) {
  const base::StringPiece kContents = "steamed hams";
  MojoHandle buffer;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().CreateSharedBuffer(kContents.size(), nullptr, &buffer));

  // New Mojo shared buffers are always writable by default.
  SharedBufferDetails details = PeekSharedBuffer(buffer);
  EXPECT_EQ(MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_WRITABLE,
            details.mode);
  EXPECT_EQ(kContents.size(), details.size);

  void* address;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().MapBuffer(buffer, 0, kContents.size(), nullptr, &address));
  memcpy(address, kContents.data(), kContents.size());
  EXPECT_EQ(MOJO_RESULT_OK, mojo().UnmapBuffer(address));
  address = nullptr;

  // We can duplicate to handle which can only be mapped for reading.
  MojoHandle readonly_buffer;
  const MojoDuplicateBufferHandleOptions readonly_options = {
      .struct_size = sizeof(readonly_options),
      .flags = MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY,
  };
  EXPECT_EQ(MOJO_RESULT_OK, mojo().DuplicateBufferHandle(
                                buffer, &readonly_options, &readonly_buffer));

  // With a read-only duplicate, it should now be impossible to create a
  // writable duplicate, and the original buffer handle should now be in
  // read-only mode.
  MojoHandle writable_buffer;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo().DuplicateBufferHandle(buffer, nullptr, &writable_buffer));

  details = PeekSharedBuffer(buffer);
  EXPECT_EQ(MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY,
            details.mode);
  EXPECT_EQ(kContents.size(), details.size);

  details = PeekSharedBuffer(readonly_buffer);
  EXPECT_EQ(MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_READ_ONLY,
            details.mode);
  EXPECT_EQ(kContents.size(), details.size);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(buffer));

  // Additional read-only duplicates are OK though.
  MojoHandle dupe;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().DuplicateBufferHandle(
                                readonly_buffer, &readonly_options, &dupe));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(dupe));

  // And finally we can map the buffer again to find the same contents.
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().MapBuffer(readonly_buffer, 0, kContents.size(), nullptr,
                             &address));
  EXPECT_EQ(kContents, base::StringPiece(static_cast<const char*>(address),
                                         kContents.size()));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(readonly_buffer));
}

TEST_F(CoreIpczTest, SharedBufferDuplicateUnsafe) {
  // A buffer which has been duplicated at least once without READ_ONLY can
  // never be duplicated as read-only.
  constexpr size_t kSize = 64;
  MojoHandle buffer;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().CreateSharedBuffer(kSize, nullptr, &buffer));

  MojoHandle dupe;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().DuplicateBufferHandle(buffer, nullptr, &dupe));

  SharedBufferDetails details = PeekSharedBuffer(buffer);
  EXPECT_EQ(MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_UNSAFE,
            details.mode);
  EXPECT_EQ(kSize, details.size);

  details = PeekSharedBuffer(dupe);
  EXPECT_EQ(MOJO_PLATFORM_SHARED_MEMORY_REGION_ACCESS_MODE_UNSAFE,
            details.mode);
  EXPECT_EQ(kSize, details.size);

  MojoHandle readonly_dupe;
  MojoDuplicateBufferHandleOptions options = {
      .struct_size = sizeof(options),
      .flags = MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY,
  };
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            mojo().DuplicateBufferHandle(buffer, &options, &readonly_dupe));

  // Unsafe duplication is still possible though.
  MojoHandle unsafe_dupe;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().DuplicateBufferHandle(buffer, nullptr, &unsafe_dupe));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(unsafe_dupe));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(dupe));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(buffer));
}

#if !BUILDFLAG(IS_IOS)

constexpr base::StringPiece kAttachmentName = "interesting pipe name";

constexpr base::StringPiece kTestMessages[] = {
    "hello hello",
    "i don't know why you say goodbye",
    "actually nvm i do",
    "lol bye",
};

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(InvitationSingleAttachmentClient,
                                  CoreIpczTestClient,
                                  h) {
  InvitationDetails details;
  ReceiveInvitationTransport(h, details);

  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().AcceptInvitation(&details.endpoint, nullptr, &invitation));

  MojoHandle new_pipe;
  EXPECT_EQ(MOJO_RESULT_OK, mojo().ExtractMessagePipeFromInvitation(
                                invitation, kAttachmentName.data(),
                                kAttachmentName.size(), nullptr, &new_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(invitation));

  WriteToMessagePipe(new_pipe, kTestMessages[3]);
  EXPECT_EQ(kTestMessages[0], ReadFromMessagePipe(new_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(new_pipe));
}

TEST_F(CoreIpczTest, InvitationSingleAttachment) {
  RunTestClientWithController(
      "InvitationSingleAttachmentClient", [&](ClientController& c) {
        InvitationDetails details;
        CreateAndShareInvitationTransport(c.pipe(), c.process(), details);

        MojoHandle new_pipe;
        MojoHandle invitation;
        EXPECT_EQ(MOJO_RESULT_OK,
                  mojo().CreateInvitation(nullptr, &invitation));
        EXPECT_EQ(MOJO_RESULT_OK,
                  mojo().AttachMessagePipeToInvitation(
                      invitation, kAttachmentName.data(),
                      kAttachmentName.size(), nullptr, &new_pipe));
        EXPECT_EQ(MOJO_RESULT_OK, mojo().SendInvitation(
                                      invitation, &details.process,
                                      &details.endpoint, nullptr, 0, nullptr));
        EXPECT_EQ(kTestMessages[3], ReadFromMessagePipe(new_pipe));
        WriteToMessagePipe(new_pipe, kTestMessages[0]);
        EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(new_pipe));
      });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(InvitationMultipleAttachmentsClient,
                                  CoreIpczTestClient,
                                  h) {
  InvitationDetails details;
  ReceiveInvitationTransport(h, details);

  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK,
            mojo().AcceptInvitation(&details.endpoint, nullptr, &invitation));

  for (uint32_t i = 0; i < std::size(kTestMessages); ++i) {
    MojoHandle pipe;
    EXPECT_EQ(MOJO_RESULT_OK, mojo().ExtractMessagePipeFromInvitation(
                                  invitation, &i, sizeof(i), nullptr, &pipe));
    WriteToMessagePipe(pipe, kTestMessages[i]);
    EXPECT_EQ(kTestMessages[i], ReadFromMessagePipe(pipe));
    EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(pipe));
  }
  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(invitation));
}

TEST_F(CoreIpczTest, InvitationMultipleAttachments) {
  RunTestClientWithController(
      "InvitationMultipleAttachmentsClient", [&](ClientController& c) {
        InvitationDetails details;
        CreateAndShareInvitationTransport(c.pipe(), c.process(), details);

        MojoHandle invitation;
        EXPECT_EQ(MOJO_RESULT_OK,
                  mojo().CreateInvitation(nullptr, &invitation));

        MojoHandle pipes[std::size(kTestMessages)];
        for (uint32_t i = 0; i < std::size(pipes); ++i) {
          EXPECT_EQ(MOJO_RESULT_OK,
                    mojo().AttachMessagePipeToInvitation(
                        invitation, &i, sizeof(i), nullptr, &pipes[i]));
        }
        EXPECT_EQ(MOJO_RESULT_OK, mojo().SendInvitation(
                                      invitation, &details.process,
                                      &details.endpoint, nullptr, 0, nullptr));

        for (size_t i = 0; i < std::size(pipes); ++i) {
          EXPECT_EQ(kTestMessages[i], ReadFromMessagePipe(pipes[i]));
          WriteToMessagePipe(pipes[i], kTestMessages[i]);
          EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(pipes[i]));
        }
      });
}

#endif  // !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace mojo::core
