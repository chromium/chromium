// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/transport.h"

#include <algorithm>
#include <cstring>
#include <queue>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_view_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/core/embedder/features.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/transmissible_platform_handle.h"
#include "mojo/core/ipcz_driver/wrapped_platform_handle.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace mojo::core::ipcz_driver {
namespace {

struct TestMessage {
  TestMessage() = default;
  explicit TestMessage(std::string_view str,
                       base::span<IpczDriverHandle> handles = {})
      : bytes(str.begin(), str.end()),
        handles(handles.begin(), handles.end()) {}

  std::string as_string() const { return std::string(std::from_range, bytes); }

  void Transmit(Transport& transmitter) {
    transmitter.Transmit(base::span(bytes), base::span(handles));
  }

  std::vector<uint8_t> bytes;
  std::vector<IpczDriverHandle> handles;
};

// These tests use Mojo and Mojo's existing multiprocess test facilities to set
// up a multiprocess environment and send an initial transport handle to the
// child process.
class MojoIpczTransportTest : public test::MojoTestBase {
 protected:
  // Creates a new ad hoc ipcz Transport object from a new PlatformChannel. One
  // end of the channel is returned as a Transport while the other is sent over
  // `pipe` to `process`.
  static scoped_refptr<Transport> CreateAndSendTransport(
      MojoHandle pipe,
      const base::Process& process,
#if BUILDFLAG(IS_WIN)
      Transport::ProcessTrust process_trust = Transport::ProcessTrust::kTrusted
#else
      // Parameter is not tracked on non-Windows platforms.
      Transport::ProcessTrust process_trust = Transport::ProcessTrust{}
#endif
  ) {
    PlatformChannel channel;
    MojoHandle transport_for_client =
        WrapPlatformHandle(channel.TakeRemoteEndpoint().TakePlatformHandle())
            .release()
            .value();
    WriteMessageWithHandles(pipe, "", &transport_for_client, 1);
    return Transport::Create(
        {.source = Transport::kBroker, .destination = Transport::kNonBroker},
        channel.TakeLocalEndpoint(), process.Duplicate(), process_trust);
  }

  // Retrieves a PlatformChannel endpoint from `pipe` and returns a newly
  // constructed Transport over it. By default the new Transport is a non-broker
  // endpoint connected to a broker.
  static scoped_refptr<Transport> ReceiveTransport(
      MojoHandle pipe,
      Transport::EndpointTypes endpoint_types =
          {.source = Transport::kNonBroker, .destination = Transport::kBroker},
      Transport::ProcessTrust process_trust = Transport::ProcessTrust{}) {
    MojoHandle transport_for_client;
    ReadMessageWithHandles(pipe, &transport_for_client, 1);
    PlatformHandle handle =
        UnwrapPlatformHandle(ScopedHandle(Handle(transport_for_client)));
    return Transport::Create(endpoint_types,
                             PlatformChannelEndpoint(std::move(handle)),
                             base::Process(), process_trust);
  }

  static TestMessage SerializeObjectFor(Transport& transmitter,
                                        scoped_refptr<ObjectBase> object) {
    size_t num_bytes = 0;
    size_t num_handles = 0;
    EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
              transmitter.SerializeObject(*object, nullptr, &num_bytes, nullptr,
                                          &num_handles));

    TestMessage message;
    message.bytes.resize(num_bytes);
    message.handles.resize(num_handles);
    EXPECT_EQ(IPCZ_RESULT_OK, transmitter.SerializeObject(
                                  *object, message.bytes.data(), &num_bytes,
                                  message.handles.data(), &num_handles));
    return message;
  }

  template <typename T>
  static scoped_refptr<T> DeserializeObjectFrom(Transport& receiver,
                                                const TestMessage& message) {
    scoped_refptr<ObjectBase> object;
    const IpczResult result = receiver.DeserializeObject(
        base::span(message.bytes), base::span(message.handles), object);
    CHECK_EQ(result, IPCZ_RESULT_OK);
    CHECK_EQ(object->type(), T::object_type());
    return base::WrapRefCounted(static_cast<T*>(object.get()));
  }

  static TestMessage SerializeFileFor(Transport& transmitter, base::File file) {
    auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
        PlatformHandle(base::ScopedPlatformFile(file.TakePlatformFile())));
    return SerializeObjectFor(transmitter, std::move(wrapper));
  }

  static base::File DeserializeFileFrom(Transport& receiver,
                                        const TestMessage& message) {
    scoped_refptr<WrappedPlatformHandle> wrapper =
        DeserializeObjectFrom<WrappedPlatformHandle>(receiver, message);
    CHECK(wrapper);
#if BUILDFLAG(IS_WIN)
    return base::File(wrapper->TakeHandle().TakeHandle());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    return base::File(wrapper->TakeHandle().TakeFD());
#endif
  }

  static TestMessage SerializeRegionFor(Transport& transmitter,
                                        base::UnsafeSharedMemoryRegion region) {
    auto handle = base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
        std::move(region));
    return SerializeObjectFor(
        transmitter, base::MakeRefCounted<SharedBuffer>(std::move(handle)));
  }

  base::UnsafeSharedMemoryRegion BufferObjectToRegion(
      scoped_refptr<SharedBuffer> buffer) {
    return base::UnsafeSharedMemoryRegion::Deserialize(
        std::move(buffer->region()));
  }
};

// TransportListener provides a convenient way for tests to listen to incoming
// events on a Transport.
class TransportListener {
 public:
  explicit TransportListener(Transport& transport) : transport_(transport) {
    transport_->Activate(reinterpret_cast<IpczHandle>(this),
                         &TransportListener::OnActivity);
  }

  ~TransportListener() {
    transport_->Deactivate();
    deactivation_event_.Wait();
  }

  TestMessage WaitForNextMessage() {
    base::AutoLock lock(lock_);
    while (messages_.empty()) {
      have_messages_.Wait();
    }

    TestMessage message = std::move(messages_.front());
    messages_.pop();
    return message;
  }

  void WaitForDisconnect() { disconnect_event_.Wait(); }

 private:
  static IpczResult OnActivity(IpczHandle transport,
                               const void* data,
                               size_t num_bytes,
                               const IpczDriverHandle* handles,
                               size_t num_handles,
                               IpczTransportActivityFlags flags,
                               const struct IpczTransportActivityOptions*) {
    auto* listener = reinterpret_cast<TransportListener*>(transport);
    // SAFETY: `data` and `handles` originate from the Ipcz driver callback,
    // which guarantees they point to at least `num_bytes` and `num_handles`
    // elements, respectively.
    UNSAFE_BUFFERS({
      auto bytes = base::span(static_cast<const uint8_t*>(data), num_bytes);
      listener->HandleActivity(bytes, base::span(handles, num_handles), flags);
    });
    return IPCZ_RESULT_OK;
  }

  void HandleActivity(base::span<const uint8_t> bytes,
                      base::span<const IpczDriverHandle> handles,
                      IpczTransportActivityFlags flags) {
    if (flags & IPCZ_TRANSPORT_ACTIVITY_ERROR) {
      disconnect_event_.Signal();
      return;
    }

    if (flags & IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED) {
      deactivation_event_.Signal();
      return;
    }

    TestMessage message;
    message.bytes = base::ToVector(bytes);
    message.handles = base::ToVector(handles);

    base::AutoLock lock(lock_);
    messages_.push(std::move(message));
    have_messages_.Signal();
  }

