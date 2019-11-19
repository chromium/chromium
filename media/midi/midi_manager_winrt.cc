// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_winrt.h"

#pragma warning(disable : 4467)

#define INITGUID

#include <windows.h>

#include <cfgmgr32.h>
#include <comdef.h>
#include <devpkey.h>
#include <initguid.h>
#include <objbase.h>
#include <robuffer.h>
#include <windows.devices.enumeration.h>
#include <windows.devices.midi.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <iomanip>
#include <unordered_map>
#include <unordered_set>

#include "base/bind.h"
#include "base/scoped_generic.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "base/win/winrt_storage_util.h"
#include "media/midi/midi_service.h"
#include "media/midi/task_service.h"

namespace midi {
namespace {

namespace WRL = Microsoft::WRL;
namespace Win = ABI::Windows;

using base::win::GetActivationFactory;
using base::win::ScopedHString;
using mojom::PortState;
using mojom::Result;
using Win::Devices::Enumeration::DeviceInformationUpdate;
using Win::Devices::Enumeration::DeviceWatcher;
using Win::Devices::Enumeration::IDeviceInformation;
using Win::Devices::Enumeration::IDeviceInformationUpdate;
using Win::Devices::Enumeration::IDeviceWatcher;
using Win::Foundation::IAsyncOperation;
using Win::Foundation::ITypedEventHandler;

enum {
  kDefaultTaskRunner = TaskService::kDefaultRunnerId,
  kComTaskRunner
};

// Helpers for printing HRESULTs.
struct PrintHr {
  PrintHr(HRESULT hr) : hr(hr) {}
  HRESULT hr;
};

std::ostream& operator<<(std::ostream& os, const PrintHr& phr) {
  std::ios_base::fmtflags ff = os.flags();
  os << _com_error(phr.hr).ErrorMessage() << " (0x" << std::hex
     << std::uppercase << std::setfill('0') << std::setw(8) << phr.hr << ")";
  os.flags(ff);
  return os;
}

template <typename T>
std::string GetIdString(T* obj) {
  HSTRING result;
  HRESULT hr = obj->get_Id(&result);
  if (FAILED(hr)) {
    VLOG(1) << "get_Id failed: " << PrintHr(hr);
    return std::string();
  }
  return ScopedHString(result).GetAsUTF8();
}

template <typename T>
std::string GetDeviceIdString(T* obj) {
  HSTRING result;
  HRESULT hr = obj->get_DeviceId(&result);
  if (FAILED(hr)) {
    VLOG(1) << "get_DeviceId failed: " << PrintHr(hr);
    return std::string();
  }
  return ScopedHString(result).GetAsUTF8();
}

std::string GetNameString(IDeviceInformation* info) {
  HSTRING result;
  HRESULT hr = info->get_Name(&result);
  if (FAILED(hr)) {
    VLOG(1) << "get_Name failed: " << PrintHr(hr);
    return std::string();
  }
  return ScopedHString(result).GetAsUTF8();
}

// Checks if given DeviceInformation represent a Microsoft GS Wavetable Synth
// instance.
bool IsMicrosoftSynthesizer(IDeviceInformation* info) {
  WRL::ComPtr<Win::Devices::Midi::IMidiSynthesizerStatics>
      midi_synthesizer_statics;
  HRESULT hr =
      GetActivationFactory<Win::Devices::Midi::IMidiSynthesizerStatics,
                           RuntimeClass_Windows_Devices_Midi_MidiSynthesizer>(
          &midi_synthesizer_statics);
  if (FAILED(hr)) {
    VLOG(1) << "IMidiSynthesizerStatics factory failed: " << PrintHr(hr);
    return false;
  }
  boolean result = FALSE;
  hr = midi_synthesizer_statics->IsSynthesizer(info, &result);
  VLOG_IF(1, FAILED(hr)) << "IsSynthesizer failed: " << PrintHr(hr);
  return result != FALSE;
}

void GetDevPropString(DEVINST handle,
                      const DEVPROPKEY* devprop_key,
                      std::string* out) {
  DEVPROPTYPE devprop_type;
  unsigned long buffer_size = 0;

  // Retrieve |buffer_size| and allocate buffer later for receiving data.
  CONFIGRET cr = CM_Get_DevNode_Property(handle, devprop_key, &devprop_type,
                                         nullptr, &buffer_size, 0);
  if (cr != CR_BUFFER_SMALL) {
    // Here we print error codes in hex instead of using PrintHr() with
    // HRESULT_FROM_WIN32() and CM_MapCrToWin32Err(), since only a minor set of
    // CONFIGRET values are mapped to Win32 errors. Same for following VLOG()s.
    VLOG(1) << "CM_Get_DevNode_Property failed: CONFIGRET 0x" << std::hex << cr;
    return;
  }
  if (devprop_type != DEVPROP_TYPE_STRING) {
    VLOG(1) << "CM_Get_DevNode_Property returns wrong data type, "
            << "expected DEVPROP_TYPE_STRING";
    return;
  }

  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);

