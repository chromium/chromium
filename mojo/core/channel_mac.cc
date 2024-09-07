// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/channel.h"

#include <mach/mach.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/apple/scoped_mach_vm.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/numerics/byte_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/typed_macros.h"

extern "C" {
kern_return_t fileport_makeport(int fd, mach_port_t*);
int fileport_makefd(mach_port_t);
}  // extern "C"

namespace mojo {
namespace core {

namespace {

constexpr mach_msg_id_t kChannelMacHandshakeMsgId = 'mjhs';
constexpr mach_msg_id_t kChannelMacInlineMsgId = 'MOJO';
constexpr mach_msg_id_t kChannelMacOOLMsgId = 'MOJ+';

class ChannelMac : public Channel,
                   public base::CurrentThread::DestructionObserver,
                   public base::MessagePumpKqueue::MachPortWatcher {
 public:
  ChannelMac(Delegate* delegate,
             ConnectionParams connection_params,
             HandlePolicy handle_policy,
             scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : Channel(delegate, handle_policy, DispatchBufferPolicy::kUnmanaged),
        self_(this),
        io_task_runner_(io_task_runner),
        watch_controller_(FROM_HERE) {
    PlatformHandle channel_handle =
        connection_params.TakeEndpoint().TakePlatformHandle();
    if (channel_handle.is_mach_send()) {
      send_port_ = channel_handle.TakeMachSendRight();
    } else if (channel_handle.is_mach_receive()) {
      receive_port_ = channel_handle.TakeMachReceiveRight();
    } else {
      NOTREACHED();
    }
  }

  ChannelMac(const ChannelMac&) = delete;
  ChannelMac& operator=(const ChannelMac&) = delete;

  void Start() override {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelMac::StartOnIOThread, this));
  }

  void ShutDownImpl() override {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelMac::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    base::AutoLock lock(write_lock_);

    if (reject_writes_) {
      return;
    }

    // If the channel is not fully established, queue pending messages.
    if (!handshake_done_) {
      pending_messages_.push_back(std::move(message));
      return;
    }

    // If messages are being queued, enqueue |message| and try to flush
    // the queue.
    if (send_buffer_contains_message_ || !pending_messages_.empty()) {
      pending_messages_.push_back(std::move(message));
      SendPendingMessagesLocked();
      return;
    }

    SendMessageLocked(std::move(message));
  }

  void LeakHandle() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    leak_handles_ = true;
  }

  bool GetReadPlatformHandles(const void* payload,
                              size_t payload_size,
                              size_t num_handles,
                              const void* extra_header,
                              size_t extra_header_size,
                              std::vector<PlatformHandle>* handles,
                              bool* deferred) override {
    // Validate the incoming handles. If validation fails, ensure they are
    // destroyed.
    std::vector<PlatformHandle> incoming_handles;
    std::swap(incoming_handles, incoming_handles_);

    if (extra_header_size <
        sizeof(Message::MachPortsExtraHeader) +
            (incoming_handles.size() * sizeof(Message::MachPortsEntry))) {
      return false;
    }

    const auto* mach_ports_header =
        reinterpret_cast<const Message::MachPortsExtraHeader*>(extra_header);
    if (mach_ports_header->num_ports != incoming_handles.size()) {
      return false;
    }

    for (uint16_t i = 0; i < mach_ports_header->num_ports; ++i) {
      auto type =
          static_cast<PlatformHandle::Type>(mach_ports_header->entries[i].type);
      if (type == PlatformHandle::Type::kNone) {
        return false;
      } else if (type == PlatformHandle::Type::kFd &&
                 incoming_handles[i].is_mach_send()) {
        int fd = fileport_makefd(incoming_handles[i].GetMachSendRight().get());
        if (fd < 0) {
          return false;
        }
        incoming_handles[i] = PlatformHandle(base::ScopedFD(fd));
      } else if (type != incoming_handles[i].type()) {
        return false;
      }
    }

    *handles = std::move(incoming_handles);
    return true;
  }

