// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CHANNEL_H_
#define MOJO_CORE_CHANNEL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/platform_handle_in_transit.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo::core {

const size_t kChannelMessageAlignment = 8;

constexpr bool IsAlignedForChannelMessage(size_t n) {
  return n % kChannelMessageAlignment == 0;
}

// Channel provides a thread-safe interface to read and write arbitrary
// delimited messages over an underlying I/O channel, optionally transferring
// one or more platform handles in the process.
class MOJO_SYSTEM_IMPL_EXPORT Channel
    : public base::RefCountedThreadSafe<Channel> {
 public:
  enum class HandlePolicy {
    // If a Channel is constructed in this mode, it will accept messages with
    // platform handle attachements.
    kAcceptHandles,

    // If a Channel is constructed in this mode, it will reject messages with
    // platform handle attachments and treat them as malformed messages.
    kRejectHandles,
  };
  enum class DispatchBufferPolicy {
    // If the Channel is constructed in this mode, it will create and manage a
    // buffer that implementations should manipulate with GetReadBuffer() and
    // OnReadComplete().
    kManaged,

    // If the Channel is constructed in this mode, it will not create and
    // manage a buffer for the implementation. Instead, the implementation must
    // use its own buffer and pass spans of it to TryDispatchMessage().
    kUnmanaged,
  };

  struct Message;

  using MessagePtr = std::unique_ptr<Message>;
  using AlignedBuffer = std::unique_ptr<char>;

  // A message to be written to a channel.
  struct MOJO_SYSTEM_IMPL_EXPORT Message {
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;

    virtual ~Message() = default;

    enum class MessageType : uint16_t {
      // An old format normal message, that uses the LegacyHeader.
      // Only used on Android and ChromeOS.
      // TODO(crbug.com/41303999): remove legacy support when Arc++ has
      // updated to Mojo with normal versioned messages.
      NORMAL_LEGACY = 0,
#if BUILDFLAG(IS_IOS)
      // A control message containing handles to echo back.
      HANDLES_SENT,
      // A control message containing handles that can now be closed.
      HANDLES_SENT_ACK,
#endif
      // A normal message that uses Header and can contain extra header values.
      NORMAL,

      // The UPGRADE_OFFER control message offers to upgrade the channel to
      // another side who has advertised support for an upgraded channel.
      UPGRADE_OFFER,
      // The UPGRADE_ACCEPT control message is returned when an upgrade offer is
      // accepted.
      UPGRADE_ACCEPT,
      // The UPGRADE_REJECT control message is returned when the receiver cannot
      // or chooses not to upgrade the channel.
      UPGRADE_REJECT,
    };

#pragma pack(push, 1)
    // Old message wire format for ChromeOS and Android, used by NORMAL_LEGACY
    // messages.
    struct LegacyHeader {
      // Message size in bytes, including the header.
      uint32_t num_bytes;

      // Number of attached handles.
      uint16_t num_handles;

      MessageType message_type;
    };

    // Header used by NORMAL messages.
    // To preserve backward compatibility with LegacyHeader, the num_bytes and
    // message_type field must be at the same offset as in LegacyHeader.
    struct Header {
      // Message size in bytes, including the header.
      uint32_t num_bytes;

      // Total size of header, including extra header data (i.e. HANDLEs on
      // windows).
      uint16_t num_header_bytes;

      MessageType message_type;

      // Number of attached handles. May be less than the reserved handle
      // storage size in this message on platforms that serialise handles as
      // data (i.e. HANDLEs on Windows, Mach ports on OSX).
      uint16_t num_handles;

      char padding[6];
    };

    // Header used for all messages when the Channel backs an ipcz transport.
    //
    // Note: This struct *must* be forward and backward compatible. Changes are
    // append-only, must add a new "struct {} vx" member, and code must be able
    // to deal with newer and older versions of this header.
    struct IpczHeader {
      // The size of this header in bytes. Used for versioning.
      uint16_t size;

      // Number of handles attached to the message, out-of-band from its data.
      // Always zero on Windows, where handles are serialized as inlined data.
      uint16_t num_handles;

      // Total size of this message in bytes. This is the size of this header
      // plus the size of any message data immediately following it.
      uint32_t num_bytes;

      struct {
        // When this header was created, relative to the reference of
        // base::TimeTicks().
        int64_t creation_timeticks_us;
      } v2;
      NO_UNIQUE_ADDRESS struct {
      } v2_marker;
    };

    static constexpr size_t kMinIpczHeaderSize = offsetof(IpczHeader, v2);
    static bool IsAtLeastV2(const IpczHeader& header) {
      return header.size >= offsetof(IpczHeader, v2_marker);
    }

#if BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
    struct MachPortsEntry {
      // The PlatformHandle::Type.
      uint8_t type;
    };
    static_assert(sizeof(MachPortsEntry) == 1,
                  "sizeof(MachPortsEntry) must be 1 byte");

    // Structure of the extra header field when present on OSX.
    struct MachPortsExtraHeader {
      // Actual number of Mach ports encoded in the extra header.
      uint16_t num_ports;

      // Array of encoded Mach ports. If |num_ports| > 0, |entries[0]| through
      // to |entries[num_ports-1]| inclusive are valid.
      MachPortsEntry entries[0];
    };
    static_assert(sizeof(MachPortsExtraHeader) == 2,
                  "sizeof(MachPortsExtraHeader) must be 2 bytes");
#elif BUILDFLAG(IS_FUCHSIA)
    struct HandleInfoEntry {
      // True if the handle represents an FDIO file-descriptor, false otherwise.
      bool is_file_descriptor;
    };
#elif BUILDFLAG(IS_WIN)
    struct HandleEntry {
      // The windows HANDLE. HANDLEs are guaranteed to fit inside 32-bits.
      // See: https://msdn.microsoft.com/en-us/library/aa384203(VS.85).aspx
      uint32_t handle;
    };
    static_assert(sizeof(HandleEntry) == 4,
                  "sizeof(HandleEntry) must be 4 bytes");
#endif
#pragma pack(pop)

    // Allocates and owns a buffer for message data with enough capacity for
    // |payload_size| bytes plus a header, plus |max_handles| platform handles.
    static MessagePtr CreateMessage(size_t payload_size, size_t max_handles);
    static MessagePtr CreateMessage(size_t payload_size,
                                    size_t max_handles,
                                    MessageType message_type);
    static MessagePtr CreateMessage(size_t capacity,
                                    size_t payload_size,
                                    size_t max_handles);
    static MessagePtr CreateMessage(size_t capacity,
                                    size_t max_handles,
                                    size_t payload_size,
                                    MessageType message_type);

    static MessagePtr CreateIpczMessage(base::span<const uint8_t> data,
                                        std::vector<PlatformHandle> handles);

    // Extends the portion of the total message capacity which contains
    // meaningful payload data. Storage capacity which falls outside of this
    // range is not transmitted when the message is sent.
    //
    // If the message's current capacity is not large enough to accommodate the
    // new payload size, it will be reallocated accordingly.
    static void ExtendPayload(MessagePtr& message, size_t new_payload_size);

    static MessagePtr CreateRawForFuzzing(base::span<const unsigned char> data);

    // Constructs a Message from serialized message data, optionally coming from
    // a known remote process.
    static MessagePtr Deserialize(
        const void* data,
        size_t data_num_bytes,
        HandlePolicy handle_policy,
        base::ProcessHandle from_process = base::kNullProcessHandle);

    virtual const void* data() const = 0;
    virtual void* mutable_data() const = 0;

    size_t data_num_bytes() const { return size_; }

    // The current capacity of the message buffer, not counting internal header
    // data.
    virtual size_t capacity() const = 0;

    const void* extra_header() const;
    void* mutable_extra_header();
    size_t extra_header_size() const;

    void* mutable_payload();
    const void* payload() const;
    size_t payload_size() const;

    size_t num_handles() const;
    bool has_handles() const;

    bool is_legacy_message() const;
    LegacyHeader* legacy_header() const;
    Header* header() const;

    // Note: SetHandles() and TakeHandles() invalidate any previous value of
    // handles().
    virtual void SetHandles(std::vector<PlatformHandle> new_handles) = 0;
    virtual void SetHandles(
        std::vector<PlatformHandleInTransit> new_handles) = 0;
    virtual std::vector<PlatformHandleInTransit> TakeHandles() = 0;
    virtual size_t NumHandlesForTransit() const = 0;

    void SetVersionForTest(uint16_t version_number);

   protected:
    Message() = default;

    virtual bool ExtendPayload(size_t new_payload_size) = 0;

    // The size of the message. This is the portion of |data_| that should
    // be transmitted if the message is written to a channel. Includes all
    // headers and user payload.
    size_t size_ = 0;
  };

  // Error types which may be reported by a Channel instance to its delegate.
  enum class Error {
    // The remote end of the channel has been closed, either explicitly or
    // because the process which hosted it is gone.
    kDisconnected,

    // For connection-oriented channels (e.g. named pipes), an unexpected error
    // occurred during channel connection.
    kConnectionFailed,

    // Some incoming data failed validation, implying either a buggy or
    // compromised sender.
    kReceivedMalformedData,
  };

  // Delegate methods are called from the I/O task runner with which the Channel
  // was created (see Channel::Create).
  class MOJO_SYSTEM_IMPL_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // Indicates whether the listener on this Channel is an ipcz transport.
    virtual bool IsIpczTransport() const;

    // Notify of a received message. |payload| is not owned and must not be
    // retained; it will be null if |payload_size| is 0. |handles| are
    // transferred to the callee.
    virtual void OnChannelMessage(const void* payload,
                                  size_t payload_size,
                                  std::vector<PlatformHandle> handles) = 0;

    // Notify that an error has occured and the Channel will cease operation.
    virtual void OnChannelError(Error error) = 0;

    // Notify that the Channel is about to be destroyed and will definitely not
    // call into the Delegate again. Only called for Channels that back an ipcz
    // transport.
    virtual void OnChannelDestroyed();
  };

  // Creates a new Channel around a |platform_handle|, taking ownership of the
  // handle. All I/O on the handle will be performed on |io_task_runner|.
  // Note that ShutDown() MUST be called on the Channel some time before
  // |delegate| is destroyed.
  static scoped_refptr<Channel> Create(
      Delegate* delegate,
      ConnectionParams connection_params,
      HandlePolicy handle_policy,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Creates a new Channel similar to above, but for use as a driver transport
  // in the ipcz-based Mojo implementation. The main difference between these
  // Channel instances and others is that these ones use a simplified message
  // header, and the Channel is no longer responsible for encoding or decoding
  // any metadata about transmitted PlatformHandles, since the ipcz driver takes
  // care of that.
  static scoped_refptr<Channel> CreateForIpczDriver(
      Delegate* delegate,
      PlatformChannelEndpoint endpoint,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && \
    !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)
  // At this point only ChannelPosix needs InitFeatures.
  static void set_posix_use_writev(bool use_writev);
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) &&
        // !BUILDFLAG(MOJO_USE_APPLE_CHANNEL)

  static void set_use_trivial_messages(bool use_trivial_messages);

  bool is_for_ipcz() const { return is_for_ipcz_; }

  // SupportsChannelUpgrade will return true if this channel is capable of being
  // upgraded.
  static bool SupportsChannelUpgrade();

  // OfferChannelUpgrade will inform this channel that it should offer an
  // upgrade to the remote.
  void OfferChannelUpgrade();

  // Allows the caller to change the Channel's HandlePolicy after construction.
  void set_handle_policy(HandlePolicy policy) { handle_policy_ = policy; }

  // Request that the channel be shut down. This should always be called before
  // releasing the last reference to a Channel to ensure that it's cleaned up
  // on its I/O task runner's thread.
  //
  // Delegate methods will no longer be invoked after this call.
  void ShutDown();

  // Sets the process handle of the remote endpoint to which this Channel is
  // connected. If called at all, must be called only once, and before Start().
  void set_remote_process(base::Process remote_process) {
    DCHECK(!remote_process_.IsValid());
    remote_process_ = std::move(remote_process);
  }
  const base::Process& remote_process() const { return remote_process_; }

  // Begin processing I/O events. Delegate methods must only be invoked after
  // this call.
  virtual void Start() = 0;

  // Stop processing I/O events.
  virtual void ShutDownImpl() = 0;

  // Queues an outgoing message on the Channel. This message will either
  // eventually be written or will fail to write and trigger
  // Delegate::OnChannelError.
  virtual void Write(MessagePtr message) = 0;

  // Causes the platform handle to leak when this channel is shut down instead
  // of closing it.
  virtual void LeakHandle() = 0;

 protected:
  // Constructor for implementations to call. |delegate| and |handle_policy|
  // should be passed from Create(). |buffer_policy| should be specified by
  // the implementation.
  Channel(Delegate* delegate,
          HandlePolicy handle_policy,
          DispatchBufferPolicy buffer_policy = DispatchBufferPolicy::kManaged);
  virtual ~Channel();

  Delegate* delegate() const { return delegate_; }

  // Allows the caller to determine the current HandlePolicy.
  HandlePolicy handle_policy() const { return handle_policy_; }

  // Called by the implementation when it wants somewhere to stick data.
  // |*buffer_capacity| may be set by the caller to indicate the desired buffer
  // size. If 0, a sane default size will be used instead.
  //
  // Returns the address of a buffer which can be written to, and indicates its
  // actual capacity in |*buffer_capacity|.
  //
  // This should only be used with DispatchBufferPolicy::kManaged.
  char* GetReadBuffer(size_t* buffer_capacity);

  // Called by the implementation when new data is available in the read
  // buffer. Returns false to indicate an error. Upon success,
  // |*next_read_size_hint| will be set to a recommended size for the next
  // read done by the implementation. This should only be used with
  // DispatchBufferPolicy::kManaged.
  bool OnReadComplete(size_t bytes_read, size_t* next_read_size_hint);

  // Called by the implementation to deserialize a message stored in |buffer|.
  // If the channel was created with DispatchBufferPolicy::kUnmanaged, the
  // implementation should call this directly. If it was created with kManaged,
  // OnReadComplete() will call this. |*size_hint| will be set to a recommended
  // size for the next read done by the implementation. If `received_handles` is
  // not null, the provided handles are taken as handles which accompanied the
  // bytes in `buffer`. Otherwise the implementation must make handles available
  // via GetReadPlatformHandles/ForIpcz() as needed.
  enum class DispatchResult {
    // The message was dispatched and consumed. |size_hint| contains the size
    // of the message.
    kOK,
    // The message could not be deserialized because |buffer| does not contain
    // enough data. |size_hint| contains the amount of data missing.
    kNotEnoughData,
    // The message has associated handles that were not transferred in this
    // message.
    kMissingHandles,
    // An error occurred during processing.
    kError,
  };
  DispatchResult TryDispatchMessage(base::span<const char> buffer,
                                    size_t* size_hint);
  DispatchResult TryDispatchMessage(
      base::span<const char> buffer,
      std::optional<std::vector<PlatformHandle>> received_handles,
      size_t* size_hint);

  // Called by the implementation when something goes horribly wrong. It is NOT
  // OK to call this synchronously from any public interface methods.
  void OnError(Error error);

  // Retrieves the set of platform handles read for a given message.
  // |extra_header| and |extra_header_size| correspond to the extra header data.
  // Depending on the Channel implementation, this body may encode platform
  // handles, or handles may be stored and managed elsewhere by the
  // implementation.
  //
  // Returns |false| on unrecoverable error (i.e. the Channel should be closed).
  // Returns |true| otherwise. Note that it is possible on some platforms for an
  // insufficient number of handles to be available when this call is made, but
  // this is not necessarily an error condition. In such cases this returns
  // |true| but |*handles| will also be reset to null.
  //
  // If the implementation sets |*deferred| to |true|, it assumes responsibility
  // for dispatching the message eventually. It must copy |payload| to retain
  // it for later transmission.
  virtual bool GetReadPlatformHandles(const void* payload,
                                      size_t payload_size,
                                      size_t num_handles,
                                      const void* extra_header,
                                      size_t extra_header_size,
                                      std::vector<PlatformHandle>* handles,
                                      bool* deferred) = 0;

  // Consumes exactly `num_handles` received handles and appends them to
  // `handles` before returning true. If the Channel doesn't have enough
  // unconsumed handles ready to satisfy this request, `handles` is unmodified
  // but this still returns true. If any kind of error condition is detected,
  // this returns false.
  virtual bool GetReadPlatformHandlesForIpcz(
      size_t num_handles,
      std::vector<PlatformHandle>& handles) = 0;

  // Handles a received control message. Returns |true| if the message is
  // accepted, or |false| otherwise.
  virtual bool OnControlMessage(Message::MessageType message_type,
                                const void* payload,
                                size_t payload_size,
                                std::vector<PlatformHandle> handles);

 protected:
  enum class MessageType {
    kSent,
    kReceive,
  };

  // Records histograms that count sent/received messages per process type.
  // Must be guarded by a call to ShouldRecordSubsampledHistograms().
  static void LogHistogramForIPCMetrics(MessageType type);

  // Returns true for ~1/1000 calls. Used to reduce reporting overhead.
  bool ShouldRecordSubsampledHistograms();

 private:
  friend class base::RefCountedThreadSafe<Channel>;

  class ReadBuffer;

  const bool is_for_ipcz_;
  raw_ptr<Delegate, AcrossTasksDanglingUntriaged> delegate_;
  HandlePolicy handle_policy_;
  const std::unique_ptr<ReadBuffer> read_buffer_;

  // Handle to the process on the other end of this Channel, iff known.
  base::Process remote_process_;

  mutable base::Lock lock_;
  // base::MetricsSubSampler uses InsecureRandomGenerator to generate
  // pseudo-random numbers which leaves the synchronization to the client and is
  // not thread-safe, hence guarded by lock here.
  base::MetricsSubSampler sub_sampler_ GUARDED_BY(lock_);

  FRIEND_TEST_ALL_PREFIXES(ChannelTest, IpczHeaderCompatibilityTest);
};

}  // namespace mojo::core

#endif  // MOJO_CORE_CHANNEL_H_