  // Receive property data.
  cr = CM_Get_DevNode_Property(handle, devprop_key, &devprop_type, buffer.get(),
                               &buffer_size, 0);
  if (cr != CR_SUCCESS)
    VLOG(1) << "CM_Get_DevNode_Property failed: CONFIGRET 0x" << std::hex << cr;
  else
    *out = base::WideToUTF8(reinterpret_cast<base::char16*>(buffer.get()));
}

// Retrieves manufacturer (provider) and version information of underlying
// device driver through PnP Configuration Manager, given device (interface) ID
// provided by WinRT. |out_manufacturer| and |out_driver_version| won't be
// modified if retrieval fails.
//
// Device instance ID is extracted from device (interface) ID provided by WinRT
// APIs, for example from the following interface ID:
// \\?\SWD#MMDEVAPI#MIDII_60F39FCA.P_0002#{504be32c-ccf6-4d2c-b73f-6f8b3747e22b}
// we extract the device instance ID: SWD\MMDEVAPI\MIDII_60F39FCA.P_0002
//
// However the extracted device instance ID represent a "software device"
// provided by Microsoft, which is an interface on top of the hardware for each
// input/output port. Therefore we further locate its parent device, which is
// the actual hardware device, for driver information.
void GetDriverInfoFromDeviceId(const std::string& dev_id,
                               std::string* out_manufacturer,
                               std::string* out_driver_version) {
  base::string16 dev_instance_id =
      base::UTF8ToWide(dev_id.substr(4, dev_id.size() - 43));
  base::ReplaceChars(dev_instance_id, L"#", L"\\", &dev_instance_id);

  DEVINST dev_instance_handle;
  CONFIGRET cr = CM_Locate_DevNode(&dev_instance_handle, &dev_instance_id[0],
                                   CM_LOCATE_DEVNODE_NORMAL);
  if (cr != CR_SUCCESS) {
    VLOG(1) << "CM_Locate_DevNode failed: CONFIGRET 0x" << std::hex << cr;
    return;
  }

  DEVINST parent_handle;
  cr = CM_Get_Parent(&parent_handle, dev_instance_handle, 0);
  if (cr != CR_SUCCESS) {
    VLOG(1) << "CM_Get_Parent failed: CONFIGRET 0x" << std::hex << cr;
    return;
  }

  GetDevPropString(parent_handle, &DEVPKEY_Device_DriverProvider,
                   out_manufacturer);
  GetDevPropString(parent_handle, &DEVPKEY_Device_DriverVersion,
                   out_driver_version);
}

// Tokens with value = 0 are considered invalid (as in <wrl/event.h>).
const int64_t kInvalidTokenValue = 0;

template <typename InterfaceType>
struct MidiPort {
  MidiPort() = default;

  uint32_t index;
  WRL::ComPtr<InterfaceType> handle;
  EventRegistrationToken token_MessageReceived;

 private:
  DISALLOW_COPY_AND_ASSIGN(MidiPort);
};

}  // namespace

template <typename InterfaceType,
          typename RuntimeType,
          typename StaticsInterfaceType,
          base::char16 const* runtime_class_id>
