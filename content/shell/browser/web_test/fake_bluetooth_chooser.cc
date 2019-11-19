// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/fake_bluetooth_chooser.h"

#include <string>
#include <utility>

#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/test/web_test_support.h"
#include "content/shell/common/web_test/fake_bluetooth_chooser.mojom.h"

namespace content {

FakeBluetoothChooser::FakeBluetoothChooser(
    mojo::PendingReceiver<mojom::FakeBluetoothChooser> receiver,
    mojo::PendingAssociatedRemote<mojom::FakeBluetoothChooserClient> client)
    : receiver_(this, std::move(receiver)), client_(std::move(client)) {
  SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting::kNeverTimeout);
}

FakeBluetoothChooser::~FakeBluetoothChooser() {
  SetTestBluetoothScanDuration(
      BluetoothTestScanDurationSetting::kImmediateTimeout);

  client_->OnEvent(mojom::FakeBluetoothChooserEvent::New(
      mojom::ChooserEventType::CHOOSER_CLOSED, /*origin=*/base::nullopt,
      /*peripheral_address=*/base::nullopt));
}

void FakeBluetoothChooser::OnRunBluetoothChooser(
    const EventHandler& event_handler,
    const url::Origin& origin) {
  event_handler_ = event_handler;
  client_->OnEvent(mojom::FakeBluetoothChooserEvent::New(
      mojom::ChooserEventType::CHOOSER_OPENED, origin,
      /*peripheral_address=*/base::nullopt));
}

// mojom::FakeBluetoothChooser overrides
void FakeBluetoothChooser::SelectPeripheral(
    const std::string& peripheral_address) {
  DCHECK(event_handler_);
  event_handler_.Run(BluetoothChooser::Event::SELECTED, peripheral_address);
}

void FakeBluetoothChooser::Cancel() {
  DCHECK(event_handler_);
  event_handler_.Run(BluetoothChooser::Event::CANCELLED, std::string());
  client_->OnEvent(mojom::FakeBluetoothChooserEvent::New(
      mojom::ChooserEventType::CHOOSER_CLOSED, /*origin=*/base::nullopt,
      /*peripheral_address=*/base::nullopt));
}

void FakeBluetoothChooser::Rescan() {
  DCHECK(event_handler_);
  event_handler_.Run(BluetoothChooser::Event::RESCAN, std::string());
  client_->OnEvent(mojom::FakeBluetoothChooserEvent::New(
      mojom::ChooserEventType::DISCOVERING, /*origin=*/base::nullopt,
      /*peripheral_address=*/base::nullopt));
}

// BluetoothChooser overrides

void FakeBluetoothChooser::SetAdapterPresence(AdapterPresence presence) {
  mojom::FakeBluetoothChooserEventPtr event_ptr =
      mojom::FakeBluetoothChooserEvent::New();
  switch (presence) {
    case AdapterPresence::ABSENT:
      event_ptr->type = mojom::ChooserEventType::ADAPTER_REMOVED;
      break;
    case AdapterPresence::POWERED_OFF:
      event_ptr->type = mojom::ChooserEventType::ADAPTER_DISABLED;
      break;
    case AdapterPresence::POWERED_ON:
      event_ptr->type = mojom::ChooserEventType::ADAPTER_ENABLED;
      break;
  }
  client_->OnEvent(std::move(event_ptr));
}

void FakeBluetoothChooser::ShowDiscoveryState(DiscoveryState state) {
  mojom::FakeBluetoothChooserEventPtr event_ptr =
      mojom::FakeBluetoothChooserEvent::New();
  switch (state) {
    case DiscoveryState::FAILED_TO_START:
      event_ptr->type = mojom::ChooserEventType::DISCOVERY_FAILED_TO_START;
      break;
    case DiscoveryState::DISCOVERING:
      event_ptr->type = mojom::ChooserEventType::DISCOVERING;
      break;
    case DiscoveryState::IDLE:
      event_ptr->type = mojom::ChooserEventType::DISCOVERY_IDLE;
      break;
  }
  client_->OnEvent(std::move(event_ptr));
}

void FakeBluetoothChooser::AddOrUpdateDevice(const std::string& device_id,
                                             bool should_update_name,
                                             const base::string16& device_name,
                                             bool is_gatt_connected,
                                             bool is_paired,
                                             int signal_strength_level) {
  client_->OnEvent(mojom::FakeBluetoothChooserEvent::New(
      mojom::ChooserEventType::ADD_OR_UPDATE_DEVICE,
      /*origin=*/base::nullopt, /*peripheral_address=*/device_id));
}

}  // namespace content
