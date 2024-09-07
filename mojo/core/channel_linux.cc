// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/channel_linux.h"

#include <fcntl.h>
#include <linux/futex.h>
#include <linux/memfd.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <memory>

#include "base/bits.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_security_policy.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/buildflags.h"
#include "mojo/core/embedder/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#ifndef EFD_ZERO_ON_WAKE
#define EFD_ZERO_ON_WAKE O_NOFOLLOW
#endif

namespace mojo {
namespace core {

namespace {

// On Android base::SysInfo::OperatingSystemVersionNumbers actually returns the
// build numbers and not the kernel version as the other posix OSes would.
void KernelVersionNumbers(int32_t* major_version,
                          int32_t* minor_version,
                          int32_t* bugfix_version) {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
  }
  int num_read = sscanf(info.release, "%d.%d.%d", major_version, minor_version,
                        bugfix_version);
  if (num_read < 1)
    *major_version = 0;
  if (num_read < 2)
    *minor_version = 0;
  if (num_read < 3)
    *bugfix_version = 0;
}

}  // namespace

// DataAvailableNotifier is a simple interface which allows us to
// substitute how we notify the reader that we've made data available,
// implementations might be EventFDNotifier or FutexNotifier.
class DataAvailableNotifier {
 public:
  DataAvailableNotifier() = default;
  explicit DataAvailableNotifier(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {}

  virtual ~DataAvailableNotifier() = default;

  // The writer should notify the reader by invoking Notify.
  virtual bool Notify() = 0;

  // A reader should clear the notification (if appropriate) by calling Clear.
  virtual bool Clear() = 0;

  // Is_valid will return true if the implementation is valid and can be used.
  virtual bool is_valid() const = 0;

 protected:
  // DataAvailable will be called by implementations of DataAvailableNotifier to
  // dispatch this message into the registered callback.
  void DataAvailable() {
    DCHECK(callback_);
    callback_.Run();
  }

  base::RepeatingClosure callback_;
};

namespace {

constexpr int kMemFDSeals = F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW;

std::atomic_bool g_params_set{false};
std::atomic_bool g_use_shared_mem{false};
std::atomic_bool g_use_zero_on_wake{false};
std::atomic_uint32_t g_shared_mem_pages{4};

struct UpgradeOfferMessage {
  constexpr static int kEventFdNotifier = 1;
  constexpr static int kEventFdZeroWakeNotifier = 2;

  constexpr static int kDefaultVersion = kEventFdNotifier;
  constexpr static int kDefaultPages = 4;

  static bool IsValidVersion(int version) {
    return (version == kEventFdNotifier || version == kEventFdZeroWakeNotifier);
  }

  int version = kDefaultVersion;
  int num_pages = kDefaultPages;
};

constexpr size_t RoundUpToWordBoundary(size_t size) {
  return base::bits::AlignUp(size, sizeof(void*));
}

base::ScopedFD CreateSealedMemFD(size_t size) {
  CHECK_GT(size, 0u);
  CHECK_EQ(size % base::GetPageSize(), 0u);
  base::ScopedFD fd(syscall(__NR_memfd_create, "mojo_channel_linux",
                            MFD_CLOEXEC | MFD_ALLOW_SEALING));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Unable to create memfd for shared memory channel";
    return {};
  }

  if (ftruncate(fd.get(), size) < 0) {
    PLOG(ERROR) << "Unable to truncate memfd for shared memory channel";
    return {};
  }

  // We make sure to use F_SEAL_SEAL to prevent any further changes to the
  // seals and F_SEAL_SHRINK guarantees that we won't accidentally decrease
  // the size, and similarly F_SEAL_GROW for increasing size.
  if (fcntl(fd.get(), F_ADD_SEALS, kMemFDSeals) < 0) {
    PLOG(ERROR) << "Unable to seal memfd for shared memory channel";
    return {};
  }

