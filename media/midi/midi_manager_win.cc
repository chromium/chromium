// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/midi_manager_win.h"

// clang-format off
#include <windows.h> // Must be in front of other Windows header files.
// clang-format on

#include <ks.h>
#include <ksmedia.h>
#include <mmreg.h>
#include <mmsystem.h>

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/midi/message_util.h"
#include "media/midi/midi_manager_winrt.h"
#include "media/midi/midi_service.h"
#include "media/midi/midi_service.mojom.h"
#include "media/midi/midi_switches.h"
#include "services/device/public/cpp/usb/usb_ids.h"

namespace midi {

// Forward declaration of PortManager for anonymous functions and internal
// classes to use it.
class MidiManagerWin::PortManager {
 public:
  // Calculates event time from elapsed time that system provides.
  base::TimeTicks CalculateInEventTime(size_t index, uint32_t elapsed_ms) const;

  // Registers HMIDIIN handle to resolve port index.
  void RegisterInHandle(HMIDIIN handle, size_t index);

  // Unregisters HMIDIIN handle.
  void UnregisterInHandle(HMIDIIN handle);

  // Finds HMIDIIN handle and fulfill |out_index| with the port index.
  bool FindInHandle(HMIDIIN hmi, size_t* out_index);

  // Restores used input buffer for the next data receive.
  void RestoreInBuffer(size_t index);

  // Ports accessors.
  std::vector<std::unique_ptr<InPort>>* inputs() { return &input_ports_; }
  std::vector<std::unique_ptr<OutPort>>* outputs() { return &output_ports_; }

  // Handles MIDI input port callbacks that runs on a system provided thread.
  static void CALLBACK HandleMidiInCallback(HMIDIIN hmi,
                                            UINT msg,
                                            DWORD_PTR instance,
                                            DWORD_PTR param1,
                                            DWORD_PTR param2);

  // Handles MIDI output port callbacks that runs on a system provided thread.
  static void CALLBACK HandleMidiOutCallback(HMIDIOUT hmo,
                                             UINT msg,
                                             DWORD_PTR instance,
                                             DWORD_PTR param1,
                                             DWORD_PTR param2);

 private:
  // Holds all MIDI input or output ports connected once.
  std::vector<std::unique_ptr<InPort>> input_ports_;
  std::vector<std::unique_ptr<OutPort>> output_ports_;