  const raw_ref<Transport> transport_;

  base::Lock lock_;
  base::ConditionVariable have_messages_{&lock_};
  std::queue<TestMessage> messages_ GUARDED_BY(lock_);
  base::WaitableEvent disconnect_event_;
  base::WaitableEvent deactivation_event_;
};

constexpr std::string_view kMessage1 = "we are messages";
constexpr std::string_view kMessage2 = "tremendous messages";
constexpr std::string_view kMessage3 = "the very best messages";
constexpr std::string_view kMessage4 = "everyone says so";

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(BasicTransmitClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);
  TestMessage(kMessage3).Transmit(*transport);
  TestMessage(kMessage4).Transmit(*transport);
  EXPECT_EQ(kMessage1, listener.WaitForNextMessage().as_string());
  EXPECT_EQ(kMessage2, listener.WaitForNextMessage().as_string());
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, BasicTransmit) {
  RunTestClientWithController("BasicTransmitClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process());

    TransportListener listener(*transport);
    TestMessage(kMessage1).Transmit(*transport);
    TestMessage(kMessage2).Transmit(*transport);
    EXPECT_EQ(kMessage3, listener.WaitForNextMessage().as_string());
    EXPECT_EQ(kMessage4, listener.WaitForNextMessage().as_string());
    listener.WaitForDisconnect();
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(MalformedObjectsClient,
                                  MojoIpczTransportTest,
                                  h) {
  // Offsets of enums that should be validated on receipt. Serialized objects
  // use types internal to transport.cc e.g. [ObjectHeader][TransportHeader]...
  // so supply direct offsets here.

  // offsetof(ObjectHeader, type).
  constexpr size_t object_type_offset = 4;
  // offsetof(TransportHeader, destination_type) + sizeof(ObjectHeader)
#if BUILDFLAG(IS_WIN)
  // offsetof(TransportHeader, destination_type) + sizeof(ObjectHeader)
  constexpr size_t transport_destination_type_offset = 0x18;
  // offsetof(BufferHeader, mode) + sizeof(ObjectHeader)
  constexpr size_t shared_bufffer_mode_offset = 0x20;
  // offsetof(WrappedPlatformHandleHeader, type) + sizeof(ObjectHeader)
  constexpr size_t wrapped_platform_type_offset = 0x1c;
#else
  constexpr size_t transport_destination_type_offset = 0x08;
  constexpr size_t shared_bufffer_mode_offset = 0x10;
  constexpr size_t wrapped_platform_type_offset = 0x0c;
#endif

  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);
  EXPECT_EQ("ready", listener.WaitForNextMessage().as_string());

  {
    auto [our_new_transport, their_new_transport] =
        Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);

    TestMessage msg =
        SerializeObjectFor(*transport, std::move(their_new_transport));
    // Peek into the message to break the encoded object type by using an out
    // of range enum value. This is uint32_t sized.
    msg.bytes[object_type_offset] = 22;
    msg.Transmit(*transport);

    EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
  }

  {
    auto [our_new_transport, their_new_transport] =
        Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);

    TestMessage msg =
        SerializeObjectFor(*transport, std::move(their_new_transport));
    // Peek into the message to break the encoded transport type by using an out
    // of range enum value. This is uint8_t sized.
    msg.bytes[transport_destination_type_offset] = 22;
    msg.Transmit(*transport);

    EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
  }

  {
    auto shared_buffer = SharedBuffer::MakeForRegion(
        base::UnsafeSharedMemoryRegion::Create(128));
    TestMessage msg = SerializeObjectFor(*transport, std::move(shared_buffer));
    // Peek into the message to break the encoded mode.
    msg.bytes[shared_bufffer_mode_offset] = 22;
    msg.Transmit(*transport);
    EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
  }

  {
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    base::File read_only_file = base::File(
        temp_dir.GetPath().AppendASCII("testfile-for-malformed-object"),
        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(PlatformHandle(
        base::ScopedPlatformFile(read_only_file.TakePlatformFile())));
    TestMessage msg = SerializeObjectFor(*transport, std::move(wrapper));
    // Peek into the message to break the encoded wrapper type.
    msg.bytes[wrapped_platform_type_offset] = 22;
    msg.Transmit(*transport);
    EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
  }

  TestMessage("done").Transmit(*transport);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, MalformedObjects) {
  RunTestClientWithController(
      "MalformedObjectsClient", [&](ClientController& c) {
        scoped_refptr<Transport> transport =
            CreateAndSendTransport(c.pipe(), c.process());

        TransportListener listener(*transport);
        TestMessage("ready").Transmit(*transport);

        {
          // Object type is invalid so the object should be rejected.
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<ObjectBase> object;
          const IpczResult result = transport->DeserializeObject(
              base::span(message.bytes), base::span(message.handles), object);
          EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
#if !BUILDFLAG(IS_WIN)
          // Adopt and free memory tracking this handle, as DeserializeObject
          // does not get far enough in to do so itself - this is ok to fake up
          // in this test as it validates that invalid messages are rejected.
          TransmissiblePlatformHandle::TakeFromHandle(message.handles[0]);
#endif  // !BUILDFLAG(IS_WIN)
          TestMessage("got null").Transmit(*transport);
        }

        {
          // Transport type is invalid so the object should be rejected.
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<ObjectBase> object;
          const IpczResult result = transport->DeserializeObject(
              base::span(message.bytes), base::span(message.handles), object);
          EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
          TestMessage("got null").Transmit(*transport);
        }

        {
          // Shared memory mode is invalid so the object should be rejected.
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<ObjectBase> object;
          const IpczResult result = transport->DeserializeObject(
              base::span(message.bytes), base::span(message.handles), object);
          EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
          TestMessage("got null").Transmit(*transport);
        }

        {
          // Wrapped platform handle type is invalid so the object should be
          // rejected.
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<ObjectBase> object;
          const IpczResult result = transport->DeserializeObject(
              base::span(message.bytes), base::span(message.handles), object);
          EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
          TestMessage("got null").Transmit(*transport);
        }

        EXPECT_EQ("done", listener.WaitForNextMessage().as_string());
        listener.WaitForDisconnect();
      });
}

// Transport on Windows does not support out-of-band handle transfer, so this
// test is impossible there. Windows handle transmission is instead covered by
// tests which more broadly cover driver object serialization.
#if !BUILDFLAG(IS_WIN)
IpczDriverHandle MakeHandleFromEndpoint(PlatformChannelEndpoint endpoint) {
  return TransmissiblePlatformHandle::ReleaseAsHandle(
      base::MakeRefCounted<TransmissiblePlatformHandle>(
          endpoint.TakePlatformHandle()));
}