  return fd;
}

// It's very important that we always verify that the FD we're passing and the
// FD we're receive is a properly sealed MemFD.
bool ValidateFDIsProperlySealedMemFD(const base::ScopedFD& fd) {
  int seals = 0;
  if ((seals = fcntl(fd.get(), F_GET_SEALS)) < 0) {
    PLOG(ERROR) << "Unable to get seals on memfd for shared memory channel";
    return false;
  }

  return seals == kMemFDSeals;
}

// EventFDNotifier is an implementation of the DataAvailableNotifier interface
// which uses EventFDNotifier to signal the reader.
class EventFDNotifier : public DataAvailableNotifier,
                        public base::MessagePumpForIO::FdWatcher {
 public:
  EventFDNotifier(EventFDNotifier&& efd) = default;

  EventFDNotifier(const EventFDNotifier&) = delete;
  EventFDNotifier& operator=(const EventFDNotifier&) = delete;

  ~EventFDNotifier() override { reset(); }

  static constexpr int kEfdFlags = EFD_CLOEXEC | EFD_NONBLOCK;

  static std::unique_ptr<EventFDNotifier> CreateWriteNotifier() {
    static bool zero_on_wake_supported = []() -> bool {
      base::ScopedFD fd(
          syscall(__NR_eventfd2, 0, kEfdFlags | EFD_ZERO_ON_WAKE));
      return fd.is_valid();
    }();

    bool use_zero_on_wake = zero_on_wake_supported && g_use_zero_on_wake;
    int extra_flags = use_zero_on_wake ? EFD_ZERO_ON_WAKE : 0;
    int fd = syscall(__NR_eventfd2, 0, kEfdFlags | extra_flags);
    if (fd < 0) {
      PLOG(ERROR) << "Unable to create an eventfd";
      return nullptr;
    }

    return WrapFD(base::ScopedFD(fd), use_zero_on_wake);
  }

  // The EventFD read notifier MUST be created on the IOThread. Luckily you're
  // typically creating the read notifier in response to an OFFER_UPGRADE
  // message which was received on the IOThread.
  static std::unique_ptr<EventFDNotifier> CreateReadNotifier(
      base::ScopedFD efd,
      base::RepeatingClosure cb,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      bool zero_on_wake) {
    DCHECK(io_task_runner->RunsTasksInCurrentSequence());
    DCHECK(cb);

    return WrapFDWithCallback(std::move(efd), std::move(cb), io_task_runner,
                              zero_on_wake);
  }

  static bool KernelSupported() {
    // Try to create an eventfd with bad flags if we get -EINVAL it's supported
    // if we get -ENOSYS it's not, we also support -EPERM because seccomp
    // policies can cause it to be returned.
    int ret = syscall(__NR_eventfd2, 0, ~0);
    PCHECK(ret < 0 && (errno == EINVAL || errno == ENOSYS || errno == EPERM));
    return (ret < 0 && errno == EINVAL);
  }

  // DataAvailableNotifier impl:
  bool Clear() override {
    // When using EFD_ZERO_ON_WAKE we don't have to do anything.
    if (zero_on_wake_) {
      return true;
    }

    uint64_t value = 0;
    ssize_t res = HANDLE_EINTR(
        read(fd_.get(), reinterpret_cast<void*>(&value), sizeof(value)));
    if (res < static_cast<int64_t>(sizeof(value))) {
      PLOG_IF(ERROR, errno != EWOULDBLOCK) << "eventfd read error";
    }
    return res == sizeof(value);
  }

  bool Notify() override {
    uint64_t value = 1;
    ssize_t res = HANDLE_EINTR(write(fd_.get(), &value, sizeof(value)));
    return res == sizeof(value);
  }

  bool is_valid() const override { return fd_.is_valid(); }

  // base::MessagePumpForIO::FdWatcher impl:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(fd == fd_.get());

    // Invoke the callback to inform them that data is available to read.
    DataAvailable();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {}

  base::ScopedFD take() { return std::move(fd_); }
  base::ScopedFD take_dup() {
    return base::ScopedFD(HANDLE_EINTR(dup(fd_.get())));
  }

  void reset() {
    watcher_.reset();
    fd_.reset();
  }

  int fd() { return fd_.get(); }

  bool zero_on_wake() const { return zero_on_wake_; }

 private:
  explicit EventFDNotifier(base::ScopedFD fd, bool zero_on_wake)
      : zero_on_wake_(zero_on_wake), fd_(std::move(fd)) {}
  explicit EventFDNotifier(
      base::ScopedFD fd,
      base::RepeatingClosure cb,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      bool zero_on_wake)
      : DataAvailableNotifier(std::move(cb)),
        zero_on_wake_(zero_on_wake),
        fd_(std::move(fd)),
        io_task_runner_(io_task_runner) {
    watcher_ =
        std::make_unique<base::MessagePumpForIO::FdWatchController>(FROM_HERE);
    WaitForEventFDOnIOThread();
  }

  static std::unique_ptr<EventFDNotifier> WrapFD(base::ScopedFD fd,
                                                 bool zero_on_wake) {
    return base::WrapUnique<EventFDNotifier>(
        new EventFDNotifier(std::move(fd), zero_on_wake));
  }

  static std::unique_ptr<EventFDNotifier> WrapFDWithCallback(
      base::ScopedFD fd,
      base::RepeatingClosure cb,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      bool zero_on_wake) {
    return base::WrapUnique<EventFDNotifier>(new EventFDNotifier(
        std::move(fd), std::move(cb), io_task_runner, zero_on_wake));
  }

  void WaitForEventFDOnIOThread() {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    base::CurrentIOThread::Get()->WatchFileDescriptor(
        fd_.get(), true, base::MessagePumpForIO::WATCH_READ, watcher_.get(),
        this);
  }

  bool zero_on_wake_ = false;
  base::ScopedFD fd_;
  std::unique_ptr<base::MessagePumpForIO::FdWatchController> watcher_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

}  // namespace

// SharedBuffer is an abstraction around a region of shared memory, it has
// methods to facilitate safely reading and writing into the shared region.
// SharedBuffer only handles the access to the shared memory any notifications
// must be performed separately.
class ChannelLinux::SharedBuffer {
 public:
  SharedBuffer(SharedBuffer&& other) = default;