class MidiManagerWinrt::MidiPortManager {
 public:
  // MidiPortManager instances should be constructed on the kComTaskRunner.
  MidiPortManager(MidiManagerWinrt* midi_manager)
      : midi_service_(midi_manager->service()), midi_manager_(midi_manager) {}

  virtual ~MidiPortManager() {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));
  }

  bool StartWatcher() {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));

    HRESULT hr = GetActivationFactory<StaticsInterfaceType, runtime_class_id>(
        &midi_port_statics_);
    if (FAILED(hr)) {
      VLOG(1) << "StaticsInterfaceType factory failed: " << PrintHr(hr);
      return false;
    }

    HSTRING device_selector = nullptr;
    hr = midi_port_statics_->GetDeviceSelector(&device_selector);
    if (FAILED(hr)) {
      VLOG(1) << "GetDeviceSelector failed: " << PrintHr(hr);
      return false;
    }

    WRL::ComPtr<Win::Devices::Enumeration::IDeviceInformationStatics>
        dev_info_statics;
    hr = GetActivationFactory<
        Win::Devices::Enumeration::IDeviceInformationStatics,
        RuntimeClass_Windows_Devices_Enumeration_DeviceInformation>(
        &dev_info_statics);
    if (FAILED(hr)) {
      VLOG(1) << "IDeviceInformationStatics failed: " << PrintHr(hr);
      return false;
    }

    hr = dev_info_statics->CreateWatcherAqsFilter(device_selector,
                                                  watcher_.GetAddressOf());
    if (FAILED(hr)) {
      VLOG(1) << "CreateWatcherAqsFilter failed: " << PrintHr(hr);
      return false;
    }

    // Register callbacks to WinRT that post state-modifying tasks back to
    // kComTaskRunner. All posted tasks run only during the MidiPortManager
    // instance is alive. This is ensured by MidiManagerWinrt by calling
    // UnbindInstance() before destructing any MidiPortManager instance. Thus
    // we can handle raw pointers safely in the following blocks.
    MidiPortManager* port_manager = this;
    TaskService* task_service = midi_service_->task_service();

    hr = watcher_->add_Added(
        WRL::Callback<ITypedEventHandler<
            DeviceWatcher*, Win::Devices::Enumeration::DeviceInformation*>>(
            [port_manager, task_service](IDeviceWatcher* watcher,
                                         IDeviceInformation* info) {
              if (!info) {
                VLOG(1) << "DeviceWatcher.Added callback provides null "
                           "pointer, ignoring";
                return S_OK;
              }

              // Disable Microsoft GS Wavetable Synth due to security reasons.
              // http://crbug.com/499279
              if (IsMicrosoftSynthesizer(info))
                return S_OK;

              std::string dev_id = GetIdString(info),
                          dev_name = GetNameString(info);

              task_service->PostBoundTask(
                  kComTaskRunner, base::BindOnce(&MidiPortManager::OnAdded,
                                                 base::Unretained(port_manager),
                                                 dev_id, dev_name));

              return S_OK;
            })
            .Get(),
        &token_Added_);
    if (FAILED(hr)) {
      VLOG(1) << "add_Added failed: " << PrintHr(hr);
      return false;
    }

    hr = watcher_->add_EnumerationCompleted(
        WRL::Callback<ITypedEventHandler<DeviceWatcher*, IInspectable*>>(
            [port_manager, task_service](IDeviceWatcher* watcher,
                                         IInspectable* insp) {
              task_service->PostBoundTask(
                  kComTaskRunner,
                  base::BindOnce(&MidiPortManager::OnEnumerationCompleted,
                                 base::Unretained(port_manager)));

              return S_OK;
            })
            .Get(),
        &token_EnumerationCompleted_);
    if (FAILED(hr)) {
      VLOG(1) << "add_EnumerationCompleted failed: " << PrintHr(hr);
      return false;
    }

    hr = watcher_->add_Removed(
        WRL::Callback<
            ITypedEventHandler<DeviceWatcher*, DeviceInformationUpdate*>>(
            [port_manager, task_service](IDeviceWatcher* watcher,
                                         IDeviceInformationUpdate* update) {
              if (!update) {
                VLOG(1) << "DeviceWatcher.Removed callback provides null "
                           "pointer, ignoring";
                return S_OK;
              }

              std::string dev_id = GetIdString(update);

              task_service->PostBoundTask(
                  kComTaskRunner,
                  base::BindOnce(&MidiPortManager::OnRemoved,
                                 base::Unretained(port_manager), dev_id));

              return S_OK;
            })
            .Get(),
        &token_Removed_);
    if (FAILED(hr)) {
      VLOG(1) << "add_Removed failed: " << PrintHr(hr);
      return false;
    }

    hr = watcher_->add_Stopped(
        WRL::Callback<ITypedEventHandler<DeviceWatcher*, IInspectable*>>(
            [](IDeviceWatcher* watcher, IInspectable* insp) {
              // Placeholder, does nothing for now.
              return S_OK;
            })
            .Get(),
        &token_Stopped_);
    if (FAILED(hr)) {
      VLOG(1) << "add_Stopped failed: " << PrintHr(hr);
      return false;
    }

    hr = watcher_->add_Updated(
        WRL::Callback<
            ITypedEventHandler<DeviceWatcher*, DeviceInformationUpdate*>>(
            [](IDeviceWatcher* watcher, IDeviceInformationUpdate* update) {
              // TODO(shaochuan): Check for fields to be updated here.
              return S_OK;
            })
            .Get(),
        &token_Updated_);
    if (FAILED(hr)) {
      VLOG(1) << "add_Updated failed: " << PrintHr(hr);
      return false;
    }

    hr = watcher_->Start();
    if (FAILED(hr)) {
      VLOG(1) << "Start failed: " << PrintHr(hr);
      return false;
    }

    is_initialized_ = true;
    return true;
  }

  void StopWatcher() {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));

    HRESULT hr;

    for (const auto& entry : ports_)
      RemovePortEventHandlers(entry.second.get());

    if (token_Added_.value != kInvalidTokenValue) {
      hr = watcher_->remove_Added(token_Added_);
      VLOG_IF(1, FAILED(hr)) << "remove_Added failed: " << PrintHr(hr);
      token_Added_.value = kInvalidTokenValue;
    }
    if (token_EnumerationCompleted_.value != kInvalidTokenValue) {
      hr = watcher_->remove_EnumerationCompleted(token_EnumerationCompleted_);
      VLOG_IF(1, FAILED(hr)) << "remove_EnumerationCompleted failed: "
                             << PrintHr(hr);
      token_EnumerationCompleted_.value = kInvalidTokenValue;
    }
    if (token_Removed_.value != kInvalidTokenValue) {
      hr = watcher_->remove_Removed(token_Removed_);
      VLOG_IF(1, FAILED(hr)) << "remove_Removed failed: " << PrintHr(hr);
      token_Removed_.value = kInvalidTokenValue;
    }
    if (token_Stopped_.value != kInvalidTokenValue) {
      hr = watcher_->remove_Stopped(token_Stopped_);
      VLOG_IF(1, FAILED(hr)) << "remove_Stopped failed: " << PrintHr(hr);
      token_Stopped_.value = kInvalidTokenValue;
    }
    if (token_Updated_.value != kInvalidTokenValue) {
      hr = watcher_->remove_Updated(token_Updated_);
      VLOG_IF(1, FAILED(hr)) << "remove_Updated failed: " << PrintHr(hr);
      token_Updated_.value = kInvalidTokenValue;
    }

    if (is_initialized_) {
      hr = watcher_->Stop();
      VLOG_IF(1, FAILED(hr)) << "Stop failed: " << PrintHr(hr);
      is_initialized_ = false;
    }
  }

  MidiPort<InterfaceType>* GetPortByDeviceId(std::string dev_id) {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));
    CHECK(is_initialized_);

    auto it = ports_.find(dev_id);
    if (it == ports_.end())
      return nullptr;
    return it->second.get();
  }

  MidiPort<InterfaceType>* GetPortByIndex(uint32_t port_index) {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));
    CHECK(is_initialized_);

    return GetPortByDeviceId(port_ids_[port_index]);
  }

 protected:
  // Points to the MidiService instance, which is expected to outlive the
  // MidiPortManager instance.
  MidiService* midi_service_;

  // Points to the MidiManagerWinrt instance, which is safe to be accessed
  // from tasks that are invoked by TaskService.
  MidiManagerWinrt* midi_manager_;

 private:
  // DeviceWatcher callbacks:
  void OnAdded(std::string dev_id, std::string dev_name) {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));
    CHECK(is_initialized_);

    port_names_[dev_id] = dev_name;

    ScopedHString dev_id_hstring = ScopedHString::Create(dev_id);
    if (!dev_id_hstring.is_valid())
      return;

    IAsyncOperation<RuntimeType*>* async_op;

    HRESULT hr =
        midi_port_statics_->FromIdAsync(dev_id_hstring.get(), &async_op);
    if (FAILED(hr)) {
      VLOG(1) << "FromIdAsync failed: " << PrintHr(hr);
      return;
    }

    MidiPortManager* port_manager = this;
    TaskService* task_service = midi_service_->task_service();

    hr = async_op->put_Completed(
        WRL::Callback<
            Win::Foundation::IAsyncOperationCompletedHandler<RuntimeType*>>(
            [port_manager, task_service](
                IAsyncOperation<RuntimeType*>* async_op, AsyncStatus status) {
              // A reference to |async_op| is kept in |async_ops_|, safe to pass
              // outside.
              task_service->PostBoundTask(
                  kComTaskRunner,
                  base::BindOnce(
                      &MidiPortManager::OnCompletedGetPortFromIdAsync,
                      base::Unretained(port_manager),
                      base::Unretained(async_op)));

              return S_OK;
            })
            .Get());
    if (FAILED(hr)) {
      VLOG(1) << "put_Completed failed: " << PrintHr(hr);
      return;
    }

    // Keep a reference to incompleted |async_op| for releasing later.
    async_ops_.insert(async_op);
  }

  void OnEnumerationCompleted() {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));
    CHECK(is_initialized_);

    if (async_ops_.empty())
      midi_manager_->OnPortManagerReady();
    else
      enumeration_completed_not_ready_ = true;
  }

  void OnRemoved(std::string dev_id) {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));
    CHECK(is_initialized_);

    // Note: in case Microsoft GS Wavetable Synth triggers this event for some
    // reason, it will be ignored here with log emitted.
    MidiPort<InterfaceType>* port = GetPortByDeviceId(dev_id);
    if (!port) {
      VLOG(1) << "Removing non-existent port " << dev_id;
      return;
    }

    SetPortState(port->index, PortState::DISCONNECTED);

    RemovePortEventHandlers(port);
    port->handle = nullptr;
  }

  void OnCompletedGetPortFromIdAsync(IAsyncOperation<RuntimeType*>* async_op) {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));
    CHECK(is_initialized_);

    InterfaceType* handle = nullptr;
    HRESULT hr = async_op->GetResults(&handle);
    if (FAILED(hr)) {
      VLOG(1) << "GetResults failed: " << PrintHr(hr);
      return;
    }

    // Manually release COM interface to completed |async_op|.
    auto it = async_ops_.find(async_op);
    CHECK(it != async_ops_.end());
    (*it)->Release();
    async_ops_.erase(it);

    if (!handle) {
      VLOG(1) << "Midi{In,Out}Port.FromIdAsync callback provides null pointer, "
                 "ignoring";
      return;
    }

    EventRegistrationToken token = {kInvalidTokenValue};
    if (!RegisterOnMessageReceived(handle, &token))
      return;

    std::string dev_id = GetDeviceIdString(handle);

    MidiPort<InterfaceType>* port = GetPortByDeviceId(dev_id);

    if (port == nullptr) {
      std::string manufacturer = "Unknown", driver_version = "Unknown";
      GetDriverInfoFromDeviceId(dev_id, &manufacturer, &driver_version);

      AddPort(mojom::PortInfo(dev_id, manufacturer, port_names_[dev_id],
                              driver_version, PortState::OPENED));

      port = new MidiPort<InterfaceType>;
      port->index = static_cast<uint32_t>(port_ids_.size());

      ports_[dev_id].reset(port);
      port_ids_.push_back(dev_id);
    } else {
      SetPortState(port->index, PortState::CONNECTED);
    }

    port->handle = handle;
    port->token_MessageReceived = token;

    if (enumeration_completed_not_ready_ && async_ops_.empty()) {
      midi_manager_->OnPortManagerReady();
      enumeration_completed_not_ready_ = false;
    }
  }

  // Overrided by MidiInPortManager to listen to input ports.
  virtual bool RegisterOnMessageReceived(InterfaceType* handle,
                                         EventRegistrationToken* p_token) {
    return true;
  }

  // Overrided by MidiInPortManager to remove MessageReceived event handler.
  virtual void RemovePortEventHandlers(MidiPort<InterfaceType>* port) {}

  // Calls midi_manager_->Add{Input,Output}Port.
  virtual void AddPort(mojom::PortInfo info) = 0;

  // Calls midi_manager_->Set{Input,Output}PortState.
  virtual void SetPortState(uint32_t port_index, PortState state) = 0;

  // Midi{In,Out}PortStatics instance.
  WRL::ComPtr<StaticsInterfaceType> midi_port_statics_;

  // DeviceWatcher instance and event registration tokens for unsubscribing
  // events in destructor.
  WRL::ComPtr<IDeviceWatcher> watcher_;
  EventRegistrationToken token_Added_ = {kInvalidTokenValue},
                         token_EnumerationCompleted_ = {kInvalidTokenValue},
                         token_Removed_ = {kInvalidTokenValue},
                         token_Stopped_ = {kInvalidTokenValue},
                         token_Updated_ = {kInvalidTokenValue};

  // All manipulations to these fields should be done on kComTaskRunner.
  std::unordered_map<std::string, std::unique_ptr<MidiPort<InterfaceType>>>
      ports_;
  std::vector<std::string> port_ids_;
  std::unordered_map<std::string, std::string> port_names_;

  // Keeps AsyncOperation references before the operation completes. Note that
  // raw pointers are used here and the COM interfaces should be released
  // manually.
  std::unordered_set<IAsyncOperation<RuntimeType*>*> async_ops_;

  // Set when device enumeration is completed but OnPortManagerReady() is not
  // called since some ports are not yet ready (i.e. |async_ops_| is not empty).
  // In such cases, OnPortManagerReady() will be called in
  // OnCompletedGetPortFromIdAsync() when the last pending port is ready.
  bool enumeration_completed_not_ready_ = false;

  // Set if the instance is initialized without error. Should be checked in all
  // methods on kComTaskRunner except StartWatcher().
  bool is_initialized_ = false;
};