scoped_refptr<Transport> MakeTransportFromMessage(const TestMessage& message) {
  CHECK_EQ(message.handles.size(), 1u);
  auto handle = TransmissiblePlatformHandle::TakeFromHandle(message.handles[0]);
  CHECK(handle);
  return Transport::Create(
      {.source = Transport::kNonBroker, .destination = Transport::kBroker},
      PlatformChannelEndpoint(handle->TakeHandle()));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitHandleClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  scoped_refptr<Transport> new_transport1;
  scoped_refptr<Transport> new_transport2;
  {
    TransportListener listener(*transport);
    new_transport1 = MakeTransportFromMessage(listener.WaitForNextMessage());
    new_transport2 = MakeTransportFromMessage(listener.WaitForNextMessage());
  }

  TransportListener listener1(*new_transport1);
  TransportListener listener2(*new_transport2);
  TestMessage(kMessage3).Transmit(*new_transport1);
  TestMessage(kMessage4).Transmit(*new_transport2);
  EXPECT_EQ(kMessage1, listener1.WaitForNextMessage().as_string());
  EXPECT_EQ(kMessage2, listener2.WaitForNextMessage().as_string());
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, TransmitHandle) {
  RunTestClientWithController("TransmitHandleClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process());

    // The PlatformHandle backing a PlatformChannelEndpoint is already
    // transmissible on all applicable platforms, so we can conveniently test
    // handle transmission without depending on driver object serialization.
    PlatformChannel channel1;
    auto new_transport1 = Transport::Create(
        {.source = Transport::kBroker, .destination = Transport::kNonBroker},
        channel1.TakeLocalEndpoint(), c.process().Duplicate());

    PlatformChannel channel2;
    auto new_transport2 = Transport::Create(
        {.source = Transport::kBroker, .destination = Transport::kNonBroker},
        channel2.TakeLocalEndpoint(), c.process().Duplicate());

    IpczDriverHandle handle1 =
        MakeHandleFromEndpoint(channel1.TakeRemoteEndpoint());
    IpczDriverHandle handle2 =
        MakeHandleFromEndpoint(channel2.TakeRemoteEndpoint());
    {
      TransportListener listener(*transport);
      TestMessage("!", base::span_from_ref(handle1)).Transmit(*transport);
      TestMessage("!", base::span_from_ref(handle2)).Transmit(*transport);
      listener.WaitForDisconnect();
    }

    TransportListener listener1(*new_transport1);
    TransportListener listener2(*new_transport2);
    TestMessage(kMessage1).Transmit(*new_transport1);
    TestMessage(kMessage2).Transmit(*new_transport2);
    EXPECT_EQ(kMessage3, listener1.WaitForNextMessage().as_string());
    EXPECT_EQ(kMessage4, listener2.WaitForNextMessage().as_string());
    listener1.WaitForDisconnect();
    listener2.WaitForDisconnect();
  });
}
#endif  // !BUILDFLAG(IS_WIN)

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitSerializedTransportClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  scoped_refptr<Transport> new_transport;
  {
    TransportListener listener(*transport);
    new_transport = DeserializeObjectFrom<Transport>(
        *transport, listener.WaitForNextMessage());
  }
  TransportListener listener(*new_transport);
  TestMessage(kMessage3).Transmit(*new_transport);
  TestMessage(kMessage4).Transmit(*new_transport);
  EXPECT_EQ(kMessage1, listener.WaitForNextMessage().as_string());
  EXPECT_EQ(kMessage2, listener.WaitForNextMessage().as_string());
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, TransmitSerializedTransport) {
  RunTestClientWithController(
      "TransmitSerializedTransportClient", [&](ClientController& c) {
        scoped_refptr<Transport> transport =
            CreateAndSendTransport(c.pipe(), c.process());

        auto [our_new_transport, their_new_transport] =
            Transport::CreatePair(Transport::kBroker, Transport::kNonBroker);
        {
          TransportListener listener(*transport);
          SerializeObjectFor(*transport, std::move(their_new_transport))
              .Transmit(*transport);
          listener.WaitForDisconnect();
        }

        TransportListener listener(*our_new_transport);
        TestMessage(kMessage1).Transmit(*our_new_transport);
        TestMessage(kMessage2).Transmit(*our_new_transport);
        EXPECT_EQ(kMessage3, listener.WaitForNextMessage().as_string());
        EXPECT_EQ(kMessage4, listener.WaitForNextMessage().as_string());
        listener.WaitForDisconnect();
      });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitFileClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);
  base::File file =
      DeserializeFileFrom(*transport, listener.WaitForNextMessage());

  std::vector<uint8_t> data(file.GetLength());
  ASSERT_TRUE(file.ReadAndCheck(0, data));
  EXPECT_EQ(kMessage1, base::as_string_view(data));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

class MojoIpczTransportSecurityTest
    : public MojoIpczTransportTest,
      public ::testing::WithParamInterface<
          std::tuple</*enforcement_enabled=*/bool,
                     /*add_no_execute_flags=*/bool>> {
 protected:
  bool IsEnforcementEnabled() {
// Enforcement only happens on Windows.
#if BUILDFLAG(IS_WIN)
    return std::get<0>(GetParam());
#else
    return false;
#endif
  }
  Transport::ProcessTrust TransportProcessTrust() {
// Enforcement only happens on Windows.
#if BUILDFLAG(IS_WIN)
    return IsEnforcementEnabled() ? Transport::ProcessTrust::kUntrusted
                                  : Transport::ProcessTrust::kTrusted;
#else
    return Transport::ProcessTrust::kUntracked;
#endif
  }
  bool ShouldMarkNoExecute() { return std::get<1>(GetParam()); }
};

TEST_P(MojoIpczTransportSecurityTest, TransmitFile) {
  RunTestClientWithController("TransmitFileClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process(), TransportProcessTrust());
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    int32_t flags = base::File::FLAG_CREATE | base::File::FLAG_READ |
                    base::File::FLAG_WRITE;
    if (ShouldMarkNoExecute()) {
      flags = base::File::AddFlagsForPassingToUntrustedProcess(flags);
    }
    base::File new_file(temp_dir.GetPath().AppendASCII("testfile"), flags);
    ASSERT_TRUE(new_file.WriteAndCheck(0, base::as_byte_span(kMessage1)));

    TransportListener listener(*transport);
    if (IsEnforcementEnabled() && !ShouldMarkNoExecute()) {
      EXPECT_DCHECK_DEATH_WITH(
          {
            SerializeFileFor(*transport, std::move(new_file))
                .Transmit(*transport);
          },
          "Transfer of writable handle to executable file to an untrusted "
          "process");
      // In this case, the message was never sent, because either DCHECK was
      // disabled so SerializeFileFor was never called, or the transport crashed
      // the process. In either case, the client is sitting there waiting for a
      // file to arrive, so send a read-only version to complete the test.
      base::File read_only_file =
          base::File(temp_dir.GetPath().AppendASCII("testfile"),
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
      SerializeFileFor(*transport, std::move(read_only_file))
          .Transmit(*transport);
    } else {
      SerializeFileFor(*transport, std::move(new_file)).Transmit(*transport);
    }
    listener.WaitForDisconnect();
  });
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MojoIpczTransportSecurityTest,
    testing::Combine(/*enforcement_enabled=*/testing::Bool(),
                     /*add_no_execute_flags=*/testing::Bool()));

