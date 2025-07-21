// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_context.h"

#include <atomic>
#include <memory>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
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

  UsbEventHandler(const UsbEventHandler&) = delete;
  UsbEventHandler& operator=(const UsbEventHandler&) = delete;

  ~UsbEventHandler() override;

  // base::SimpleThread
  void Run() override;

  void Stop();

 private:
  std::atomic<bool> running_{true};
  raw_ptr<libusb_context, DanglingUntriaged> context_;
};

UsbContext::UsbEventHandler::UsbEventHandler(libusb_context* context)
    : base::SimpleThread("UsbEventHandler"), context_(context) {
}

UsbContext::UsbEventHandler::~UsbEventHandler() {
  libusb_exit(context_);
}

void UsbContext::UsbEventHandler::Run() {
  VLOG(1) << "UsbEventHandler started.";

  while (running_.load(std::memory_order_acquire)) {
    const int rv = libusb_handle_events(context_);
    if (rv != LIBUSB_SUCCESS) {
      VLOG(1) << "Failed to handle events: "
              << ConvertPlatformUsbErrorToString(rv);
    }
  }

  VLOG(1) << "UsbEventHandler shutting down.";
}

void UsbContext::UsbEventHandler::Stop() {
  running_.store(false, std::memory_order_release);
  libusb_interrupt_handle_event(context_);
}

UsbContext::UsbContext(PlatformUsbContext context) : context_(context) {
  // Ownership of the PlatformUsbContext is passed to the event handler thread.
  event_handler_ = std::make_unique<UsbEventHandler>(context_);
  event_handler_->Start();
}

UsbContext::~UsbContext() {
  event_handler_->Stop();

  // Temporary workaround for https://crbug.com/1150182 until the libusb backend
  // is removed in https://crbug.com/1096743. The last outstanding transfer can
  // cause this class to be released on a worker thread where blocking is not
  // typically allowed. Make an exception here as this will only occur during
  // shutdown.
  base::ScopedAllowBaseSyncPrimitives allow_sync;
  event_handler_->Join();
}

}  // namespace device
