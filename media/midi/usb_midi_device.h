// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DEVICE_H_
#define MEDIA_MIDI_USB_MIDI_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/midi/usb_midi_export.h"

namespace midi {

class UsbMidiDevice;

// Delegate class for UsbMidiDevice.
// Each method is called when an corresponding event arrives at the device.
class USB_MIDI_EXPORT UsbMidiDeviceDelegate {
 public:
  virtual ~UsbMidiDeviceDelegate() {}

  // Called when USB-MIDI data arrives at |device|.
  virtual void ReceiveUsbMidiData(UsbMidiDevice* device,
                                  int endpoint_number,
                                  const uint8_t* data,
                                  size_t size,
                                  base::TimeTicks time) = 0;

  // Called when a USB-MIDI device is attached.
  virtual void OnDeviceAttached(std::unique_ptr<UsbMidiDevice> device) = 0;
  // Called when a USB-MIDI device is detached.
  virtual void OnDeviceDetached(size_t index) = 0;
};

// UsbMidiDevice represents a USB-MIDI device.
// This is an interface class and each platform-dependent implementation class
// will be a derived class.
class USB_MIDI_EXPORT UsbMidiDevice {
 public:
  typedef std::vector<std::unique_ptr<UsbMidiDevice>> Devices;

  // Factory class for USB-MIDI devices.
  // Each concrete implementation will find and create devices
  // in platform-dependent way.
  class Factory {
   public:
    typedef base::OnceCallback<void(bool result, Devices* devices)> Callback;
    virtual ~Factory() {}

    // Enumerates devices.
    // Devices that have no USB-MIDI interfaces can be omitted.
    // When the operation succeeds, |callback| will be called with |true| and
    // devices.
    // Otherwise |callback| will be called with |false| and empty devices.
    // When this factory is destroyed during the operation, the operation
    // will be canceled silently (i.e. |callback| will not be called).
    // This function can be called at most once per instance.
    virtual void EnumerateDevices(UsbMidiDeviceDelegate* delegate,
                                  Callback callback) = 0;
  };

  virtual ~UsbMidiDevice() {}

  // Returns the descriptors of this device.
  virtual std::vector<uint8_t> GetDescriptors() = 0;

  // Return the name of the manufacturer.
  virtual std::string GetManufacturer() = 0;

  // Return the name of the device.
  virtual std::string GetProductName() = 0;

  // Return the device version.
  virtual std::string GetDeviceVersion() = 0;

  // Sends |data| to the given USB endpoint of this device.
  virtual void Send(int endpoint_number, const std::vector<uint8_t>& data) = 0;
};

}  // namespace midi

#endif  // MEDIA_MIDI_USB_MIDI_DEVICE_H_
