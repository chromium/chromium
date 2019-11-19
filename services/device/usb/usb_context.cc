// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_context.h"

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "services/device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/interrupt.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

// The UsbEventHandler works around a design flaw in the libusb interface. There
// is currently no way to signal to libusb that any caller into one of the event
// handler calls should return without handling any events.
class UsbContext::UsbEventHandler : public base::SimpleThread {
 public:
  explicit UsbEventHandler(libusb_context* context);
  ~UsbEventHandler() override;

  // base::SimpleThread
  void Run() override;

  void Stop();

 private:
  base::subtle::Atomic32 running_;
  libusb_context* context_;
  DISALLOW_COPY_AND_ASSIGN(UsbEventHandler);
};

UsbContext::UsbEventHandler::UsbEventHandler(libusb_context* context)
    : base::SimpleThread("UsbEventHandler"), context_(context) {
  base::subtle::Release_Store(&running_, 1);
}

UsbContext::UsbEventHandler::~UsbEventHandler() {
  libusb_exit(context_);
}

void UsbContext::UsbEventHandler::Run() {
  VLOG(1) << "UsbEventHandler started.";

  while (base::subtle::Acquire_Load(&running_)) {
    const int rv = libusb_handle_events(context_);
    if (rv != LIBUSB_SUCCESS) {
      VLOG(1) << "Failed to handle events: "
              << ConvertPlatformUsbErrorToString(rv);
    }
  }

  VLOG(1) << "UsbEventHandler shutting down.";
}

void UsbContext::UsbEventHandler::Stop() {
  base::subtle::Release_Store(&running_, 0);
  libusb_interrupt_handle_event(context_);
}

UsbContext::UsbContext(PlatformUsbContext context) : context_(context) {
  // Ownership of the PlatformUsbContext is passed to the event handler thread.
  event_handler_.reset(new UsbEventHandler(context_));
  event_handler_->Start();
}

UsbContext::~UsbContext() {
  event_handler_->Stop();
  event_handler_->Join();
}

}  // namespace device