  // Map to resolve MIDI input port index from HMIDIIN.
  std::map<HMIDIIN, size_t> hmidiin_to_index_map_;
};

namespace {

// Assumes that nullptr represents an invalid MIDI handle.
constexpr HMIDIIN kInvalidInHandle = nullptr;
constexpr HMIDIOUT kInvalidOutHandle = nullptr;

// Defines SysEx message size limit.
// TODO(crbug.com/40370059): This restriction should be removed once Web MIDI
// defines a standardized way to handle large sysex messages.
// Note for built-in USB-MIDI driver:
// From an observation on Windows 7/8.1 with a USB-MIDI keyboard,
// midiOutLongMsg() will be always blocked. Sending 64 bytes or less data takes
// roughly 300 usecs. Sending 2048 bytes or more data takes roughly
// |message.size() / (75 * 1024)| secs in practice. Here we put 256 KB size
// limit on SysEx message, with hoping that midiOutLongMsg will be blocked at
// most 4 sec or so with a typical USB-MIDI device.
// TODO(toyoshim): Consider to use linked small buffers so that midiOutReset()
// can abort sending unhandled following buffers.
constexpr size_t kSysExSizeLimit = 256 * 1024;

// Defines input buffer size.
constexpr size_t kBufferLength = 32 * 1024;

// Global variables to identify MidiManager instance.
constexpr int64_t kInvalidInstanceId = -1;
int64_t g_active_instance_id = kInvalidInstanceId;
MidiManagerWin* g_manager_instance = nullptr;

// Obtains base::Lock instance pointer to lock instance_id.
base::Lock* GetInstanceIdLock() {
  static base::Lock* lock = new base::Lock;
  return lock;
}

// Issues unique MidiManager instance ID.
int64_t IssueNextInstanceId(std::optional<int64_t> override_id) {
  static int64_t id = kInvalidInstanceId;
  if (override_id) {
    int64_t result = ++id;
    id = *override_id;
    return result;
  }
  if (id == std::numeric_limits<int64_t>::max())
    return kInvalidInstanceId;
  return ++id;
}

// Use single TaskRunner for all tasks running outside the I/O thread.
constexpr int kTaskRunner = 0;

// Obtains base::Lock instance pointer to ensure tasks run safely on TaskRunner.
// Since all tasks on TaskRunner run behind a lock of *GetTaskLock(), we can
// access all members even on the I/O thread if a lock of *GetTaskLock() is
// obtained.
base::Lock* GetTaskLock() {
  static base::Lock* lock = new base::Lock;
  return lock;
}

// Helper function to run a posted task on TaskRunner safely.
void RunTask(int instance_id, base::OnceClosure task) {
  // Obtains task lock to ensure that the instance should not complete
  // Finalize() while running the |task|.
  base::AutoLock task_lock(*GetTaskLock());
  {
    // If destructor finished before the lock avobe, do nothing.
    base::AutoLock lock(*GetInstanceIdLock());
    if (instance_id != g_active_instance_id)
      return;
  }
  std::move(task).Run();
}

// TODO(toyoshim): Use midi::TaskService and deprecate its prototype
// implementation above that is still used in this MidiManagerWin class.

// Obtains base::Lock instance pointer to protect
// |g_midi_in_get_num_devs_thread_id|.
base::Lock* GetMidiInGetNumDevsThreadIdLock() {
  static base::Lock* lock = new base::Lock;
  return lock;
}

// Holds a thread id that calls midiInGetNumDevs() now. We use a platform
// primitive to identify the thread because the following functions can be
// called on a thread that Windows allocates internally, and Chrome or //base
// library does not know.
base::PlatformThreadId g_midi_in_get_num_devs_thread_id;

// Prepares to call midiInGetNumDevs().
void EnterMidiInGetNumDevs() {
  base::AutoLock lock(*GetMidiInGetNumDevsThreadIdLock());
  g_midi_in_get_num_devs_thread_id = base::PlatformThread::CurrentId();
}

// Finalizes to call midiInGetNumDevs().
void LeaveMidiInGetNumDevs() {
  base::AutoLock lock(*GetMidiInGetNumDevsThreadIdLock());
  g_midi_in_get_num_devs_thread_id = base::PlatformThreadId();
}

// Checks if the current thread is running midiInGetNumDevs(), that means
// current code is invoked inside midiInGetNumDevs().
bool IsRunningInsideMidiInGetNumDevs() {
  base::AutoLock lock(*GetMidiInGetNumDevsThreadIdLock());
  return base::PlatformThread::CurrentId() == g_midi_in_get_num_devs_thread_id;
}

// Utility class to handle MIDIHDR struct safely.
class MIDIHDRDeleter {
 public:
  void operator()(LPMIDIHDR header) {
    if (!header)
      return;
    delete[] static_cast<char*>(header->lpData);
    delete header;
  }
};

using ScopedMIDIHDR = std::unique_ptr<MIDIHDR, MIDIHDRDeleter>;

ScopedMIDIHDR CreateMIDIHDR(size_t size) {
  ScopedMIDIHDR hdr(new MIDIHDR);
  ZeroMemory(hdr.get(), sizeof(*hdr));
  hdr->lpData = new char[size];
  hdr->dwBufferLength = static_cast<DWORD>(size);
  return hdr;
}

ScopedMIDIHDR CreateMIDIHDR(const std::vector<uint8_t>& data) {
  ScopedMIDIHDR hdr(CreateMIDIHDR(data.size()));
  base::ranges::copy(data, hdr->lpData);
  return hdr;
}

// Helper functions to close MIDI device handles on TaskRunner asynchronously.
void FinalizeInPort(HMIDIIN handle, ScopedMIDIHDR hdr) {
  // Resets the device. This stops receiving messages, and allows to release
  // registered buffer headers. Otherwise, midiInUnprepareHeader() and
  // midiInClose() will fail with MIDIERR_STILLPLAYING.
  midiInReset(handle);

  if (hdr)
    midiInUnprepareHeader(handle, hdr.get(), sizeof(*hdr));
  midiInClose(handle);
}

void FinalizeOutPort(HMIDIOUT handle) {
  // Resets inflight buffers. This will cancel sending data that system
  // holds and were not sent yet.
  midiOutReset(handle);
  midiOutClose(handle);
}

// Gets manufacturer name in string from identifiers.
std::string GetManufacturerName(uint16_t id, const GUID& guid) {
  if (IS_COMPATIBLE_USBAUDIO_MID(&guid)) {
    const char* name =
        device::UsbIds::GetVendorName(EXTRACT_USBAUDIO_MID(&guid));
    if (name)
      return std::string(name);
  }
  if (id == MM_MICROSOFT)
    return "Microsoft Corporation";

  // TODO(crbug.com/41165639): Support other manufacture IDs.
  return "";
}

// All instances of Port subclasses are always accessed behind a lock of
// *GetTaskLock(). Port and subclasses implementation do not need to
// consider thread safety.
class Port {
 public:
  Port(const std::string& type,
       uint32_t device_id,
       uint16_t manufacturer_id,
       uint16_t product_id,
       uint32_t driver_version,
       const std::string& product_name,
       const GUID& manufacturer_guid)
      : index_(0u),
        type_(type),
        device_id_(device_id),
        manufacturer_id_(manufacturer_id),
        product_id_(product_id),
        driver_version_(driver_version),
        product_name_(product_name) {
    info_.manufacturer =
        GetManufacturerName(manufacturer_id, manufacturer_guid);
    info_.name = product_name_;
    info_.version = base::StringPrintf("%d.%d", HIBYTE(driver_version_),
                                       LOBYTE(driver_version_));
    info_.state = mojom::PortState::DISCONNECTED;
  }