constexpr std::string_view kMemoryMessage = "mojo wuz here";

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitMemoryClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  TransportListener listener(*transport);
  const TestMessage message = listener.WaitForNextMessage();
  auto region = base::UnsafeSharedMemoryRegion::Deserialize(std::move(
      DeserializeObjectFrom<SharedBuffer>(*transport, message)->region()));
  EXPECT_EQ(kMemoryMessage.size(), region.GetSize());
  auto mapping = region.Map();
  auto contents = std::string_view(static_cast<const char*>(mapping.memory()),
                                   kMemoryMessage.size());
  EXPECT_EQ(kMemoryMessage, contents);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, TransmitMemory) {
  RunTestClientWithController("TransmitMemoryClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process());

    auto region = base::UnsafeSharedMemoryRegion::Create(kMemoryMessage.size());
    auto mapping = region.Map();
    mapping.GetMemoryAsSpan<char>(kMemoryMessage.size())
        .copy_from(kMemoryMessage);
    auto buffer = SharedBuffer::MakeForRegion(std::move(region));

    TransportListener listener(*transport);
    SerializeObjectFor(*transport, std::move(buffer)).Transmit(*transport);
    listener.WaitForDisconnect();
  });
}

#if BUILDFLAG(IS_WIN)
constexpr std::string_view kGotInvalid = "got an invalid handle as expected";
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(InvalidHandleClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);
  // Arbitrary handle value (simulate a closed handle).
  {
    TestMessage message = listener.WaitForNextMessage();
    scoped_refptr<ObjectBase> object;
    // We nerfed the handle between serialization and sending so this fails.
    const IpczResult result = transport->DeserializeObject(
        base::span(message.bytes), base::span(message.handles), object);
    EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
    TestMessage(kGotInvalid).Transmit(*transport);
  }
  // Zero value.
  {
    TestMessage message = listener.WaitForNextMessage();
    scoped_refptr<ObjectBase> object;
    const IpczResult result = transport->DeserializeObject(
        base::span(message.bytes), base::span(message.handles), object);
    EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
    TestMessage(kGotInvalid).Transmit(*transport);
  }
  // GetCurrentThread() pseudo handle value.
  {
    TestMessage message = listener.WaitForNextMessage();
    scoped_refptr<ObjectBase> object;
    const IpczResult result = transport->DeserializeObject(
        base::span(message.bytes), base::span(message.handles), object);
    EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
    TestMessage(kGotInvalid).Transmit(*transport);
  }

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, InvalidHandle) {
  RunTestClientWithController("InvalidHandleClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport =
        CreateAndSendTransport(c.pipe(), c.process());

    TransportListener listener(*transport);
    {
      auto region = base::UnsafeSharedMemoryRegion::Create(kGotInvalid.size());
      auto fake_buffer = SharedBuffer::MakeForRegion(std::move(region));
      size_t num_bytes = 0;
      size_t num_handles = 0;
      TestMessage message;
      message.handles.resize(num_handles);
      EXPECT_EQ(IPCZ_RESULT_RESOURCE_EXHAUSTED,
                transport->SerializeObject(*fake_buffer, message.bytes.data(),
                                           &num_bytes, message.handles.data(),
                                           &num_handles));
      message.bytes.resize(num_bytes);
      EXPECT_EQ(IPCZ_RESULT_OK,
                transport->SerializeObject(*fake_buffer, message.bytes.data(),
                                           &num_bytes, message.handles.data(),
                                           &num_handles));
      // Nerf the handle to a value that could be a handle.
      uint32_t fake_handle = 0x12345678u;
      base::span(message.bytes)
          .subspan(Transport::FirstHandleOffsetForTesting())
          .first<sizeof(uint32_t)>()
          .copy_from(base::byte_span_from_ref(fake_handle));
      // Also close the region in the parent.
      ::CloseHandle(fake_buffer->region().GetPlatformHandle());
      message.Transmit(*transport);
      EXPECT_EQ(kGotInvalid, listener.WaitForNextMessage().as_string());
    }
    // Send null.
    {
      base::win::ScopedHandle handle(
          ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
      auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
          PlatformHandle(std::move(handle)));
      TestMessage message = SerializeObjectFor(*transport, std::move(wrapper));
      // Nerf to nullptr.
      uint64_t fake_handle = 0;
      base::span(message.bytes)
          .subspan(Transport::FirstHandleOffsetForTesting())
          .first<sizeof(uint64_t)>()
          .copy_from(base::byte_span_from_ref(fake_handle));
      message.Transmit(*transport);
      EXPECT_EQ(kGotInvalid, listener.WaitForNextMessage().as_string());
    }
    // Send pseudothread.
    {
      base::win::ScopedHandle handle(
          ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
      auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
          PlatformHandle(std::move(handle)));
      TestMessage message = SerializeObjectFor(*transport, std::move(wrapper));
      // Nerf to nullptr.
      uint64_t fake_handle = 0xfffffffffffffffe;
      base::span(message.bytes)
          .subspan(Transport::FirstHandleOffsetForTesting())
          .first<sizeof(uint64_t)>()
          .copy_from(base::byte_span_from_ref(fake_handle));
      message.Transmit(*transport);
      EXPECT_EQ(kGotInvalid, listener.WaitForNextMessage().as_string());
    }

    listener.WaitForDisconnect();
  });
}

constexpr std::string_view kFromUntrusted = "from untrusted";
constexpr std::string_view kFromTrusted = "from trusted";
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(InvalidHandleUntrustedClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);

  // Send pseudothread.
  {
    EXPECT_EQ(kFromTrusted, listener.WaitForNextMessage().as_string());
    base::win::ScopedHandle handle(
        ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
    auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
        PlatformHandle(std::move(handle)));
    TestMessage message = SerializeObjectFor(*transport, std::move(wrapper));
    // Nerf to nullptr.
    uint64_t fake_handle = 0xfffffffffffffffe;
    base::span(message.bytes)
        .subspan(Transport::FirstHandleOffsetForTesting())
        .first<sizeof(uint64_t)>()
        .copy_from(base::as_bytes(base::span_from_ref(fake_handle)));
    message.Transmit(*transport);
  }

  EXPECT_EQ(kGotInvalid, listener.WaitForNextMessage().as_string());
  TestMessage(kFromUntrusted).Transmit(*transport);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, InvalidHandleUntrusted) {
  RunTestClientWithController(
      "InvalidHandleUntrustedClient", [&](ClientController& c) {
        scoped_refptr<Transport> transport = CreateAndSendTransport(
            c.pipe(), c.process(), Transport::ProcessTrust{});

        TransportListener listener(*transport);
        TestMessage(kFromTrusted).Transmit(*transport);
        // GetCurrentThread() pseudo handle value.
        {
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<ObjectBase> object;
          const IpczResult result = transport->DeserializeObject(
              base::span(message.bytes), base::span(message.handles), object);
          EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
          TestMessage(kGotInvalid).Transmit(*transport);
        }

        EXPECT_EQ(kFromUntrusted, listener.WaitForNextMessage().as_string());
        listener.WaitForDisconnect();
      });
}