class MidiManagerWinrt::MidiInPortManager final
    : public MidiPortManager<Win::Devices::Midi::IMidiInPort,
                             Win::Devices::Midi::MidiInPort,
                             Win::Devices::Midi::IMidiInPortStatics,
                             RuntimeClass_Windows_Devices_Midi_MidiInPort> {
 public:
  MidiInPortManager(MidiManagerWinrt* midi_manager)
      : MidiPortManager(midi_manager) {}

 private:
  // MidiPortManager overrides:
  bool RegisterOnMessageReceived(Win::Devices::Midi::IMidiInPort* handle,
                                 EventRegistrationToken* p_token) override {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));

    MidiInPortManager* port_manager = this;
    TaskService* task_service = midi_service_->task_service();

    HRESULT hr = handle->add_MessageReceived(
        WRL::Callback<ITypedEventHandler<
            Win::Devices::Midi::MidiInPort*,
            Win::Devices::Midi::MidiMessageReceivedEventArgs*>>(
            [port_manager, task_service](
                Win::Devices::Midi::IMidiInPort* handle,
                Win::Devices::Midi::IMidiMessageReceivedEventArgs* args) {
              const base::TimeTicks now = base::TimeTicks::Now();

              std::string dev_id = GetDeviceIdString(handle);

              WRL::ComPtr<Win::Devices::Midi::IMidiMessage> message;
              HRESULT hr = args->get_Message(message.GetAddressOf());
              if (FAILED(hr)) {
                VLOG(1) << "get_Message failed: " << PrintHr(hr);
                return hr;
              }

              WRL::ComPtr<Win::Storage::Streams::IBuffer> buffer;
              hr = message->get_RawData(buffer.GetAddressOf());
              if (FAILED(hr)) {
                VLOG(1) << "get_RawData failed: " << PrintHr(hr);
                return hr;
              }

              uint8_t* p_buffer_data = nullptr;
              uint32_t data_length = 0;
              hr = base::win::GetPointerToBufferData(
                  buffer.Get(), &p_buffer_data, &data_length);
              if (FAILED(hr))
                return hr;

              std::vector<uint8_t> data(p_buffer_data,
                                        p_buffer_data + data_length);

              task_service->PostBoundTask(
                  kComTaskRunner,
                  base::BindOnce(&MidiInPortManager::OnMessageReceived,
                                 base::Unretained(port_manager), dev_id, data,
                                 now));

              return S_OK;
            })
            .Get(),
        p_token);
    if (FAILED(hr)) {
      VLOG(1) << "add_MessageReceived failed: " << PrintHr(hr);
      return false;
    }

    return true;
  }

  void RemovePortEventHandlers(
      MidiPort<Win::Devices::Midi::IMidiInPort>* port) override {
    if (!(port->handle &&
          port->token_MessageReceived.value != kInvalidTokenValue))
      return;

    HRESULT hr =
        port->handle->remove_MessageReceived(port->token_MessageReceived);
    VLOG_IF(1, FAILED(hr)) << "remove_MessageReceived failed: " << PrintHr(hr);
    port->token_MessageReceived.value = kInvalidTokenValue;
  }

  void AddPort(mojom::PortInfo info) final {
    midi_manager_->AddInputPort(info);
  }

  void SetPortState(uint32_t port_index, PortState state) final {
    midi_manager_->SetInputPortState(port_index, state);
  }

  // Callback on receiving MIDI input message.
  void OnMessageReceived(std::string dev_id,
                         std::vector<uint8_t> data,
                         base::TimeTicks time) {
    DCHECK(midi_service_->task_service()->IsOnTaskRunner(kComTaskRunner));

    MidiPort<Win::Devices::Midi::IMidiInPort>* port = GetPortByDeviceId(dev_id);
    CHECK(port);

    midi_manager_->ReceiveMidiData(port->index, &data[0], data.size(), time);
  }

  DISALLOW_COPY_AND_ASSIGN(MidiInPortManager);
};