  virtual ~Port() {}

  bool operator==(const Port& other) const {
    // Should not use |device_id| for comparison because it can be changed on
    // each enumeration.
    // Since the GUID will be changed on each enumeration for Microsoft GS
    // Wavetable synth and might be done for others, do not use it for device
    // comparison.
    return manufacturer_id_ == other.manufacturer_id_ &&
           product_id_ == other.product_id_ &&
           driver_version_ == other.driver_version_ &&
           product_name_ == other.product_name_;
  }

  bool IsConnected() const {
    return info_.state != mojom::PortState::DISCONNECTED;
  }

  void set_index(size_t index) {
    index_ = index;
    // TODO(toyoshim): Use hashed ID.
    info_.id = base::StringPrintf("%s-%zd", type_.c_str(), index_);
  }
  size_t index() { return index_; }
  void set_device_id(uint32_t device_id) { device_id_ = device_id; }
  uint32_t device_id() { return device_id_; }
  const mojom::PortInfo& info() { return info_; }

  virtual bool Connect() {
    if (info_.state != mojom::PortState::DISCONNECTED)
      return false;

    info_.state = mojom::PortState::CONNECTED;
    // TODO(toyoshim) Until open() / close() are supported, open each device on
    // connected.
    Open();
    return true;
  }

  virtual bool Disconnect() {
    if (info_.state == mojom::PortState::DISCONNECTED)
      return false;
    info_.state = mojom::PortState::DISCONNECTED;
    return true;
  }

  virtual void Open() { info_.state = mojom::PortState::OPENED; }

 protected:
  size_t index_;
  std::string type_;
  uint32_t device_id_;
  const uint16_t manufacturer_id_;
  const uint16_t product_id_;
  const uint32_t driver_version_;
  const std::string product_name_;
  mojom::PortInfo info_;
};  // class Port

}  // namespace

class MidiManagerWin::InPort final : public Port {
 public:
  InPort(MidiManagerWin* manager,
         int instance_id,
         UINT device_id,
         const MIDIINCAPS2W& caps)
      : Port("input",
             device_id,
             caps.wMid,
             caps.wPid,
             caps.vDriverVersion,
             base::WideToUTF8(std::wstring(caps.szPname, wcslen(caps.szPname))),
             caps.ManufacturerGuid),
        manager_(manager),
        in_handle_(kInvalidInHandle),
        instance_id_(instance_id) {}