  SharedBuffer(const SharedBuffer&) = delete;
  SharedBuffer& operator=(const SharedBuffer&) = delete;

  ~SharedBuffer() { reset(); }

  enum class Error { kSuccess = 0, kGeneralError = 1, kControlCorruption = 2 };

  static std::unique_ptr<SharedBuffer> Create(const base::ScopedFD& memfd,
                                              size_t size) {
    if (!memfd.is_valid()) {
      return nullptr;
    }

    // Enforce the system shared memory security policy.
    if (!base::SharedMemorySecurityPolicy::AcquireReservationForMapping(size)) {
      LOG(ERROR)
          << "Unable to create shared buffer: unable to acquire reservation";
      return nullptr;
    }

    uint8_t* ptr = reinterpret_cast<uint8_t*>(mmap(
        nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, memfd.get(), 0));

    if (ptr == MAP_FAILED) {
      PLOG(ERROR) << "Unable to map shared memory";

      // Always clean up our reservation if we actually fail to map.
      base::SharedMemorySecurityPolicy::ReleaseReservationForMapping(size);
      return nullptr;
    }

    return base::WrapUnique<SharedBuffer>(new SharedBuffer(ptr, size));
  }

  uint8_t* usable_region_ptr() { return base_ptr_ + kReservedSpace; }
  size_t usable_len() const { return len_ - kReservedSpace; }
  bool is_valid() const { return base_ptr_ != nullptr && len_ > 0; }

  void reset() {
    if (is_valid()) {
      if (munmap(base_ptr_, len_) < 0) {
        PLOG(ERROR) << "Unable to unmap shared buffer";
        return;
      }

      base::SharedMemorySecurityPolicy::ReleaseReservationForMapping(len_);
      base_ptr_ = nullptr;
      len_ = 0;
    }
  }

  // Only one side should call Initialize, this will initialize the first
  // sizeof(ControlStructure) bytes as our control structure. This should be
  // done when offering fast comms.
  void Initialize() { new (static_cast<void*>(base_ptr_)) ControlStructure; }