  // Unlike GetReadPlatformHandles(), this does not validate the underlying
  // PlatformHandle type here. Instead, ipcz does this in
  // ipcz::Message::DeserializeFromTransport(), which is called by
  // the AcceptParcel message deserializer (OnAcceptParcel).
  bool GetReadPlatformHandlesForIpcz(
      size_t num_handles,
      std::vector<PlatformHandle>& handles) override {
    if (incoming_handles_.size() != num_handles) {
      // ChannelMac messages are transmitted all at once or not at all, so this
      // method should always be invoked with the exact, correct number of
      // handles already in `incoming_handles_`.
      return false;
    }

    DCHECK(handles.empty());
    incoming_handles_.swap(handles);
    return true;
  }

 private:
  ~ChannelMac() override = default;

  void StartOnIOThread() {
    vm_address_t address = 0;
    const vm_size_t size = getpagesize();
    kern_return_t kr =
        vm_allocate(mach_task_self(), &address, size,
                    VM_MAKE_TAG(VM_MEMORY_MACH_MSG) | VM_FLAGS_ANYWHERE);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "vm_allocate";
    {
      base::AutoLock lock(write_lock_);
      send_buffer_.reset(address, size);
    }

    kr = vm_allocate(mach_task_self(), &address, size,
                     VM_MAKE_TAG(VM_MEMORY_MACH_MSG) | VM_FLAGS_ANYWHERE);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "vm_allocate";
    receive_buffer_.reset(address, size);

    // When a channel is created, it only has one end of communication (either
    // send or receive). If it was created with a receive port, the first thing
    // a channel does is receive a special channel-internal message containing
    // its peer's send right. If the channel was created with a send right, it
    // creates a new receive right and sends to its peer (using the send right
    // it was created with) a new send right to the receive right. This
    // establishes the bidirectional communication channel.
    if (send_port_ != MACH_PORT_NULL) {
      DCHECK(receive_port_ == MACH_PORT_NULL);
      CHECK(base::apple::CreateMachPort(&receive_port_, nullptr,
                                        MACH_PORT_QLIMIT_LARGE));
      if (!RequestSendDeadNameNotification()) {
        OnError(Error::kConnectionFailed);
        return;
      }
      SendHandshake();
    } else if (receive_port_ != MACH_PORT_NULL) {
      DCHECK(send_port_ == MACH_PORT_NULL);
      // Wait for the received message via the MessageLoop.
    } else {
      NOTREACHED();
    }

    base::CurrentThread::Get()->AddDestructionObserver(this);
    base::CurrentIOThread::Get()->WatchMachReceivePort(
        receive_port_.get(), &watch_controller_, this);
  }

  void ShutDownOnIOThread() {
    base::CurrentThread::Get()->RemoveDestructionObserver(this);

    watch_controller_.StopWatchingMachPort();

    {
      base::AutoLock lock(write_lock_);
      send_buffer_.reset();
      reject_writes_ = true;
    }
    receive_buffer_.reset();
    incoming_handles_.clear();

    if (leak_handles_) {
      std::ignore = receive_port_.release();
      std::ignore = send_port_.release();
    } else {
      receive_port_.reset();
      send_port_.reset();
    }

    // May destroy the |this| if it was the last reference.
    self_ = nullptr;
  }

  // Requests that the kernel notify the |receive_port_| when the receive right
  // connected to |send_port_| becomes a dead name. This should be called as
  // soon as the Channel establishes both the send and receive ports.
  bool RequestSendDeadNameNotification() {
    base::apple::ScopedMachSendRight previous;
    kern_return_t kr = mach_port_request_notification(
        mach_task_self(), send_port_.get(), MACH_NOTIFY_DEAD_NAME, 0,
        receive_port_.get(), MACH_MSG_TYPE_MAKE_SEND_ONCE,
        base::apple::ScopedMachSendRight::Receiver(previous).get());
    if (kr != KERN_SUCCESS) {
      // If port is already a dead name (i.e. the receiver is already gone),
      // then the channel should be shut down by the caller.
      MACH_LOG_IF(ERROR, kr != KERN_INVALID_ARGUMENT, kr)
          << "mach_port_request_notification";
      return false;
    }
    return true;
  }