  static std::vector<std::unique_ptr<InPort>> EnumerateActivePorts(
      MidiManagerWin* manager,
      int instance_id) {
    std::vector<std::unique_ptr<InPort>> ports;

    // Allow callback invocations indie midiInGetNumDevs().
    EnterMidiInGetNumDevs();
    const UINT num_devices = midiInGetNumDevs();
    LeaveMidiInGetNumDevs();

    for (UINT device_id = 0; device_id < num_devices; ++device_id) {
      MIDIINCAPS2W caps;
      MMRESULT result = midiInGetDevCaps(
          device_id, reinterpret_cast<LPMIDIINCAPSW>(&caps), sizeof(caps));
      if (result != MMSYSERR_NOERROR) {
        LOG(ERROR) << "midiInGetDevCaps fails on device " << device_id;
        continue;
      }
      ports.push_back(
          std::make_unique<InPort>(manager, instance_id, device_id, caps));
    }
    return ports;
  }

  void Finalize(scoped_refptr<base::SingleThreadTaskRunner> runner) {
    if (in_handle_ != kInvalidInHandle) {
      runner->PostTask(FROM_HERE, base::BindOnce(&FinalizeInPort, in_handle_,
                                                 std::move(hdr_)));
      manager_->port_manager()->UnregisterInHandle(in_handle_);
      in_handle_ = kInvalidInHandle;
    }
  }

  base::TimeTicks CalculateInEventTime(uint32_t elapsed_ms) const {
    return start_time_ + base::Milliseconds(elapsed_ms);
  }

  void RestoreBuffer() {
    if (in_handle_ == kInvalidInHandle || !hdr_)
      return;
    midiInAddBuffer(in_handle_, hdr_.get(), sizeof(*hdr_));
  }

  void NotifyPortStateSet(MidiManagerWin* manager) {
    manager->PostReplyTask(base::BindOnce(
        &MidiManagerWin::SetInputPortState, base::Unretained(manager),
        static_cast<uint32_t>(index_), info_.state));
  }

  void NotifyPortAdded(MidiManagerWin* manager) {
    manager->PostReplyTask(base::BindOnce(&MidiManagerWin::AddInputPort,
                                          base::Unretained(manager), info_));
  }

  // Port overrides:
  bool Disconnect() override {
    if (in_handle_ != kInvalidInHandle) {
      // Following API call may fail because device was already disconnected.
      // But just in case.
      midiInClose(in_handle_);
      manager_->port_manager()->UnregisterInHandle(in_handle_);
      in_handle_ = kInvalidInHandle;
    }
    return Port::Disconnect();
  }

  void Open() override {
    MMRESULT result = midiInOpen(
        &in_handle_, device_id_,
        reinterpret_cast<DWORD_PTR>(&PortManager::HandleMidiInCallback),
        instance_id_, CALLBACK_FUNCTION);
    if (result == MMSYSERR_NOERROR) {
      hdr_ = CreateMIDIHDR(kBufferLength);
      result = midiInPrepareHeader(in_handle_, hdr_.get(), sizeof(*hdr_));
    }
    if (result != MMSYSERR_NOERROR)
      in_handle_ = kInvalidInHandle;
    if (result == MMSYSERR_NOERROR)
      result = midiInAddBuffer(in_handle_, hdr_.get(), sizeof(*hdr_));
    if (result == MMSYSERR_NOERROR)
      result = midiInStart(in_handle_);
    if (result == MMSYSERR_NOERROR) {
      start_time_ = base::TimeTicks::Now();
      manager_->port_manager()->RegisterInHandle(in_handle_, index_);
      Port::Open();
    } else {
      if (in_handle_ != kInvalidInHandle) {
        midiInUnprepareHeader(in_handle_, hdr_.get(), sizeof(*hdr_));
        hdr_.reset();
        midiInClose(in_handle_);
        in_handle_ = kInvalidInHandle;
      }
      Disconnect();
    }
  }

 private:
  raw_ptr<MidiManagerWin> manager_;
  HMIDIIN in_handle_;
  ScopedMIDIHDR hdr_;
  base::TimeTicks start_time_;
  const int instance_id_;
};

class MidiManagerWin::OutPort final : public Port {
 public:
  OutPort(UINT device_id, const MIDIOUTCAPS2W& caps)
      : Port("output",
             device_id,
             caps.wMid,
             caps.wPid,
             caps.vDriverVersion,
             base::WideToUTF8(std::wstring(caps.szPname, wcslen(caps.szPname))),
             caps.ManufacturerGuid),
        software_(caps.wTechnology == MOD_SWSYNTH),
        out_handle_(kInvalidOutHandle) {}