// Substitutes `local_handle` for the first handle encoded in `message` and
// verifies that `transport` refuses to deserialize the result. `message` must
// have been encoded with HandleOwner::kRecipient. On return `local_handle` must
// still be owned solely by the caller.
void ExpectRecipientOwnedHandleRejected(Transport& transport,
                                        TestMessage message,
                                        HANDLE local_handle) {
  const uint64_t value =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(local_handle));
  base::span(message.bytes)
      .subspan(Transport::FirstHandleOffsetForTesting())
      .first<sizeof(uint64_t)>()
      .copy_from(base::byte_span_from_ref(value));

  scoped_refptr<ObjectBase> object;
  const IpczResult result = transport.DeserializeObject(
      base::span(message.bytes), base::span(message.handles), object);
  EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
  EXPECT_FALSE(object);

  // The handle must not have been adopted (and therefore closed) by the
  // deserializer.
  EXPECT_TRUE(::SetEvent(local_handle));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(RecipientHandleFromUntrustedBrokerClient,
                                  MojoIpczTransportTest,
                                  h) {
  // This client is connected to a broker but is running in an elevated process
  // relative to that broker (as is the case for an elevated process accepting
  // an invitation), so it must not accept handles which the broker claims to
  // already belong to this client.
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  transport->set_is_elevated(true);
  transport->set_is_trusted_by_peer(true);
  ASSERT_FALSE(transport->CanAcceptReceiverOwnedHandles());

  TransportListener listener(*transport);

  // Hold a local handle which the deserializer must not adopt.
  base::win::ScopedHandle event(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
  ASSERT_TRUE(event.is_valid());
  ExpectRecipientOwnedHandleRejected(*transport, listener.WaitForNextMessage(),
                                     event.get());

  TestMessage("done").Transmit(*transport);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, RecipientHandleFromUntrustedBroker) {
  RunTestClientWithController(
      "RecipientHandleFromUntrustedBrokerClient", [&](ClientController& c) {
        scoped_refptr<Transport> transport =
            CreateAndSendTransport(c.pipe(), c.process());

        TransportListener listener(*transport);

        // Serialize a wrapped handle. With a known remote process and a broker
        // source, handles are encoded as already owned by the recipient. The
        // client substitutes its own local handle value before deserializing.
        base::win::ScopedHandle handle(
            ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
        auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
            PlatformHandle(std::move(handle)));
        SerializeObjectFor(*transport, std::move(wrapper)).Transmit(*transport);

        EXPECT_EQ("done", listener.WaitForNextMessage().as_string());
        listener.WaitForDisconnect();
      });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(
    RecipientHandleFromUntrustedBrokerToBrokerClient,
    MojoIpczTransportTest,
    h) {
  // This client is a broker connected to another broker over what is modeled
  // on mojo::IsolatedConnection, and it has declared the peer's process
  // untrustworthy - as the updater does for its public endpoint, which any
  // authenticated user may connect to. Being a broker confers no implicit trust
  // on a peer broker, so this client must not accept handles which the peer
  // claims to already belong to it.
  scoped_refptr<Transport> transport = ReceiveTransport(
      h, {.source = Transport::kBroker, .destination = Transport::kBroker},
      Transport::ProcessTrust::kUntrusted);
  ASSERT_FALSE(transport->is_peer_trusted());
  ASSERT_FALSE(transport->CanAcceptReceiverOwnedHandles());

  TransportListener listener(*transport);

  // Hold a local handle which the deserializer must not adopt.
  base::win::ScopedHandle event(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
  ASSERT_TRUE(event.is_valid());
  ExpectRecipientOwnedHandleRejected(*transport, listener.WaitForNextMessage(),
                                     event.get());

  TestMessage("done").Transmit(*transport);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, RecipientHandleFromUntrustedBrokerToBroker) {
  RunTestClientWithController(
      "RecipientHandleFromUntrustedBrokerToBrokerClient",
      [&](ClientController& c) {
        PlatformChannel channel;
        MojoHandle transport_for_client =
            WrapPlatformHandle(
                channel.TakeRemoteEndpoint().TakePlatformHandle())
                .release()
                .value();
        WriteMessageWithHandles(c.pipe(), "", &transport_for_client, 1);
        scoped_refptr<Transport> transport = Transport::Create(
            {.source = Transport::kBroker, .destination = Transport::kBroker},
            channel.TakeLocalEndpoint(), c.process().Duplicate());
        EXPECT_FALSE(transport->is_peer_trusted());

        // A broker with a handle to the peer's process encodes handles as
        // already owned by the recipient. The client must reject them because
        // it has declared this peer's process untrustworthy.
        transport->set_is_trusted_by_peer(true);

        TransportListener listener(*transport);

        // The client substitutes its own local handle value for the encoded
        // one before deserializing.
        base::win::ScopedHandle handle(
            ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
        auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
            PlatformHandle(std::move(handle)));
        SerializeObjectFor(*transport, std::move(wrapper)).Transmit(*transport);

        EXPECT_EQ("done", listener.WaitForNextMessage().as_string());
        listener.WaitForDisconnect();
      });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(SenderHandleToUntrustingBrokerClient,
                                  MojoIpczTransportTest,
                                  h) {
  // This client plays the server of RecipientHandleFromUntrustedBrokerToBroker:
  // a broker which has declared its peer broker's process untrustworthy, but
  // which holds a handle to that process, as the updater's server does by
  // opening each caller before accepting its connection.
  scoped_refptr<Transport> transport = ReceiveTransport(
      h, {.source = Transport::kBroker, .destination = Transport::kBroker},
      Transport::ProcessTrust::kUntrusted);
  transport->set_remote_process(base::Process::OpenWithAccess(
      base::GetParentProcessId(base::GetCurrentProcessHandle()),
      PROCESS_DUP_HANDLE));
  ASSERT_TRUE(transport->remote_process().IsValid());
  ASSERT_FALSE(transport->CanAcceptReceiverOwnedHandles());

  TransportListener listener(*transport);

  // The peer must have encoded its handle as sender-owned, since a
  // recipient-owned one would have been refused above. This end duplicates it
  // out of the peer's process.
  scoped_refptr<WrappedPlatformHandle> wrapper =
      DeserializeObjectFrom<WrappedPlatformHandle>(
          *transport, listener.WaitForNextMessage());
  PlatformHandle handle = wrapper->TakeHandle();
  ASSERT_TRUE(handle.is_valid());
  EXPECT_TRUE(::SetEvent(handle.GetHandle().get()));

  TestMessage("done").Transmit(*transport);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, SenderHandleToUntrustingBroker) {
  // The client side of the same relationship. It can open the peer's process
  // (an administrator connecting to the system updater can), so on its own it
  // would pre-duplicate handles and be refused. Declaring the peer elevated, as
  // UpdateServiceProxyMojoImpl::OnConnected() does, makes it send handles as
  // its own instead and lets the peer do the duplication.
  RunTestClientWithController(
      "SenderHandleToUntrustingBrokerClient", [&](ClientController& c) {
        PlatformChannel channel;
        MojoHandle transport_for_client =
            WrapPlatformHandle(
                channel.TakeRemoteEndpoint().TakePlatformHandle())
                .release()
                .value();
        WriteMessageWithHandles(c.pipe(), "", &transport_for_client, 1);
        scoped_refptr<Transport> transport = Transport::Create(
            {.source = Transport::kBroker, .destination = Transport::kBroker},
            channel.TakeLocalEndpoint(), c.process().Duplicate());
        ASSERT_TRUE(transport->remote_process().IsValid());
        transport->set_is_peer_elevated(true);

        TransportListener listener(*transport);

        base::win::ScopedHandle handle(
            ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
        ASSERT_TRUE(handle.is_valid());
        // Keep a duplicate to observe the peer's SetEvent() on the object.
        base::win::ScopedHandle observer;
        {
          HANDLE raw = nullptr;
          ASSERT_TRUE(::DuplicateHandle(::GetCurrentProcess(), handle.get(),
                                        ::GetCurrentProcess(), &raw, 0, FALSE,
                                        DUPLICATE_SAME_ACCESS));
          observer.Set(raw);
        }
        auto wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
            PlatformHandle(std::move(handle)));
        SerializeObjectFor(*transport, std::move(wrapper)).Transmit(*transport);

        EXPECT_EQ("done", listener.WaitForNextMessage().as_string());
        EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(observer.get(), 0));
        listener.WaitForDisconnect();
      });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransmitThreadClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);

  TransportListener listener(*transport);

  scoped_refptr<WrappedPlatformHandle> wrapper =
      DeserializeObjectFrom<WrappedPlatformHandle>(
          *transport, listener.WaitForNextMessage());
  CHECK(wrapper);
  auto handle = wrapper->TakeHandle().TakeHandle();
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

class MojoIpczTransportHandleTest
    : public MojoIpczTransportTest,
      public ::testing::WithParamInterface</*feature_enabled=*/bool> {
 public:
  MojoIpczTransportHandleTest() {
    features_.InitWithFeatureState(core::kMojoHandleTypeProtections,
                                   GetParam());
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that only the allowlisted set of object types can be transmitted. See
// `MaybeCheckIfHandleIsUnsafe` for the allowlist. An object of type "Thread" is
// used here.
TEST_P(MojoIpczTransportHandleTest, TransmitThread) {
  RunTestClientWithController("TransmitThreadClient", [&](ClientController& c) {
    scoped_refptr<Transport> transport = CreateAndSendTransport(
        c.pipe(), c.process(), Transport::ProcessTrust::kUntrusted);

    TransportListener listener(*transport);
    HANDLE thread;
    // Create a real Thread handle, not a psuedohandle. Psuedohandles are
    // blocked elsewhere.
    CHECK(::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(),
                            ::GetCurrentProcess(), &thread,
                            /*dwDesiredAccess=*/0, /*bInheritHandle=*/FALSE,
                            DUPLICATE_SAME_ACCESS));
    auto thread_wrapper = base::MakeRefCounted<WrappedPlatformHandle>(
        PlatformHandle(base::win::ScopedHandle(thread)));
    if (GetParam()) {
      EXPECT_NOTREACHED_DEATH({
        SerializeObjectFor(*transport, std::move(thread_wrapper))
            .Transmit(*transport);
      });
      // Handler will never get this message as the controller has crashed in
      // death check, so send over a valid handle in the form of a file to
      // unblock the handler.
      SerializeFileFor(
          *transport, base::File(base::PathService::CheckedGet(base::FILE_EXE),
                                 base::File::FLAG_OPEN | base::File::FLAG_READ))
          .Transmit(*transport);
    } else {
      SerializeObjectFor(*transport, std::move(thread_wrapper))
          .Transmit(*transport);
    }
    listener.WaitForDisconnect();
  });
}

INSTANTIATE_TEST_SUITE_P(/*empty prefix*/,
                         MojoIpczTransportHandleTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "FeatureEnabled"
                                             : "FeatureDisabled";
                         });

// Regression test for the exemption in DecodeHandle() for transports whose
// remote process is this process. mojo::DirectReceiver hosts an extra node
// inside an existing process and hands the broker a transport whose remote
// process is that process. Within a non-broker process both of that
// transport's endpoint types are kNonBroker, so the node's end is not
// source-trusted, but the broker holds a handle to the process and therefore
// still pre-duplicates handles for it. Rejecting those handles leaves the node
// unable to receive its shared memory and hangs it. See CreateTransportPair()
// in mojo/public/cpp/bindings/direct_receiver.cc.
TEST_F(MojoIpczTransportTest, RecipientHandleForNodeInSameProcess) {
  // The broker's end of the introduced transport, which adopted a handle to
  // the process hosting the new node. See CreateTransports() in driver.cc.
  auto [broker_side, unused_broker_peer] =
      Transport::CreatePair(Transport::kBroker, Transport::kNonBroker);
  broker_side->set_remote_process(base::Process::Current());

  // The new node's end of the same transport, as created within a non-broker
  // process.
  auto [local_side, unused_local_peer] =
      Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);
  local_side->set_remote_process(base::Process::Current());
  ASSERT_FALSE(local_side->CanAcceptReceiverOwnedHandles());

  base::win::ScopedHandle event(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
  ASSERT_TRUE(event.is_valid());
  TestMessage message = SerializeObjectFor(
      *broker_side, base::MakeRefCounted<WrappedPlatformHandle>(
                        PlatformHandle(std::move(event))));

  scoped_refptr<WrappedPlatformHandle> received =
      DeserializeObjectFrom<WrappedPlatformHandle>(*local_side, message);
  ASSERT_TRUE(received);
  EXPECT_TRUE(received->TakeHandle().is_valid());
}

// Both ends of a mojo::IsolatedConnection are brokers and neither is explicitly
// trusted by the other, but handle transfer must keep working in both
// directions: whichever end cannot open its peer's process depends entirely on
// the peer pre-duplicating handles for it. Only an end which has declared the
// peer's process untrustworthy refuses, per
// RecipientHandleFromUntrustedBrokerToBroker.
TEST_F(MojoIpczTransportTest, RecipientHandleBetweenIsolatedBrokers) {
  auto [sender, unused_sender_peer] =
      Transport::CreatePair(Transport::kBroker, Transport::kBroker);
  sender->set_remote_process(base::Process::Current());

  // The receiving end has no handle to the sender's process, which is exactly
  // why the sender has to pre-duplicate. This models an unprivileged client of
  // e.g. the updater, which cannot open the server process.
  auto [receiver, unused_receiver_peer] =
      Transport::CreatePair(Transport::kBroker, Transport::kBroker);
  ASSERT_FALSE(receiver->is_peer_trusted());
  ASSERT_FALSE(receiver->remote_process().IsValid());
  ASSERT_TRUE(receiver->CanAcceptReceiverOwnedHandles());

  base::win::ScopedHandle event(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
  ASSERT_TRUE(event.is_valid());
  TestMessage message =
      SerializeObjectFor(*sender, base::MakeRefCounted<WrappedPlatformHandle>(
                                      PlatformHandle(std::move(event))));

  scoped_refptr<WrappedPlatformHandle> received =
      DeserializeObjectFrom<WrappedPlatformHandle>(*receiver, message);
  ASSERT_TRUE(received);
  EXPECT_TRUE(received->TakeHandle().is_valid());
}

#endif  // BUILDFLAG(IS_WIN)

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransportFromUntrustedClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  TransportListener listener(*transport);
  EXPECT_EQ("ready", listener.WaitForNextMessage().as_string());

  for (int i = 0; i < 2; i++) {
    auto ours = i == 0 ? Transport::kNonBroker : Transport::kBroker;
    auto theirs = i == 0 ? Transport::kBroker : Transport::kNonBroker;
    {
      auto [our_new_transport, their_new_transport] =
          Transport::CreatePair(ours, theirs);

      their_new_transport->set_is_peer_trusted(true);

      SerializeObjectFor(*transport, std::move(their_new_transport))
          .Transmit(*transport);
      EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
    }

    {
      auto [our_new_transport, their_new_transport] =
          Transport::CreatePair(ours, theirs);

      their_new_transport->set_is_trusted_by_peer(true);

      SerializeObjectFor(*transport, std::move(their_new_transport))
          .Transmit(*transport);
      if (ours == Transport::kNonBroker) {
        EXPECT_EQ("got untrusted", listener.WaitForNextMessage().as_string());
      } else {
        EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
      }
    }
  }

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransportFromUntrustedBrokerClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  TransportListener listener(*transport);
  EXPECT_EQ("ready", listener.WaitForNextMessage().as_string());

  {
    auto [our_new_transport, their_new_transport] =
        Transport::CreatePair(Transport::kBroker, Transport::kNonBroker);
    EXPECT_EQ(Transport::kBroker, their_new_transport->destination_type());
    SerializeObjectFor(*transport, std::move(their_new_transport))
        .Transmit(*transport);
    EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
  }

  {
    auto [our_new_transport, their_new_transport] =
        Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);
    their_new_transport->set_is_peer_trusted(true);
    SerializeObjectFor(*transport, std::move(their_new_transport))
        .Transmit(*transport);
    EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
  }

  {
    auto [our_new_transport, their_new_transport] =
        Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);
    SerializeObjectFor(*transport, std::move(their_new_transport))
        .Transmit(*transport);
    EXPECT_EQ("got untrusted", listener.WaitForNextMessage().as_string());
  }

  {
    // A transport whose remote process is this client's own process, so that
    // the serialized header carries `is_same_remote_process`. A broker peer
    // must not be able to make the recipient adopt its handle to this process.
    auto [our_new_transport, their_new_transport] =
        Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);
    their_new_transport->set_remote_process(base::Process::Current());
    SerializeObjectFor(*transport, std::move(their_new_transport))
        .Transmit(*transport);
    EXPECT_EQ("got no process", listener.WaitForNextMessage().as_string());
  }

#if BUILDFLAG(IS_WIN)
  {
    // The other way to name a remote process: attach a handle to one directly.
    // Opening our own process by PID yields a real handle rather than the
    // current-process pseudo-handle, so `is_same_remote_process` stays clear
    // and the process travels in the serialized transport's second handle.
    // A broker peer must not adopt that either. This vector is Windows-only;
    // see Transport::ShouldSerializeProcessHandle().
    auto [our_new_transport, their_new_transport] =
        Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);
    base::Process opened = base::Process::Open(base::GetCurrentProcId());
    ASSERT_TRUE(opened.IsValid());
    ASSERT_FALSE(opened.is_current());
    their_new_transport->set_remote_process(std::move(opened));
    SerializeObjectFor(*transport, std::move(their_new_transport))
        .Transmit(*transport);
    EXPECT_EQ("got no attached process",
              listener.WaitForNextMessage().as_string());
  }