  // SendHandshake() sends to the |receive_port_| a right to |send_port_|,
  // establishing bi-directional communication with the peer. After the
  // handshake message has been sent, this Channel can queue any pending
  // messages for its peer.
  void SendHandshake() {
    mach_msg_header_t message{};
    message.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
    message.msgh_size = sizeof(message);
    message.msgh_remote_port = send_port_.get();
    message.msgh_local_port = receive_port_.get();
    message.msgh_id = kChannelMacHandshakeMsgId;
    kern_return_t kr =
        mach_msg(&message, MACH_SEND_MSG, sizeof(message), 0, MACH_PORT_NULL,
                 MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
      MACH_LOG(ERROR, kr) << "mach_msg send handshake";

      base::AutoLock lock(write_lock_);
      OnWriteErrorLocked(Error::kConnectionFailed);
      return;
    }

    base::AutoLock lock(write_lock_);
    handshake_done_ = true;
    SendPendingMessagesLocked();
  }

  // Acquires the peer's send right from the handshake message sent via
  // SendHandshake(). After this, bi-directional communication is established
  // and this Channel can send to its peer any pending messages.
  bool ReceiveHandshake(base::BufferIterator<char>& buffer) {
    if (handshake_done_) {
      OnError(Error::kReceivedMalformedData);
      return false;
    }

    DCHECK(send_port_ == MACH_PORT_NULL);

    auto* message = buffer.Object<mach_msg_header_t>();
    if (message->msgh_id != kChannelMacHandshakeMsgId ||
        message->msgh_local_port == MACH_PORT_NULL) {
      OnError(Error::kConnectionFailed);
      return false;
    }

    send_port_ = base::apple::ScopedMachSendRight(message->msgh_remote_port);

    if (!RequestSendDeadNameNotification()) {
      send_port_.reset();
      OnError(Error::kConnectionFailed);
      return false;
    }

    // Record the audit token of the sender. All messages received by the
    // channel must be from this same sender.
    auto* trailer = buffer.Object<mach_msg_audit_trailer_t>();
    peer_audit_token_ = std::make_unique<audit_token_t>();
    memcpy(peer_audit_token_.get(), &trailer->msgh_audit,
           sizeof(audit_token_t));

    base::AutoLock lock(write_lock_);
    handshake_done_ = true;
    SendPendingMessagesLocked();

    return true;
  }

  void SendPendingMessagesLocked() EXCLUSIVE_LOCKS_REQUIRED(write_lock_) {
    // If a previous send failed due to the receiver's kernel message queue
    // being full, attempt to send that failed message first.
    if (send_buffer_contains_message_ && !reject_writes_) {
      auto* header =
          reinterpret_cast<mach_msg_header_t*>(send_buffer_.address());
      if (!MachMessageSendLocked(header)) {
        // The send failed again. If the peer is still unable to receive,
        // MachMessageSendLocked() will have arranged another attempt. If an
        // error occurred, the channel will be shut down.
        return;
      }
    }

    // Try and send any other pending messages that were queued.
    while (!pending_messages_.empty() && !reject_writes_) {
      bool did_send = SendMessageLocked(std::move(pending_messages_.front()));
      // If the message failed to send because the kernel message queue is
      // full, the message will have been fully serialized and
      // |send_buffer_contains_message_| will be set to true. The Mojo message
      // object can be destroyed at this point.
      pending_messages_.pop_front();
      if (!did_send)
        break;
    }
  }