  static std::vector<std::unique_ptr<OutPort>> EnumerateActivePorts() {
    std::vector<std::unique_ptr<OutPort>> ports;
    const UINT num_devices = midiOutGetNumDevs();
    for (UINT device_id = 0; device_id < num_devices; ++device_id) {
      MIDIOUTCAPS2W caps;
      MMRESULT result = midiOutGetDevCaps(
          device_id, reinterpret_cast<LPMIDIOUTCAPSW>(&caps), sizeof(caps));
      if (result != MMSYSERR_NOERROR) {
        LOG(ERROR) << "midiOutGetDevCaps fails on device " << device_id;
        continue;
      }
      ports.push_back(std::make_unique<OutPort>(device_id, caps));
    }
    return ports;
  }

  void Finalize(scoped_refptr<base::SingleThreadTaskRunner> runner) {
    if (out_handle_ != kInvalidOutHandle) {
      runner->PostTask(FROM_HERE,
                       base::BindOnce(&FinalizeOutPort, out_handle_));
      out_handle_ = kInvalidOutHandle;
    }
  }

  void NotifyPortStateSet(MidiManagerWin* manager) {
    manager->PostReplyTask(base::BindOnce(
        &MidiManagerWin::SetOutputPortState, base::Unretained(manager),
        static_cast<uint32_t>(index_), info_.state));
  }

  void NotifyPortAdded(MidiManagerWin* manager) {
    manager->PostReplyTask(base::BindOnce(&MidiManagerWin::AddOutputPort,
                                          base::Unretained(manager), info_));
  }

  void Send(const std::vector<uint8_t>& data) {
    if (out_handle_ == kInvalidOutHandle)
      return;

    if (data.size() <= 3) {
      uint32_t message = 0;
      for (size_t i = 0; i < data.size(); ++i)
        message |= (static_cast<uint32_t>(data[i]) << (i * 8));
      midiOutShortMsg(out_handle_, message);
    } else {
      if (data.size() > kSysExSizeLimit) {
        LOG(ERROR) << "Ignoring SysEx message due to the size limit"
                   << ", size = " << data.size();
        // TODO(toyoshim): Consider to report metrics here.
        return;
      }
      ScopedMIDIHDR hdr(CreateMIDIHDR(data));
      MMRESULT result =
          midiOutPrepareHeader(out_handle_, hdr.get(), sizeof(*hdr));
      if (result != MMSYSERR_NOERROR)
        return;
      result = midiOutLongMsg(out_handle_, hdr.get(), sizeof(*hdr));
      if (result != MMSYSERR_NOERROR) {
        midiOutUnprepareHeader(out_handle_, hdr.get(), sizeof(*hdr));
      } else {
        // MIDIHDR will be released on MOM_DONE.
        std::ignore = hdr.release();
      }
    }
  }

  // Port overrides:
  bool Connect() override {
    // Until |software| option is supported, disable Microsoft GS Wavetable
    // Synth that has a known security issue.
    if (software_ && manufacturer_id_ == MM_MICROSOFT &&
        (product_id_ == MM_MSFT_WDMAUDIO_MIDIOUT ||
         product_id_ == MM_MSFT_GENERIC_MIDISYNTH)) {
      return false;
    }
    return Port::Connect();
  }

  bool Disconnect() override {
    if (out_handle_ != kInvalidOutHandle) {
      // Following API call may fail because device was already disconnected.
      // But just in case.
      midiOutClose(out_handle_);
      out_handle_ = kInvalidOutHandle;
    }
    return Port::Disconnect();
  }

  void Open() override {
    MMRESULT result = midiOutOpen(
        &out_handle_, device_id_,
        reinterpret_cast<DWORD_PTR>(&PortManager::HandleMidiOutCallback), 0,
        CALLBACK_FUNCTION);
    if (result == MMSYSERR_NOERROR) {
      Port::Open();
    } else {
      out_handle_ = kInvalidOutHandle;
      Disconnect();
    }
  }

