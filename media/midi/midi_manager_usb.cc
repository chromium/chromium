// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_usb.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "media/midi/midi_service.h"
#include "media/midi/task_service.h"
#include "media/midi/usb_midi_descriptor_parser.h"

namespace midi {

using mojom::PortState;
using mojom::Result;

MidiManagerUsb::MidiManagerUsb(MidiService* service,
                               std::unique_ptr<UsbMidiDevice::Factory> factory)
    : MidiManager(service), device_factory_(std::move(factory)) {}

MidiManagerUsb::~MidiManagerUsb() {
  if (!service()->task_service()->UnbindInstance())
    return;

  // Finalization steps should be implemented after the UnbindInstance() call
  // above, if we need.
}

void MidiManagerUsb::StartInitialization() {
  if (!service()->task_service()->BindInstance())
    return CompleteInitialization(Result::INITIALIZATION_ERROR);

  Initialize();
}

void MidiManagerUsb::Initialize() {
  // This is safe because EnumerateDevices cancels the operation on destruction.
  device_factory_->EnumerateDevices(
      this, base::BindOnce(&MidiManagerUsb::OnEnumerateDevicesDone,
                           base::Unretained(this)));
}

void MidiManagerUsb::DispatchSendMidiData(MidiManagerClient* client,
                                          uint32_t port_index,
                                          const std::vector<uint8_t>& data,
                                          base::TimeTicks timestamp) {
  if (port_index >= output_streams_.size()) {
    // |port_index| is provided by a renderer so we can't believe that it is
    // in the valid range.
    return;
  }
  base::TimeDelta delay = MidiService::TimestampToTimeDeltaDelay(timestamp);

  // output_streams_[port_index] is alive unless MidiManagerUsb is deleted.
  service()->task_service()->PostBoundDelayedTask(
      TaskService::kDefaultRunnerId,
      base::BindOnce(&UsbMidiOutputStream::Send,
                     base::Unretained(output_streams_[port_index].get()), data),
      delay);
  service()->task_service()->PostBoundDelayedTask(
      TaskService::kDefaultRunnerId,
      base::BindOnce(&MidiManager::AccumulateMidiBytesSent,
                     base::Unretained(this), client, data.size()),
      delay);
}

void MidiManagerUsb::ReceiveUsbMidiData(UsbMidiDevice* device,
                                        int endpoint_number,
                                        const uint8_t* data,
                                        size_t size,
                                        base::TimeTicks time) {
  if (!input_stream_)
    return;
  input_stream_->OnReceivedData(device,
                                endpoint_number,
                                data,
                                size,
                                time);
}

void MidiManagerUsb::OnDeviceAttached(std::unique_ptr<UsbMidiDevice> device) {
  int device_id = static_cast<int>(devices_.size());
  devices_.push_back(std::move(device));
  AddPorts(devices_.back().get(), device_id);
}

void MidiManagerUsb::OnDeviceDetached(size_t index) {
  if (index >= devices_.size()) {
    return;
  }
  UsbMidiDevice* device = devices_[index].get();
  for (size_t i = 0; i < output_streams_.size(); ++i) {
    if (output_streams_[i]->jack().device == device) {
      SetOutputPortState(static_cast<uint32_t>(i), PortState::DISCONNECTED);
    }
  }
  const std::vector<UsbMidiJack>& input_jacks = input_stream_->jacks();
  for (size_t i = 0; i < input_jacks.size(); ++i) {
    if (input_jacks[i].device == device) {
      SetInputPortState(static_cast<uint32_t>(i), PortState::DISCONNECTED);
    }
  }
}

void MidiManagerUsb::OnReceivedData(size_t jack_index,
                                    const uint8_t* data,
                                    size_t size,
                                    base::TimeTicks time) {
  ReceiveMidiData(static_cast<uint32_t>(jack_index), data, size, time);
}


void MidiManagerUsb::OnEnumerateDevicesDone(bool result,
                                            UsbMidiDevice::Devices* devices) {
  if (result) {
    input_stream_ = std::make_unique<UsbMidiInputStream>(this);
    devices->swap(devices_);
    for (size_t i = 0; i < devices_.size(); ++i) {
      if (!AddPorts(devices_[i].get(), static_cast<int>(i))) {
        result = false;
        break;
      }
    }
  }
  service()->task_service()->PostBoundTask(
      TaskService::kDefaultRunnerId,
      base::BindOnce(&MidiManager::CompleteInitialization,
                     base::Unretained(this),
                     result ? Result::OK : Result::INITIALIZATION_ERROR));
}

bool MidiManagerUsb::AddPorts(UsbMidiDevice* device, int device_id) {
  UsbMidiDescriptorParser parser;
  std::vector<uint8_t> descriptor = device->GetDescriptors();
  const uint8_t* data = descriptor.size() > 0 ? &descriptor[0] : NULL;
  std::vector<UsbMidiJack> jacks;
  bool parse_result = parser.Parse(device,
                                   data,
                                   descriptor.size(),
                                   &jacks);
  if (!parse_result)
    return false;

  std::string manufacturer(device->GetManufacturer());
  std::string product_name(device->GetProductName());
  std::string version(device->GetDeviceVersion());

  for (size_t j = 0; j < jacks.size(); ++j) {
    // Port ID must be unique in a MIDI manager. This ID setting is
    // sufficiently unique although there is no user-friendly meaning.
    // TODO(yhirano): Use a hashed string as ID.
    std::string id(
        base::StringPrintf("usb:port-%d-%ld", device_id, static_cast<long>(j)));
    if (jacks[j].direction() == UsbMidiJack::DIRECTION_OUT) {
      output_streams_.push_back(
          std::make_unique<UsbMidiOutputStream>(jacks[j]));
      AddOutputPort(mojom::PortInfo(id, manufacturer, product_name, version,
                                    PortState::OPENED));
    } else {
      DCHECK_EQ(jacks[j].direction(), UsbMidiJack::DIRECTION_IN);
      input_stream_->Add(jacks[j]);
      AddInputPort(mojom::PortInfo(id, manufacturer, product_name, version,
                                   PortState::OPENED));
    }
  }
  return true;
}

}  // namespace midi