class MidiManagerWinrt::MidiOutPortManager final
    : public MidiPortManager<Win::Devices::Midi::IMidiOutPort,
                             Win::Devices::Midi::IMidiOutPort,
                             Win::Devices::Midi::IMidiOutPortStatics,
                             RuntimeClass_Windows_Devices_Midi_MidiOutPort> {
 public:
  MidiOutPortManager(MidiManagerWinrt* midi_manager)
      : MidiPortManager(midi_manager) {}

 private:
  // MidiPortManager overrides:
  void AddPort(mojom::PortInfo info) final {
    midi_manager_->AddOutputPort(info);
  }

  void SetPortState(uint32_t port_index, PortState state) final {
    midi_manager_->SetOutputPortState(port_index, state);
  }

  DISALLOW_COPY_AND_ASSIGN(MidiOutPortManager);
};

namespace {

// FinalizeOnComRunner() run on kComTaskRunner even after the MidiManager
// instance destruction.
void FinalizeOnComRunner(
    std::unique_ptr<MidiManagerWinrt::MidiInPortManager> port_manager_in,
    std::unique_ptr<MidiManagerWinrt::MidiOutPortManager> port_manager_out) {
  if (port_manager_in)
    port_manager_in->StopWatcher();

  if (port_manager_out)
    port_manager_out->StopWatcher();
}

}  // namespace