  const bool software_;
  HMIDIOUT out_handle_;
};

base::TimeTicks MidiManagerWin::PortManager::CalculateInEventTime(
    size_t index,
    uint32_t elapsed_ms) const {
  GetTaskLock()->AssertAcquired();
  CHECK_GT(input_ports_.size(), index);
  return input_ports_[index]->CalculateInEventTime(elapsed_ms);
}

void MidiManagerWin::PortManager::RegisterInHandle(HMIDIIN handle,
                                                   size_t index) {
  GetTaskLock()->AssertAcquired();
  hmidiin_to_index_map_[handle] = index;
}

void MidiManagerWin::PortManager::UnregisterInHandle(HMIDIIN handle) {
  GetTaskLock()->AssertAcquired();
  hmidiin_to_index_map_.erase(handle);
}

bool MidiManagerWin::PortManager::FindInHandle(HMIDIIN hmi, size_t* out_index) {
  GetTaskLock()->AssertAcquired();
  auto found = hmidiin_to_index_map_.find(hmi);
  if (found == hmidiin_to_index_map_.end())
    return false;
  *out_index = found->second;
  return true;
}

void MidiManagerWin::PortManager::RestoreInBuffer(size_t index) {
  GetTaskLock()->AssertAcquired();
  CHECK_GT(input_ports_.size(), index);
  input_ports_[index]->RestoreBuffer();
}

void CALLBACK
MidiManagerWin::PortManager::HandleMidiInCallback(HMIDIIN hmi,
                                                  UINT msg,
                                                  DWORD_PTR instance,
                                                  DWORD_PTR param1,
                                                  DWORD_PTR param2) {
  if (msg != MIM_DATA && msg != MIM_LONGDATA)
    return;
  int instance_id = static_cast<int>(instance);
  MidiManagerWin* manager = nullptr;

  // Use |g_task_lock| so to ensure the instance can keep alive while running,
  // and to access member variables that are used on TaskRunner.
  // Exceptionally, we do not take the lock when this callback is invoked inside
  // midiInGetNumDevs() on the caller thread because the lock is already
  // obtained by the current caller thread.
  std::optional<base::AutoLock> task_lock;
  if (IsRunningInsideMidiInGetNumDevs())
    GetTaskLock()->AssertAcquired();
  else
    task_lock.emplace(*GetTaskLock());
  {
    base::AutoLock lock(*GetInstanceIdLock());
    if (instance_id != g_active_instance_id)
      return;
    manager = g_manager_instance;
  }

  size_t index;
  if (!manager->port_manager()->FindInHandle(hmi, &index))
    return;

  DCHECK(msg == MIM_DATA || msg == MIM_LONGDATA);
  if (msg == MIM_DATA) {
    const uint8_t status_byte = static_cast<uint8_t>(param1 & 0xff);
    const uint8_t first_data_byte = static_cast<uint8_t>((param1 >> 8) & 0xff);
    const uint8_t second_data_byte =
        static_cast<uint8_t>((param1 >> 16) & 0xff);
    const uint8_t kData[] = {status_byte, first_data_byte, second_data_byte};
    const size_t len = GetMessageLength(status_byte);
    DCHECK_LE(len, std::size(kData));
    std::vector<uint8_t> data;
    data.assign(kData, kData + len);
    manager->PostReplyTask(base::BindOnce(
        &MidiManagerWin::ReceiveMidiData, base::Unretained(manager),
        static_cast<uint32_t>(index), data,
        manager->port_manager()->CalculateInEventTime(index, param2)));
  } else {
    DCHECK_EQ(static_cast<UINT>(MIM_LONGDATA), msg);
    LPMIDIHDR hdr = reinterpret_cast<LPMIDIHDR>(param1);
    if (hdr->dwBytesRecorded > 0) {
      const uint8_t* src = reinterpret_cast<const uint8_t*>(hdr->lpData);
      std::vector<uint8_t> data;
      data.assign(src, src + hdr->dwBytesRecorded);
      manager->PostReplyTask(base::BindOnce(
          &MidiManagerWin::ReceiveMidiData, base::Unretained(manager),
          static_cast<uint32_t>(index), data,
          manager->port_manager()->CalculateInEventTime(index, param2)));
    }
    manager->port_manager()->RestoreInBuffer(index);
  }
}

void CALLBACK
MidiManagerWin::PortManager::HandleMidiOutCallback(HMIDIOUT hmo,
                                                   UINT msg,
                                                   DWORD_PTR instance,
                                                   DWORD_PTR param1,
                                                   DWORD_PTR param2) {
  if (msg == MOM_DONE) {
    ScopedMIDIHDR hdr(reinterpret_cast<LPMIDIHDR>(param1));
    if (!hdr)
      return;
    // TODO(toyoshim): Call midiOutUnprepareHeader outside the callback.
    // Since this callback may be invoked after the manager is destructed,
    // and can not send a task to the TaskRunner in such case, we need to
    // consider to track MIDIHDR per port, and clean it in port finalization
    // steps, too.
    midiOutUnprepareHeader(hmo, hdr.get(), sizeof(*hdr));
  }
}

// static
void MidiManagerWin::OverflowInstanceIdForTesting() {
  IssueNextInstanceId(std::numeric_limits<int64_t>::max());
}

MidiManagerWin::MidiManagerWin(MidiService* service)
    : MidiManager(service),
      instance_id_(IssueNextInstanceId(std::nullopt)),
      port_manager_(std::make_unique<PortManager>()) {
  base::AutoLock lock(*GetInstanceIdLock());
  CHECK_EQ(kInvalidInstanceId, g_active_instance_id);

  // Obtains the task runner for the current thread that hosts this instance.
  thread_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

MidiManagerWin::~MidiManagerWin() {
  // Initialization failed. Exit without running actual finalization that should
  // not be needed.
  if (instance_id_ == kInvalidInstanceId)
    return;

  // Unregisters on the I/O thread. OnDevicesChanged() won't be called any more.
  CHECK(thread_runner_->BelongsToCurrentThread());
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);

  // Posts tasks that finalize each device port without MidiManager instance
  // on TaskRunner. If another MidiManager instance is created, its
  // initialization runs on the same task runner after all tasks posted here
  // finish.
  for (const auto& port : *port_manager_->inputs())
    port->Finalize(service()->GetTaskRunner(kTaskRunner));
  for (const auto& port : *port_manager_->outputs())
    port->Finalize(service()->GetTaskRunner(kTaskRunner));

  // Invalidate instance bound tasks.
  {
    base::AutoLock lock(*GetInstanceIdLock());
    CHECK_EQ(instance_id_, g_active_instance_id);
    g_active_instance_id = kInvalidInstanceId;
    CHECK_EQ(this, g_manager_instance);
    g_manager_instance = nullptr;
  }

  // Ensures that no bound task runs on TaskRunner so to destruct the instance
  // safely.
  // Tasks that did not started yet will do nothing after invalidate the
  // instance ID above.
  // Behind the lock below, we can safely access all members for finalization
  // even on the I/O thread.
  base::AutoLock lock(*GetTaskLock());
}

void MidiManagerWin::StartInitialization() {
  {
    base::AutoLock lock(*GetInstanceIdLock());
    if (instance_id_ == kInvalidInstanceId)
      return CompleteInitialization(mojom::Result::INITIALIZATION_ERROR);

    CHECK_EQ(kInvalidInstanceId, g_active_instance_id);
    g_active_instance_id = instance_id_;
    CHECK_EQ(nullptr, g_manager_instance);
    g_manager_instance = this;
  }
  // Registers on the I/O thread to be notified on the I/O thread.
  CHECK(thread_runner_->BelongsToCurrentThread());
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  // Starts asynchronous initialization on TaskRunner.
  PostTask(base::BindOnce(&MidiManagerWin::InitializeOnTaskRunner,
                          base::Unretained(this)));
}

void MidiManagerWin::DispatchSendMidiData(MidiManagerClient* client,
                                          uint32_t port_index,
                                          const std::vector<uint8_t>& data,
                                          base::TimeTicks timestamp) {
  PostDelayedTask(
      base::BindOnce(&MidiManagerWin::SendOnTaskRunner, base::Unretained(this),
                     client, port_index, data),
      MidiService::TimestampToTimeDeltaDelay(timestamp));
}

void MidiManagerWin::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  // Notified on the I/O thread.
  CHECK(thread_runner_->BelongsToCurrentThread());

  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO:
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      // Add case of other unrelated device types here.
      return;
    case base::SystemMonitor::DEVTYPE_UNKNOWN: {
      PostTask(base::BindOnce(&MidiManagerWin::UpdateDeviceListOnTaskRunner,
                              base::Unretained(this)));
      break;
    }
  }
}