  // TryWrite will attempt to append |data| of |len| to the shared buffer, this
  // call will only succeed if there is no one else trying to write AND there is
  // enough space currently in the buffer.
  Error TryWrite(const void* data, size_t len) {
    DCHECK(data);
    DCHECK(len);

    if (len > usable_len()) {
      return Error::kGeneralError;
    }

    if (!TryLockForWriting()) {
      return Error::kGeneralError;
    }

    // At this point we know that the space available can only grow because
    // we're the only writer we will write from write_pos -> end and 0 -> (len
    // - (end - write_pos)) where end is usable_len().
    uint32_t cur_read_pos = read_pos().load();
    uint32_t cur_write_pos = write_pos().load();

    if (!ValidateReadWritePositions(cur_read_pos, cur_write_pos)) {
      UnlockForWriting();
      return Error::kControlCorruption;
    }

    uint32_t space_available =
        usable_len() - NumBytesInUse(cur_read_pos, cur_write_pos);

    if (space_available <= len) {
      UnlockForWriting();

      return Error::kGeneralError;
    }

    // If we do not have enough space from the current write position to the end
    // then we will be forced to wrap around. If we do have enough space we can
    // just start writing at the write position, otherwise we start writing at
    // the write position up to the end of the usable area and then we write the
    // remainder of the payload starting at position 0.
    if ((usable_len() - cur_write_pos) > len) {
      memcpy(usable_region_ptr() + cur_write_pos, data, len);
    } else {
      size_t copy1_len = usable_len() - cur_write_pos;
      memcpy(usable_region_ptr() + cur_write_pos, data, copy1_len);
      memcpy(usable_region_ptr(),
             reinterpret_cast<const uint8_t*>(data) + copy1_len,
             len - copy1_len);
    }

    // Atomically update the write position.
    // We also verify that the write position did not advance, it SHOULD NEVER
    // advance since we were holding the write lock.
    if (write_pos().exchange((cur_write_pos + len) % usable_len()) !=
        cur_write_pos) {
      UnlockForWriting();
      return Error::kControlCorruption;
    }

    UnlockForWriting();

    return Error::kSuccess;
  }

  Error TryReadLocked(void* data, uint32_t len, uint32_t* bytes_read) {
    uint32_t cur_read_pos = read_pos().load();
    uint32_t cur_write_pos = write_pos().load();

    if (!ValidateReadWritePositions(cur_read_pos, cur_write_pos)) {
      return Error::kControlCorruption;
    }

    // The most we can read is the smaller of what's in use in the shared memory
    // usable area and the buffer size we've been passed.
    uint32_t bytes_available_to_read =
        NumBytesInUse(cur_read_pos, cur_write_pos);
    bytes_available_to_read = std::min(bytes_available_to_read, len);
    if (bytes_available_to_read == 0) {
      *bytes_read = 0;
      return Error::kSuccess;
    }

    // We have two cases when reading, the first is the read position is behind
    // the write position, in that case we can simply read all data between the
    // read and write position (up to our buffer size). The second case is when
    // the write position is behind the read position. In this situation we must
    // read from the read position to the end of the available area, and
    // continue reading from the 0 position up to the write position or the
    // maximum buffer size (bytes_available_to_read).
    if (cur_read_pos < cur_write_pos) {
      memcpy(data, usable_region_ptr() + cur_read_pos, bytes_available_to_read);
    } else {
      // We first start by reading to the end of the the usable area, if we
      // cannot read all the way (because our buffer is too small, we're done).
      uint32_t bytes_from_read_to_end = usable_len() - cur_read_pos;
      bytes_from_read_to_end =
          std::min(bytes_from_read_to_end, bytes_available_to_read);
      memcpy(data, usable_region_ptr() + cur_read_pos, bytes_from_read_to_end);

      if (bytes_from_read_to_end < bytes_available_to_read) {
        memcpy(reinterpret_cast<uint8_t*>(data) + bytes_from_read_to_end,
               usable_region_ptr(),
               bytes_available_to_read - bytes_from_read_to_end);
      }
    }

    // Atomically update the read position.
    // We also verify that the read position did not advance, it SHOULD NEVER
    // advance since we were holding the read lock.
    uint32_t new_read_pos =
        (cur_read_pos + bytes_available_to_read) % usable_len();
    if (read_pos().exchange(new_read_pos) != cur_read_pos) {
      *bytes_read = 0;
      return Error::kControlCorruption;
    }

    *bytes_read = bytes_available_to_read;
    return Error::kSuccess;
  }

