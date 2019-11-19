// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_bluetooth_chooser_factory.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

namespace content {

class WebContents;

// Implements a Bluetooth chooser that records events it's sent, instead of
// showing a dialog. It allows tests to control how the chooser responds.
class WebTestBluetoothChooserFactory::Chooser : public BluetoothChooser {
 public:
  Chooser(const base::WeakPtr<WebTestBluetoothChooserFactory>& factory,
          const EventHandler& event_handler)
      : event_handler(event_handler), factory_(factory) {
    DCHECK(factory);
    factory->choosers_.insert(this);
  }

  ~Chooser() override {
    CheckFactory();
    factory_->choosers_.erase(this);
  }

  // BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override {
    CheckFactory();
    switch (presence) {
      case AdapterPresence::ABSENT:
        factory_->events_.push_back("adapter-removed");
        break;
      case AdapterPresence::POWERED_OFF:
        factory_->events_.push_back("adapter-disabled");
        break;
      case AdapterPresence::POWERED_ON:
        factory_->events_.push_back("adapter-enabled");
        break;
    }
  }

  void ShowDiscoveryState(DiscoveryState state) override {
    CheckFactory();
    switch (state) {
      case DiscoveryState::FAILED_TO_START:
        factory_->events_.push_back("discovery-failed-to-start");
        break;
      case DiscoveryState::DISCOVERING:
        factory_->events_.push_back("discovering");
        break;
      case DiscoveryState::IDLE:
        factory_->events_.push_back("discovery-idle");
        break;
    }
  }

  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override {
    CheckFactory();
    std::string event = "add-device(";
    event += base::UTF16ToUTF8(device_name);
    event += ")=";
    event += device_id;
    factory_->events_.push_back(event);
  }

  EventHandler event_handler;

 private:
  void CheckFactory() const {
    CHECK(factory_) << "The factory should cancel all choosers in its "
                       "destructor, and choosers should be destroyed "
                       "synchronously when canceled.";
  }

  base::WeakPtr<WebTestBluetoothChooserFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(Chooser);
};

WebTestBluetoothChooserFactory::WebTestBluetoothChooserFactory() {}

WebTestBluetoothChooserFactory::~WebTestBluetoothChooserFactory() {
  SendEvent(BluetoothChooser::Event::CANCELLED, "");
}

std::unique_ptr<BluetoothChooser>
WebTestBluetoothChooserFactory::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  const url::Origin origin = frame->GetLastCommittedOrigin();
  DCHECK(!origin.opaque());
  std::string event = "chooser-opened(";
  event += origin.Serialize();
  event += ")";
  events_.push_back(event);
  return std::make_unique<Chooser>(weak_this_.GetWeakPtr(), event_handler);
}

std::vector<std::string> WebTestBluetoothChooserFactory::GetAndResetEvents() {
  std::vector<std::string> result;
  result.swap(events_);
  return result;
}

void WebTestBluetoothChooserFactory::SendEvent(BluetoothChooser::Event event,
                                               const std::string& device_id) {
  // Copy |choosers_| to make sure event handler executions that modify
  // |choosers_| don't invalidate iterators.
  std::vector<Chooser*> choosers_copy(choosers_.begin(), choosers_.end());
  for (Chooser* chooser : choosers_copy) {
    chooser->event_handler.Run(event, device_id);
  }
}

}  // namespace content