void MidiManagerWin::ReceiveMidiData(uint32_t index,
                                     const std::vector<uint8_t>& data,
                                     base::TimeTicks time) {
  MidiManager::ReceiveMidiData(index, data.data(), data.size(), time);
}

void MidiManagerWin::PostTask(base::OnceClosure task) {
  service()
      ->GetTaskRunner(kTaskRunner)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&RunTask, instance_id_, std::move(task)));
}

void MidiManagerWin::PostDelayedTask(base::OnceClosure task,
                                     base::TimeDelta delay) {
  service()
      ->GetTaskRunner(kTaskRunner)
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(&RunTask, instance_id_, std::move(task)),
                        delay);
}

void MidiManagerWin::PostReplyTask(base::OnceClosure task) {
  thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RunTask, instance_id_, std::move(task)));
}

void MidiManagerWin::InitializeOnTaskRunner() {
  UpdateDeviceListOnTaskRunner();
  PostReplyTask(base::BindOnce(&MidiManagerWin::CompleteInitialization,
                               base::Unretained(this), mojom::Result::OK));
}

void MidiManagerWin::UpdateDeviceListOnTaskRunner() {
  std::vector<std::unique_ptr<InPort>> active_input_ports =
      InPort::EnumerateActivePorts(this, instance_id_);
  ReflectActiveDeviceList(this, port_manager_->inputs(), &active_input_ports);

  std::vector<std::unique_ptr<OutPort>> active_output_ports =
      OutPort::EnumerateActivePorts();
  ReflectActiveDeviceList(this, port_manager_->outputs(), &active_output_ports);

  // TODO(toyoshim): This method may run before internal MIDI device lists that
  // Windows manages were updated. This may be because MIDI driver may be loaded
  // after the raw device list was updated. To avoid this problem, we may want
  // to retry device check later if no changes are detected here.
}