  bool TryLockForReading() {
    // We return true if we set the flag (meaning it was false).
    return !read_flag().test_and_set(std::memory_order_acquire);
  }

  void UnlockForReading() { read_flag().clear(std::memory_order_release); }

 private:
  struct ControlStructure {
    std::atomic_flag write_flag{false};
    std::atomic_uint32_t write_pos{0};

    std::atomic_flag read_flag{false};
    std::atomic_uint32_t read_pos{0};

    // If we're using a notification mechanism that relies on futex, make the
    // space available for one, if not these 32bits are unused. The kernel
    // requires they be 32bit aligned.
    alignas(4) volatile uint32_t futex = 0;
  };

  // This function will only validate that the values provided for write and
  // read positions are valid based on usable size of the shared memory region.
  // This should ALWAYS be called before attempting a write or read using
  // atomically loaded values from the control structure.
  bool ValidateReadWritePositions(uint32_t read_pos, uint32_t write_pos) {
    // The only valid values for read and write positions are [0 - usable_len
    // - 1].
    if (write_pos >= usable_len()) {
      LOG(ERROR) << "Write position of shared buffer is currently beyond the "
                    "usable length";
      return false;
    }

    if (read_pos >= usable_len()) {
      LOG(ERROR) << "Read position of shared buffer is currently beyond the "
                    "usable length";
      return false;
    }

    return true;
  }

  // NumBytesInUse will calculate how many bytes in the shared buffer are
  // currently in use.
  uint32_t NumBytesInUse(uint32_t read_pos, uint32_t write_pos) {
    uint32_t bytes_in_use = 0;
    if (read_pos <= write_pos) {
      bytes_in_use = write_pos - read_pos;
    } else {
      bytes_in_use = write_pos + (usable_len() - read_pos);
    }

    return bytes_in_use;
  }

  bool TryLockForWriting() {
    // We return true if we set the flag (meaning it was false).
    return !write_flag().test_and_set(std::memory_order_acquire);
  }

  void UnlockForWriting() { write_flag().clear(std::memory_order_release); }

  // This is the space we need to reserve in this shared buffer for our control
  // structure at the start.
  constexpr static size_t kReservedSpace =
      RoundUpToWordBoundary(sizeof(ControlStructure));

  std::atomic_flag& write_flag() {
    DCHECK(is_valid());
    return reinterpret_cast<ControlStructure*>(base_ptr_)->write_flag;
  }

  std::atomic_flag& read_flag() {
    DCHECK(is_valid());
    return reinterpret_cast<ControlStructure*>(base_ptr_)->read_flag;
  }

  std::atomic_uint32_t& read_pos() {
    DCHECK(is_valid());
    return reinterpret_cast<ControlStructure*>(base_ptr_)->read_pos;
  }

  std::atomic_uint32_t& write_pos() {
    DCHECK(is_valid());
    return reinterpret_cast<ControlStructure*>(base_ptr_)->write_pos;
  }

  SharedBuffer(uint8_t* ptr, size_t len) : base_ptr_(ptr), len_(len) {}

  // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always mmap'ed), so
  // there is no benefit to using a raw_ptr, only cost.
  RAW_PTR_EXCLUSION uint8_t* base_ptr_ = nullptr;
  size_t len_ = 0;
};

ChannelLinux::ChannelLinux(
    Delegate* delegate,
    ConnectionParams connection_params,
    HandlePolicy handle_policy,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : ChannelPosix(delegate,
                   std::move(connection_params),
                   handle_policy,
                   io_task_runner),
      num_pages_(g_shared_mem_pages.load()) {}

ChannelLinux::~ChannelLinux() = default;

void ChannelLinux::Write(MessagePtr message) {
  if (!shared_mem_writer_ || message->has_handles() || reject_writes_) {
    // Let the ChannelPosix deal with this.
    return ChannelPosix::Write(std::move(message));
  }

  // Can we use the fast shared memory buffer?
  SharedBuffer::Error write_result =
      write_buffer_->TryWrite(message->data(), message->data_num_bytes());
  if (write_result == SharedBuffer::Error::kGeneralError) {
    // We can handle this with the posix channel.
    return ChannelPosix::Write(std::move(message));
  } else if (write_result == SharedBuffer::Error::kControlCorruption) {
    // We will no longer be issuing writes via shared memory, and we will
    // dispatch a write error.
    reject_writes_ = true;

    // Theoretically we could fall back to only using PosixChannel::Write
    // but if this situation happens it's likely something else is going
    // horribly wrong.
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelLinux::OnWriteError, this,
                                  Channel::Error::kReceivedMalformedData));
    return;
  }