  bool SendMessageLocked(MessagePtr message)
      EXCLUSIVE_LOCKS_REQUIRED(write_lock_) {
    DCHECK(!send_buffer_contains_message_);
    base::BufferIterator<char> buffer(
        reinterpret_cast<char*>(send_buffer_.address()), send_buffer_.size());

    auto* header = buffer.MutableObject<mach_msg_header_t>();
    *header = mach_msg_header_t{};

    std::vector<PlatformHandleInTransit> handles = message->TakeHandles();

    // Compute the total size of the message. If the message data are larger
    // than the allocated receive buffer, the data will be transferred out-of-
    // line. The receive buffer is the same size as the send buffer, but there
    // also needs to be room to receive the trailer.
    const size_t mach_header_size =
        sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t) +
        (handles.size() * sizeof(mach_msg_port_descriptor_t));
    const size_t expected_message_size =
        round_msg(mach_header_size + sizeof(uint64_t) +
                  message->data_num_bytes() + sizeof(mach_msg_audit_trailer_t));
    const bool transfer_message_ool =
        expected_message_size >= send_buffer_.size();

    const bool is_complex = !handles.empty() || transfer_message_ool;

    header->msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND) |
                        (is_complex ? MACH_MSGH_BITS_COMPLEX : 0);
    header->msgh_remote_port = send_port_.get();
    header->msgh_id =
        transfer_message_ool ? kChannelMacOOLMsgId : kChannelMacInlineMsgId;

    auto* body = buffer.MutableObject<mach_msg_body_t>();
    body->msgh_descriptor_count = handles.size();

    auto descriptors =
        buffer.MutableSpan<mach_msg_port_descriptor_t>(handles.size());
    for (size_t i = 0; i < handles.size(); ++i) {
      auto* descriptor = &descriptors[i];
      descriptor->pad1 = 0;
      descriptor->pad2 = 0;
      descriptor->type = MACH_MSG_PORT_DESCRIPTOR;

      PlatformHandle handle = handles[i].TakeHandle();

      switch (handle.type()) {
        case PlatformHandle::Type::kMachSend:
          descriptor->name = handle.ReleaseMachSendRight();
          descriptor->disposition = MACH_MSG_TYPE_MOVE_SEND;
          break;
        case PlatformHandle::Type::kMachReceive:
          descriptor->name = handle.ReleaseMachReceiveRight();
          descriptor->disposition = MACH_MSG_TYPE_MOVE_RECEIVE;
          break;
        case PlatformHandle::Type::kFd: {
          // After putting the FD in a fileport, the kernel will keep a
          // reference to the opened file, and the local descriptor can be
          // closed.
          kern_return_t kr =
              fileport_makeport(handle.GetFD().get(), &descriptor->name);
          if (kr != KERN_SUCCESS) {
            MACH_LOG(ERROR, kr) << "fileport_makeport";
            OnWriteErrorLocked(Error::kDisconnected);
            return false;
          }
          descriptor->disposition = MACH_MSG_TYPE_MOVE_SEND;
          break;
        }
        default:
          NOTREACHED() << "Unsupported handle type "
                       << static_cast<int>(handle.type());
      }
    }

    if (transfer_message_ool) {
      auto* descriptor = buffer.MutableObject<mach_msg_ool_descriptor_t>();
      descriptor->address = const_cast<void*>(message->data());
      descriptor->size = message->data_num_bytes();
      descriptor->copy = MACH_MSG_VIRTUAL_COPY;
      descriptor->deallocate = false;
      descriptor->pad1 = 0;
      descriptor->type = MACH_MSG_OOL_DESCRIPTOR;
      ++body->msgh_descriptor_count;
    } else {
      // Mach message structs are all 4-byte aligned, but `uint64_t` is 8-byte
      // aligned on 64-bit architectures. To avoid alignment issues, write the
      // size as bytes.
      buffer.MutableSpan<uint8_t, 8>()->copy_from(
          base::U64ToNativeEndian(message->data_num_bytes()));

      auto data = buffer.MutableSpan<char>(message->data_num_bytes());
      memcpy(data.data(), message->data(), message->data_num_bytes());
    }