MidiManagerWinrt::MidiManagerWinrt(MidiService* service)
    : MidiManager(service) {}

MidiManagerWinrt::~MidiManagerWinrt() {
  // Unbind and take a lock to ensure that InitializeOnComRunner should not run
  // after here.
  if (!service()->task_service()->UnbindInstance())
    return;

  base::AutoLock auto_lock(lazy_init_member_lock_);
  service()->task_service()->PostStaticTask(
      kComTaskRunner,
      base::BindOnce(&FinalizeOnComRunner, std::move(port_manager_in_),
                     std::move(port_manager_out_)));
}

void MidiManagerWinrt::StartInitialization() {
  if (!service()->task_service()->BindInstance())
    return CompleteInitialization(Result::INITIALIZATION_ERROR);

  service()->task_service()->PostBoundTask(
      kComTaskRunner, base::BindOnce(&MidiManagerWinrt::InitializeOnComRunner,
                                     base::Unretained(this)));
}

void MidiManagerWinrt::DispatchSendMidiData(MidiManagerClient* client,
                                            uint32_t port_index,
                                            const std::vector<uint8_t>& data,
                                            base::TimeTicks timestamp) {
  base::TimeDelta delay = MidiService::TimestampToTimeDeltaDelay(timestamp);
  service()->task_service()->PostBoundDelayedTask(
      kComTaskRunner,
      base::BindOnce(&MidiManagerWinrt::SendOnComRunner, base::Unretained(this),
                     port_index, data),
      delay);
  service()->task_service()->PostBoundDelayedTask(
      kComTaskRunner,
      base::BindOnce(&MidiManagerWinrt::AccumulateMidiBytesSent,
                     base::Unretained(this), client, data.size()),
      delay);
}