  //  The write with shared memory was successful.
  write_notifier_->Notify();
}

void ChannelLinux::OfferSharedMemUpgrade() {
  if (!offered_.test_and_set() && UpgradesEnabled()) {
    // Before we offer we need to make sure we can send handles, if we can't
    // then no point in trying.
    if (handle_policy() == HandlePolicy::kAcceptHandles) {
      OfferSharedMemUpgradeInternal();
    }
  }
}

bool ChannelLinux::OnControlMessage(Message::MessageType message_type,
                                    const void* payload,
                                    size_t payload_size,
                                    std::vector<PlatformHandle> handles) {
  switch (message_type) {
    case Message::MessageType::UPGRADE_OFFER: {
      if (payload_size < sizeof(UpgradeOfferMessage)) {
        LOG(ERROR) << "Received an UPGRADE_OFFER without a payload";
        return true;
      }

      const UpgradeOfferMessage* msg =
          reinterpret_cast<const UpgradeOfferMessage*>(payload);
      if (!UpgradeOfferMessage::IsValidVersion(msg->version)) {
        LOG(ERROR) << "Reject shared mem upgrade unexpected version: "
                   << msg->version;
        RejectUpgradeOffer();
        return true;
      }

      if (handles.size() != 2) {
        LOG(ERROR) << "Received an UPGRADE_OFFER without two FDs";
        RejectUpgradeOffer();
        return true;
      }

      if (read_buffer_ || read_notifier_) {
        LOG(ERROR) << "Received an UPGRADE_OFFER on already upgraded channel";
        return true;
      }

      base::ScopedFD memfd(handles[0].TakeFD());
      if (memfd.is_valid() && !ValidateFDIsProperlySealedMemFD(memfd)) {
        PLOG(ERROR) << "Passed FD was not properly sealed";
        DLOG(FATAL) << "MemFD was NOT properly sealed";
        memfd.reset();
      }

      if (!memfd.is_valid()) {
        RejectUpgradeOffer();
        return true;
      }

      if (msg->num_pages <= 0 || msg->num_pages > 128) {
        LOG(ERROR) << "SharedMemory upgrade offer was received with invalid "
                      "number of pages: "
                   << msg->num_pages;
        RejectUpgradeOffer();
      }

      std::unique_ptr<DataAvailableNotifier> read_notifier;
      if (msg->version == UpgradeOfferMessage::kEventFdNotifier ||
          msg->version == UpgradeOfferMessage::kEventFdZeroWakeNotifier) {
        bool zero_on_wake =
            msg->version == UpgradeOfferMessage::kEventFdZeroWakeNotifier;
        read_notifier = EventFDNotifier::CreateReadNotifier(
            handles[1].TakeFD(),
            base::BindRepeating(&ChannelLinux::SharedMemReadReady, this),
            io_task_runner_, zero_on_wake);
      }

      if (!read_notifier) {
        RejectUpgradeOffer();
        return true;
      }

      read_notifier_ = std::move(read_notifier);

      std::unique_ptr<SharedBuffer> read_sb = SharedBuffer::Create(
          std::move(memfd), msg->num_pages * base::GetPageSize());
      if (!read_sb || !read_sb->is_valid()) {
        RejectUpgradeOffer();
        return true;
      }

      read_buffer_ = std::move(read_sb);

      read_buf_.resize(read_buffer_->usable_len());
      AcceptUpgradeOffer();

      // And if we haven't offered ourselves just go ahead and do it now.
      OfferSharedMemUpgrade();
      return true;
    }

    case Message::MessageType::UPGRADE_ACCEPT: {
      if (!write_buffer_ || !write_notifier_ || !write_notifier_->is_valid()) {
        LOG(ERROR) << "Received unexpected UPGRADE_ACCEPT";

        // Clean up anything that may have been set.
        shared_mem_writer_ = false;
        write_buffer_.reset();
        write_notifier_.reset();
        return true;
      }

      shared_mem_writer_ = true;
      return true;
    }

    case Message::MessageType::UPGRADE_REJECT: {
      // We can free our resources.
      shared_mem_writer_ = false;
      write_buffer_.reset();
      write_notifier_.reset();

      return true;
    }
    default:
      break;
  }

  return ChannelPosix::OnControlMessage(message_type, payload, payload_size,
                                        std::move(handles));
}

void ChannelLinux::SharedMemReadReady() {
  CHECK(read_buffer_);
  if (read_buffer_->TryLockForReading()) {
    read_notifier_->Clear();
    bool read_fail = false;
    do {
      uint32_t bytes_read = 0;
      SharedBuffer::Error read_res = read_buffer_->TryReadLocked(
          read_buf_.data(), read_buf_.size(), &bytes_read);
      if (read_res == SharedBuffer::Error::kControlCorruption) {
        // This is an error we cannot recover from.
        OnError(Error::kReceivedMalformedData);
        break;
      }

      if (bytes_read == 0) {
        break;
      }

      // Now dispatch the message, we KNOW it's at least one full message
      // because we checked the message size before putting it into the
      // shared buffer, this mechanism can never write a partial message.
      off_t data_offset = 0;
      while (bytes_read - data_offset > 0) {
        size_t read_size_hint;
        DispatchResult result = TryDispatchMessage(
            base::make_span(
                reinterpret_cast<char*>(read_buf_.data() + data_offset),
                static_cast<size_t>(bytes_read - data_offset)),
            &read_size_hint);

        // We cannot have a message parse failure, we KNOW that we wrote a
        // full message if we get one something has gone horribly wrong.
        if (result != DispatchResult::kOK) {
          LOG(ERROR) << "Recevied a bad message via shared memory";
          read_fail = true;
          OnError(Error::kReceivedMalformedData);
          break;
        }

        // The next message will start after read_size_hint bytes the writer
        // guarantees that we wrote a full message and we've guaranteed that the
        // message was dispatched correctly so we know where the next message
        // starts.
        data_offset += read_size_hint;
      }
    } while (!read_fail);
    read_buffer_->UnlockForReading();
  }
}

void ChannelLinux::OnWriteError(Error error) {
  reject_writes_ = true;
  ChannelPosix::OnWriteError(error);
}

void ChannelLinux::ShutDownOnIOThread() {
  reject_writes_ = true;
  read_notifier_.reset();
  write_notifier_.reset();

  ChannelPosix::ShutDownOnIOThread();
}

void ChannelLinux::StartOnIOThread() {
  ChannelPosix::StartOnIOThread();
}

void ChannelLinux::OfferSharedMemUpgradeInternal() {
  if (reject_writes_) {
    return;
  }

  if (write_buffer_ || write_notifier_) {
    LOG(ERROR) << "Upgrade attempted on an already upgraded channel";
    return;
  }

  const size_t kSize = num_pages_ * base::GetPageSize();
  base::ScopedFD memfd = CreateSealedMemFD(kSize);
  if (!memfd.is_valid()) {
    PLOG(ERROR) << "Unable to create memfd";
    return;
  }

  bool properly_sealed = ValidateFDIsProperlySealedMemFD(memfd);
  if (!properly_sealed) {
    // We will not attempt an offer, something has gone wrong.
    LOG(ERROR) << "FD was not properly sealed we cannot offer upgrade.";
    return;
  }

  std::unique_ptr<SharedBuffer> write_buffer =
      SharedBuffer::Create(memfd, kSize);
  if (!write_buffer || !write_buffer->is_valid()) {
    PLOG(ERROR) << "Unable to map shared memory";
    return;
  }

  write_buffer->Initialize();

  auto notifier_version = UpgradeOfferMessage::kEventFdNotifier;
  std::unique_ptr<EventFDNotifier> write_notifier =
      EventFDNotifier::CreateWriteNotifier();
  if (!write_notifier) {
    PLOG(ERROR) << "Failed to create eventfd write notifier";
    return;
  }

  if (write_notifier->zero_on_wake()) {
    // The notifier was created using EFD_ZERO_ON_WAKE
    notifier_version = UpgradeOfferMessage::kEventFdZeroWakeNotifier;
  }

  std::vector<PlatformHandle> fds;
  fds.emplace_back(std::move(memfd));
  fds.emplace_back(write_notifier->take_dup());

  write_notifier_ = std::move(write_notifier);
  write_buffer_ = std::move(write_buffer);

  UpgradeOfferMessage offer_msg;
  offer_msg.num_pages = num_pages_;
  offer_msg.version = notifier_version;
  MessagePtr msg = Message::CreateMessage(sizeof(UpgradeOfferMessage),
                                          /*num handles=*/fds.size(),
                                          Message::MessageType::UPGRADE_OFFER);
  msg->SetHandles(std::move(fds));
  memcpy(msg->mutable_payload(), &offer_msg, sizeof(offer_msg));

  ChannelPosix::Write(std::move(msg));
}

// static
bool ChannelLinux::KernelSupportsUpgradeRequirements() {
  static bool supported = []() -> bool {
    // See https://crbug.com/1192696 for more context, but some Android vendor
    // kernels pre-3.17 would use higher undefined syscall numbers for private
    // syscalls. To start we'll validate the kernel version is greater than or
    // equal to 3.17 before even bothering to call memfd_create.
    //
    // Additionally, the behavior of eventfd prior to the 4.0 kernel could be
    // racy.
    int os_major_version = 0;
    int os_minor_version = 0;
    int os_bugfix_version = 0;
    KernelVersionNumbers(&os_major_version, &os_minor_version,
                         &os_bugfix_version);
    if (os_major_version < 4) {
      // Due to the potentially races in 3.17/3.18 kernels with eventfd,
      // explicitly require a 4.x+ kernel.
      return false;
    }

#if BUILDFLAG(IS_ANDROID)
    // Finally, if running on Android it must have API version of at
    // least 29 (Q). The reason for this was SELinux seccomp policies prior to
    // that API version wouldn't allow moving a memfd.
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SdkVersion::SDK_VERSION_Q) {
      return false;
    }
#endif

    // Do we have memfd_create support, we check by seeing if we get an
    // -ENOSYS or an -EINVAL. We also support -EPERM because of seccomp
    // rules this is another possible outcome.
    int ret = syscall(__NR_memfd_create, "", ~0);
    PCHECK(ret < 0 && (errno == EINVAL || errno == ENOSYS || errno == EPERM));
    bool memfd_supported = (ret < 0 && errno == EINVAL);
    return memfd_supported && EventFDNotifier::KernelSupported();
  }();
  return supported;
}

// static
bool ChannelLinux::UpgradesEnabled() {
  if (!g_params_set.load())
    return g_use_shared_mem.load();

  return base::FeatureList::IsEnabled(kMojoLinuxChannelSharedMem);
}

// static
void ChannelLinux::SetSharedMemParameters(bool enabled,
                                          uint32_t num_pages,
                                          bool use_zero_on_wake) {
  g_params_set.store(true);
  g_use_shared_mem.store(enabled);
  g_shared_mem_pages.store(num_pages);
  g_use_zero_on_wake.store(use_zero_on_wake);
}

}  // namespace core
}  // namespace mojo