#endif  // BUILDFLAG(IS_WIN)

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, TransportFromUntrustedBroker) {
  // When both endpoints of a transport are brokers, the peer being a broker
  // does not by itself grant it any additional trust on this end. A broker
  // receiving a serialized transport over such a link must reject it if it
  // claims its own peer is trusted or is itself a broker, and must not let it
  // inherit a handle to the sender's process.
  RunTestClientWithController(
      "TransportFromUntrustedBrokerClient", [&](ClientController& c) {
        PlatformChannel channel;
        MojoHandle transport_for_client =
            WrapPlatformHandle(
                channel.TakeRemoteEndpoint().TakePlatformHandle())
                .release()
                .value();
        WriteMessageWithHandles(c.pipe(), "", &transport_for_client, 1);
#if BUILDFLAG(IS_WIN)
        constexpr auto kClientProcessTrust =
            Transport::ProcessTrust::kUntrusted;
#else
        // Process trust is not tracked off Windows.
        constexpr auto kClientProcessTrust = Transport::ProcessTrust{};
#endif
        scoped_refptr<Transport> transport = Transport::Create(
            {.source = Transport::kBroker, .destination = Transport::kBroker},
            channel.TakeLocalEndpoint(), c.process().Duplicate(),
            kClientProcessTrust);
        EXPECT_FALSE(transport->is_peer_trusted());

        TransportListener listener(*transport);
        TestMessage("ready").Transmit(*transport);

        for (int i = 0; i < 2; i++) {
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<ObjectBase> object;
          const IpczResult result = transport->DeserializeObject(
              base::span(message.bytes), base::span(message.handles), object);
          EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
          TestMessage("got null").Transmit(*transport);
        }

        {
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<Transport> received =
              DeserializeObjectFrom<Transport>(*transport, message);
          EXPECT_FALSE(received->is_peer_trusted());
          EXPECT_EQ(Transport::kNonBroker, received->destination_type());
          TestMessage("got untrusted").Transmit(*transport);
        }

        {
          // The serialized transport claims to share the client's process. On
          // Windows we have declared that process untrustworthy, so the new
          // transport must not inherit our handle to it. Elsewhere process
          // trust is untracked and a remote process handle plays no part in
          // handle transfer, so the claim is honored as it is on trunk.
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<Transport> received =
              DeserializeObjectFrom<Transport>(*transport, message);
#if BUILDFLAG(IS_WIN)
          EXPECT_FALSE(received->remote_process().IsValid());
#endif
          TestMessage("got no process").Transmit(*transport);
        }

#if BUILDFLAG(IS_WIN)
        {
          // Same conclusion for a process handle the client attached directly
          // rather than asserting through the header. Refusing only the header
          // bit would leave this path open.
          TestMessage message = listener.WaitForNextMessage();
          scoped_refptr<Transport> received =
              DeserializeObjectFrom<Transport>(*transport, message);
          ASSERT_TRUE(received);
          EXPECT_FALSE(received->remote_process().IsValid());
          TestMessage("got no attached process").Transmit(*transport);
        }
#endif  // BUILDFLAG(IS_WIN)

        listener.WaitForDisconnect();
      });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransportWithSameRemoteProcessClient,
                                  MojoIpczTransportTest,
                                  h) {
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  TransportListener listener(*transport);
  EXPECT_EQ("ready", listener.WaitForNextMessage().as_string());

  // A node hosted within this process is introduced to our broker by sending
  // the broker a transport whose remote process is our own. See
  // CreateTransports() in driver.cc.
  auto [our_new_transport, their_new_transport] =
      Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);
  their_new_transport->set_remote_process(base::Process::Current());
  SerializeObjectFor(*transport, std::move(their_new_transport))
      .Transmit(*transport);
  EXPECT_EQ("got process", listener.WaitForNextMessage().as_string());

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, TransportWithSameRemoteProcess) {
  // A broker must honor `is_same_remote_process` from a non-broker it brokers
  // for, because that is how nodes hosted within a client process (e.g.
  // mojo::DirectReceiver) are introduced. The process handle is not serialized
  // in this case, so without this the broker would have no handle to the new
  // transport's process and could not transmit handles over it at all.
  RunTestClientWithController(
      "TransportWithSameRemoteProcessClient", [&](ClientController& c) {
        scoped_refptr<Transport> transport =
            CreateAndSendTransport(c.pipe(), c.process());
        TransportListener listener(*transport);
        TestMessage("ready").Transmit(*transport);

        TestMessage message = listener.WaitForNextMessage();
        scoped_refptr<Transport> received =
            DeserializeObjectFrom<Transport>(*transport, message);
        EXPECT_TRUE(received->remote_process().IsValid());
        EXPECT_EQ(c.process().Pid(), received->remote_process().Pid());
        TestMessage("got process").Transmit(*transport);

        listener.WaitForDisconnect();
      });
}