void MidiManagerWinrt::InitializeOnComRunner() {
  base::AutoLock auto_lock(lazy_init_member_lock_);

  DCHECK(service()->task_service()->IsOnTaskRunner(kComTaskRunner));

  bool preload_success = base::win::ResolveCoreWinRTDelayload() &&
                         ScopedHString::ResolveCoreWinRTStringDelayload();
  if (!preload_success) {
    service()->task_service()->PostBoundTask(
        kDefaultTaskRunner,
        base::BindOnce(&MidiManagerWinrt::CompleteInitialization,
                       base::Unretained(this), Result::INITIALIZATION_ERROR));
    return;
  }

  port_manager_in_.reset(new MidiInPortManager(this));
  port_manager_out_.reset(new MidiOutPortManager(this));

  if (!(port_manager_in_->StartWatcher() &&
        port_manager_out_->StartWatcher())) {
    port_manager_in_->StopWatcher();
    port_manager_out_->StopWatcher();
    service()->task_service()->PostBoundTask(
        kDefaultTaskRunner,
        base::BindOnce(&MidiManagerWinrt::CompleteInitialization,
                       base::Unretained(this), Result::INITIALIZATION_ERROR));
  }
}

void MidiManagerWinrt::SendOnComRunner(uint32_t port_index,
                                       const std::vector<uint8_t>& data) {
  DCHECK(service()->task_service()->IsOnTaskRunner(kComTaskRunner));

  base::AutoLock auto_lock(lazy_init_member_lock_);
  MidiPort<Win::Devices::Midi::IMidiOutPort>* port =
      port_manager_out_->GetPortByIndex(port_index);
  if (!(port && port->handle)) {
    VLOG(1) << "Port not available: " << port_index;
    return;
  }

  WRL::ComPtr<Win::Storage::Streams::IBuffer> buffer;
  HRESULT hr = base::win::CreateIBufferFromData(
      data.data(), static_cast<UINT32>(data.size()), &buffer);
  if (FAILED(hr)) {
    VLOG(1) << "CreateIBufferFromData failed: " << PrintHr(hr);
    return;
  }

  hr = port->handle->SendBuffer(buffer.Get());
  if (FAILED(hr)) {
    VLOG(1) << "SendBuffer failed: " << PrintHr(hr);
    return;
  }
}

void MidiManagerWinrt::OnPortManagerReady() {
  DCHECK(service()->task_service()->IsOnTaskRunner(kComTaskRunner));
  DCHECK(port_manager_ready_count_ < 2);

  if (++port_manager_ready_count_ == 2) {
    service()->task_service()->PostBoundTask(
        kDefaultTaskRunner,
        base::BindOnce(&MidiManagerWinrt::CompleteInitialization,
                       base::Unretained(this), Result::OK));
  }
}

}  // namespace midi