template <typename T>
void MidiManagerWin::ReflectActiveDeviceList(
    MidiManagerWin* manager,
    std::vector<std::unique_ptr<T>>* known_ports,
    std::vector<std::unique_ptr<T>>* active_ports) {
  // Update existing port states.
  for (const auto& port : *known_ports) {
    const auto& it = base::ranges::find(
        *active_ports, *port,
        [](const auto& candidate) -> T& { return *candidate; });
    if (it == active_ports->end()) {
      if (port->Disconnect())
        port->NotifyPortStateSet(this);
    } else {
      port->set_device_id((*it)->device_id());
      if (port->Connect())
        port->NotifyPortStateSet(this);
    }
  }

  // Find new ports from active ports and append them to known ports.
  for (auto& port : *active_ports) {
    if (!base::Contains(*known_ports, *port, [](const auto& candidate) -> T& {
          return *candidate;
        })) {
      size_t index = known_ports->size();
      port->set_index(index);
      known_ports->push_back(std::move(port));
      (*known_ports)[index]->Connect();
      (*known_ports)[index]->NotifyPortAdded(this);
    }
  }
}

void MidiManagerWin::SendOnTaskRunner(MidiManagerClient* client,
                                      uint32_t port_index,
                                      const std::vector<uint8_t>& data) {
  CHECK_GT(port_manager_->outputs()->size(), port_index);
  (*port_manager_->outputs())[port_index]->Send(data);
  // |client| will be checked inside MidiManager::AccumulateMidiBytesSent.
  PostReplyTask(base::BindOnce(&MidiManagerWin::AccumulateMidiBytesSent,
                               base::Unretained(this), client, data.size()));
}

MidiManager* MidiManager::Create(MidiService* service) {
  if (base::FeatureList::IsEnabled(features::kMidiManagerWinrt)) {
    return new MidiManagerWinrt(service);
  }
  return new MidiManagerWin(service);
}

}  // namespace midi