// The counterpart of the case rejected by TransportFromUntrustedBroker, and of
// RecipientHandleBetweenIsolatedBrokers for the other predicate. Both ends of a
// mojo::IsolatedConnection are brokers and neither is source-trusted, but
// unless one of them has declared the other's process untrustworthy the claim
// must still be honored. Refusing it would leave transports introduced over
// such a link with no handle to their remote process, and so no way to move
// handles at all.
TEST_F(MojoIpczTransportTest, SameRemoteProcessBetweenIsolatedBrokers) {
  auto [sender, unused_sender_peer] =
      Transport::CreatePair(Transport::kBroker, Transport::kBroker);

  auto [receiver, unused_receiver_peer] =
      Transport::CreatePair(Transport::kBroker, Transport::kBroker);
  receiver->set_remote_process(base::Process::Current());
  ASSERT_FALSE(receiver->is_peer_trusted());
  ASSERT_TRUE(receiver->CanAcceptRemoteProcessFromPeer());

  // A transport the peer hosts in its own process, which is what makes it set
  // TransportHeader::is_same_remote_process when serializing.
  auto [transmitted, unused_transmitted_peer] =
      Transport::CreatePair(Transport::kNonBroker, Transport::kNonBroker);
  transmitted->set_remote_process(base::Process::Current());

  TestMessage message = SerializeObjectFor(*sender, std::move(transmitted));
  scoped_refptr<Transport> received =
      DeserializeObjectFrom<Transport>(*receiver, message);
  ASSERT_TRUE(received);
  EXPECT_TRUE(received->remote_process().IsValid());
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(TransportFromLessPrivilegedBrokerClient,
                                  MojoIpczTransportTest,
                                  h) {
  // This client simulates an elevated process which has accepted an invitation
  // from a less-privileged broker (see Invitation::Accept). The broker must not
  // be trusted to vouch for other transports sent to this process.
  scoped_refptr<Transport> transport = ReceiveTransport(h);
  transport->set_is_elevated(true);
  transport->set_is_trusted_by_peer(true);
#if BUILDFLAG(IS_WIN)
  // Like an elevated process, use a handle to the broker's process to
  // duplicate handles sent by the broker.
  transport->set_remote_process(base::Process::OpenWithAccess(
      base::GetParentProcessId(base::GetCurrentProcessHandle()),
      PROCESS_DUP_HANDLE));
  ASSERT_TRUE(transport->remote_process().IsValid());
#endif

  TransportListener listener(*transport);

  // A transport claiming a trusted peer, then a transport to a broker.
  for (int i = 0; i < 2; i++) {
    TestMessage message = listener.WaitForNextMessage();
    scoped_refptr<ObjectBase> object;
    const IpczResult result = transport->DeserializeObject(
        base::span(message.bytes), base::span(message.handles), object);
    EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
    EXPECT_FALSE(object);
    TestMessage("got null").Transmit(*transport);
  }

  // An untrusted transport to a non-broker is accepted, and is elevated
  // relative to its peer just like the transport which conveyed it.
  {
    scoped_refptr<Transport> received = DeserializeObjectFrom<Transport>(
        *transport, listener.WaitForNextMessage());
    EXPECT_FALSE(received->is_peer_trusted());
    EXPECT_TRUE(received->is_elevated());
    EXPECT_EQ(Transport::kNonBroker, received->destination_type());
    TestMessage("got untrusted").Transmit(*transport);
  }

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h));
}