    header->msgh_size = round_msg(buffer.position());
    return MachMessageSendLocked(header);
  }

  bool MachMessageSendLocked(mach_msg_header_t* header)
      EXCLUSIVE_LOCKS_REQUIRED(write_lock_) {
    kern_return_t kr = mach_msg(header, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                                header->msgh_size, 0, MACH_PORT_NULL,
                                /*timeout=*/0, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
      if (kr == MACH_SEND_TIMED_OUT) {
        // The kernel message queue for the peer's receive port is full, so the
        // send timed out. Since the send buffer contains a fully serialized
        // message, set a flag to indicate this condition.
        send_buffer_contains_message_ = true;
        if (!is_retry_scheduled_) {
          // Arrange to retry sending the message again. Set a flag to ensure
          // that this does not build up a flood of tasks to retry it, which
          // could happen if Write() is called (potentially from a different
          // thread), and the receiver's queue is still blocked.
          io_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&ChannelMac::RetrySendPendingMessages, this));
          is_retry_scheduled_ = true;
        }
      } else {
        // If the message failed to send for other reasons, destroy it.
        send_buffer_contains_message_ = false;
        mach_msg_destroy(header);
        if (kr != MACH_SEND_INVALID_DEST) {
          // If the message failed to send because the receiver is a dead-name,
          // wait for the Channel to process the dead-name notification.
          // Otherwise, the notification message will never be received and the
          // dead-name right contained within it will be leaked
          // (https://crbug.com/1041682). If the message failed to send for any
          // other reason, report an error and shut down.
          MACH_LOG(ERROR, kr) << "mach_msg send";
          OnWriteErrorLocked(Error::kDisconnected);
        }
      }
      return false;
    }

    send_buffer_contains_message_ = false;
    return true;
  }

  void RetrySendPendingMessages() {
    base::AutoLock lock(write_lock_);
    is_retry_scheduled_ = false;
    SendPendingMessagesLocked();
  }

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessagePumpKqueue::MachPortWatcher:
  void OnMachMessageReceived(mach_port_t port) override {
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("toplevel.ipc"), "Mojo read message");

    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

    base::BufferIterator<char> buffer(
        reinterpret_cast<char*>(receive_buffer_.address()),
        receive_buffer_.size());
    auto* header = buffer.MutableObject<mach_msg_header_t>();
    *header = mach_msg_header_t{};
    header->msgh_size = buffer.total_size();
    header->msgh_local_port = receive_port_.get();

    const mach_msg_option_t rcv_options =
        MACH_RCV_MSG | MACH_RCV_TIMEOUT |
        MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
        MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);
    kern_return_t kr =
        mach_msg(header, rcv_options, 0, header->msgh_size, receive_port_.get(),
                 /*timeout=*/0, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
      if (kr == MACH_RCV_TIMED_OUT)
        return;
      MACH_LOG(ERROR, kr) << "mach_msg receive";
      OnError(Error::kDisconnected);
      return;
    }

    base::ScopedMachMsgDestroy scoped_message(header);

    if (header->msgh_id == kChannelMacHandshakeMsgId) {
      buffer.Seek(0);
      if (ReceiveHandshake(buffer))
        scoped_message.Disarm();
      return;
    }

    if (header->msgh_id == MACH_NOTIFY_DEAD_NAME) {
      // The DEAD_NAME notification contains a port right that must be
      // explicitly destroyed, as it is not carried in a descriptor.
      buffer.Seek(0);
      auto* notification = buffer.Object<mach_dead_name_notification_t>();

      // Verify that the kernel sent the notification.
      buffer.Seek(notification->not_header.msgh_size);
      auto* trailer = buffer.Object<mach_msg_audit_trailer_t>();
      static const audit_token_t kernel_audit_token = KERNEL_AUDIT_TOKEN_VALUE;
      if (memcmp(&trailer->msgh_audit, &kernel_audit_token,
                 sizeof(audit_token_t)) == 0) {
        DCHECK(notification->not_port == send_port_);
        // Release the notification's send right using this scoper.
        base::apple::ScopedMachSendRight notify_port(notification->not_port);
      }
      OnError(Error::kDisconnected);
      return;
    } else if (header->msgh_id == MACH_NOTIFY_SEND_ONCE) {
      // Notification of an extant send-once right being destroyed. This is
      // sent for the right allocated in RequestSendDeadNameNotification(),
      // and no action needs to be taken. Since it is ignored, the kernel
      // audit token need not be checked.
      return;
    }

    if (header->msgh_size < sizeof(mach_msg_base_t)) {
      OnError(Error::kReceivedMalformedData);
      return;
    }

    if (peer_audit_token_) {
      buffer.Seek(header->msgh_size);
      auto* trailer = buffer.Object<mach_msg_audit_trailer_t>();
      if (memcmp(&trailer->msgh_audit, peer_audit_token_.get(),
                 sizeof(audit_token_t)) != 0) {
        // Do not shut down the channel because this endpoint could be
        // accessible via the bootstrap server, which means anyone could send
        // messages to it.
        LOG(ERROR) << "Rejecting message from unauthorized peer";
        return;
      }
      buffer.Seek(sizeof(*header));
    }

    auto* body = buffer.Object<mach_msg_body_t>();
    if (((header->msgh_bits & MACH_MSGH_BITS_COMPLEX) != 0) !=
        (body->msgh_descriptor_count > 0)) {
      LOG(ERROR) << "Message complex bit does not match descriptor count";
      OnError(Error::kReceivedMalformedData);
      return;
    }

    bool transfer_message_ool = false;
    mach_msg_size_t mojo_handle_count = body->msgh_descriptor_count;
    if (header->msgh_id == kChannelMacOOLMsgId) {
      transfer_message_ool = true;
      // The number of Mojo handles to process will be one fewer, since the
      // message itself was transferred using OOL memory.
      if (body->msgh_descriptor_count < 1) {
        LOG(ERROR) << "OOL message does not have descriptor";
        OnError(Error::kReceivedMalformedData);
        return;
      }
      --mojo_handle_count;
    } else if (header->msgh_id != kChannelMacInlineMsgId) {
      OnError(Error::kReceivedMalformedData);
      return;
    }

    incoming_handles_.clear();
    incoming_handles_.reserve(mojo_handle_count);

    // Accept the descriptors into |incoming_handles_|. They will be validated
    // in GetReadPlatformHandles(). If the handle is accepted, the name in the
    // descriptor is cleared, so that it is not double-unrefed if the
    // |scoped_message| destroys the message on error.
    auto descriptors =
        buffer.MutableSpan<mach_msg_port_descriptor_t>(mojo_handle_count);
    for (auto& descriptor : descriptors) {
      if (descriptor.type != MACH_MSG_PORT_DESCRIPTOR) {
        LOG(ERROR) << "Incorrect descriptor type " << descriptor.type;
        OnError(Error::kReceivedMalformedData);
        return;
      }
      switch (descriptor.disposition) {
        case MACH_MSG_TYPE_MOVE_SEND:
          incoming_handles_.emplace_back(
              base::apple::ScopedMachSendRight(descriptor.name));
          descriptor.name = MACH_PORT_NULL;
          break;
        case MACH_MSG_TYPE_MOVE_RECEIVE:
          incoming_handles_.emplace_back(
              base::apple::ScopedMachReceiveRight(descriptor.name));
          descriptor.name = MACH_PORT_NULL;
          break;
        default:
          DLOG(ERROR) << "Unhandled descriptor disposition "
                      << descriptor.disposition;
          OnError(Error::kReceivedMalformedData);
          return;
      }
    }

    base::span<const char> payload;
    base::apple::ScopedMachVM ool_memory;
    if (transfer_message_ool) {
      auto* descriptor = buffer.Object<mach_msg_ool_descriptor_t>();
      if (descriptor->type != MACH_MSG_OOL_DESCRIPTOR) {
        LOG(ERROR) << "Incorrect descriptor type " << descriptor->type;
        OnError(Error::kReceivedMalformedData);
        return;
      }

      payload = base::span<const char>(
          reinterpret_cast<const char*>(descriptor->address), descriptor->size);
      // The kernel page-aligns the OOL memory when performing the mach_msg on
      // the send side, but it preserves the original size in the descriptor.
      ool_memory.reset_unaligned(
          reinterpret_cast<vm_address_t>(descriptor->address),
          descriptor->size);
    } else {
      // Mach message structs are all 4-byte aligned, but `uint64_t` is 8-byte
      // aligned on 64-bit architectures. To avoid alignment issues, write the
      // size as bytes.
      uint64_t data_size =
          base::U64FromNativeEndian(*buffer.Span<uint8_t, 8>());
      payload = buffer.Span<const char>(data_size);
    }

    if (payload.empty()) {
      OnError(Error::kReceivedMalformedData);
      return;
    }

    scoped_message.Disarm();

    size_t ignored;
    DispatchResult result = TryDispatchMessage(payload, &ignored);
    if (result != DispatchResult::kOK) {
      OnError(Error::kReceivedMalformedData);
      return;
    }
  }

  // Marks the channel as unaccepting of new messages and shuts it down.
  void OnWriteErrorLocked(Error error) EXCLUSIVE_LOCKS_REQUIRED(write_lock_) {
    reject_writes_ = true;
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelMac::OnError, this, error));
  }

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<ChannelMac> self_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::apple::ScopedMachReceiveRight receive_port_;
  base::apple::ScopedMachSendRight send_port_;

  // Whether to leak the above Mach ports when the channel is shut down.
  bool leak_handles_ = false;

  // Whether or not the channel-internal handshake, which establishes bi-
  // directional communication, is complete. If false, calls to Write() will
  // enqueue messages on |pending_messages_|.
  bool handshake_done_ = false;

  // If the channel was created with a receive right, the first message it
  // receives is the internal handshake. The audit token of the sender of the
  // handshake is recorded here, and all future messages are required to be
  // from that sender.
  std::unique_ptr<audit_token_t> peer_audit_token_;

  // IO buffer for receiving Mach messages. Only accessed on |io_task_runner_|.
  base::apple::ScopedMachVM receive_buffer_;

  // Handles that were received with a message that are validated and returned
  // in GetReadPlatformHandles(). Only accessed on |io_task_runner_|.
  std::vector<PlatformHandle> incoming_handles_;

  // Watch controller for |receive_port_|, calls OnMachMessageReceived() when
  // new messages are available.
  base::MessagePumpForIO::MachPortWatchController watch_controller_;

  // Lock that protects the following members.
  base::Lock write_lock_;
  // Whether writes should be rejected due to an internal error or channel
  // shutdown.
  bool reject_writes_ GUARDED_BY(write_lock_) = false;
  // IO buffer for sending Mach messages.
  base::apple::ScopedMachVM send_buffer_ GUARDED_BY(write_lock_);
  // If a message timed out during send in MachMessageSendLocked(), this will
  // be true to indicate that |send_buffer_| contains a message that must
  // be sent. If this is true, then other calls to Write() queue messages onto
  // |pending_messages_|.
  bool send_buffer_contains_message_ GUARDED_BY(write_lock_) = false;
  // If |send_buffer_contains_message_| is true, this boolean tracks whether
  // a task to RetrySendPendingMessages() has been posted. There should only be
  // one retry task in-flight at once.
  bool is_retry_scheduled_ GUARDED_BY(write_lock_) = false;
  // When |handshake_done_| is false or |send_buffer_contains_message_| is true,
  // calls to Write() will enqueue messages here.
  base::circular_deque<MessagePtr> pending_messages_ GUARDED_BY(write_lock_);
};

}  // namespace

MOJO_SYSTEM_IMPL_EXPORT
scoped_refptr<Channel> Channel::Create(
    Channel::Delegate* delegate,
    ConnectionParams connection_params,
    Channel::HandlePolicy handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return new ChannelMac(delegate, std::move(connection_params), handle_policy,
                        io_task_runner);
}

}  // namespace core
}  // namespace mojo