TEST_F(MojoIpczTransportTest, TransportFromLessPrivilegedBroker) {
  RunTestClientWithController(
      "TransportFromLessPrivilegedBrokerClient", [&](ClientController& c) {
        // Configure this end like a broker which has invited an elevated
        // process (see Invitation::Send).
        scoped_refptr<Transport> transport =
            CreateAndSendTransport(c.pipe(), c.process());
        transport->set_is_peer_trusted(true);
        transport->set_is_peer_elevated(true);

        TransportListener listener(*transport);

        {
          auto [our_new_transport, their_new_transport] = Transport::CreatePair(
              Transport::kNonBroker, Transport::kNonBroker);
          their_new_transport->set_is_peer_trusted(true);
          SerializeObjectFor(*transport, std::move(their_new_transport))
              .Transmit(*transport);
          EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
        }

        {
          auto [our_new_transport, their_new_transport] =
              Transport::CreatePair(Transport::kBroker, Transport::kNonBroker);
          EXPECT_EQ(Transport::kBroker,
                    their_new_transport->destination_type());
          SerializeObjectFor(*transport, std::move(their_new_transport))
              .Transmit(*transport);
          EXPECT_EQ("got null", listener.WaitForNextMessage().as_string());
        }

        {
          auto [our_new_transport, their_new_transport] = Transport::CreatePair(
              Transport::kNonBroker, Transport::kNonBroker);
          SerializeObjectFor(*transport, std::move(their_new_transport))
              .Transmit(*transport);
          EXPECT_EQ("got untrusted", listener.WaitForNextMessage().as_string());
        }

        listener.WaitForDisconnect();
      });
}

TEST_F(MojoIpczTransportTest, TransportFromUntrusted) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/414392683) default to untrusted/untracked.
  Transport::ProcessTrust process_trust = Transport::ProcessTrust::kUntrusted;
#else
  Transport::ProcessTrust process_trust{};
#endif
  RunTestClientWithController(
      "TransportFromUntrustedClient", [&](ClientController& c) {
        scoped_refptr<Transport> transport =
            CreateAndSendTransport(c.pipe(), c.process(), process_trust);

        TransportListener listener(*transport);
        TestMessage("ready").Transmit(*transport);

        // A broker (this process) should reject transports from untrusted
        // clients if they claim the transport's peer is trusted or is a broker.
        // It is ok to allow transports from a client that indicates they trust
        // the peer, as a broker will not make trust decisions based on that.
        for (int i = 0; i < 2; i++) {
          auto theirs = i == 0 ? Transport::kNonBroker : Transport::kBroker;
          {
            TestMessage message = listener.WaitForNextMessage();
            scoped_refptr<ObjectBase> object;
            const IpczResult result = transport->DeserializeObject(
                base::span(message.bytes), base::span(message.handles), object);
            EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
            TestMessage("got null").Transmit(*transport);
          }

          {
            TestMessage message = listener.WaitForNextMessage();
            if (theirs == Transport::kNonBroker) {
              scoped_refptr<Transport> transport2 =
                  DeserializeObjectFrom<Transport>(*transport, message);
              EXPECT_TRUE(transport2->is_trusted_by_peer());
              EXPECT_FALSE(transport2->is_peer_trusted());
              TestMessage("got untrusted").Transmit(*transport);
            } else {
              scoped_refptr<ObjectBase> object;
              const IpczResult result = transport->DeserializeObject(
                  base::span(message.bytes), base::span(message.handles),
                  object);
              EXPECT_EQ(result, IPCZ_RESULT_INVALID_ARGUMENT);
              TestMessage("got null").Transmit(*transport);
            }
          }
        }

        listener.WaitForDisconnect();
      });
}

}  // namespace
}  // namespace mojo::core::ipcz_driver
