// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_CHOOSER_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_CHOOSER_H_

#include <memory>

#include "content/public/browser/bluetooth_chooser.h"
#include "content/shell/common/web_test/fake_bluetooth_chooser.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/origin.h"

namespace content {

// Implementation of FakeBluetoothChooser in
// src/content/shell/common/web_test/fake_bluetooth_chooser.mojom
// to provide a method of controlling the Bluetooth chooser during a test.
// Serves as a Bluetooth chooser factory for choosers that can be manually
// controlled through the Mojo API. Only one instance of this class will exist
// while the chooser is active.
//
// The implementation details for FakeBluetoothChooser can be found in the Web
// Bluetooth Test Scanning design document.
// https://docs.google.com/document/d/1XFl_4ZAgO8ddM6U53A9AfUuZeWgJnlYD5wtbXqEpzeg
//
// Intended to only be used through the FakeBluetoothChooser Mojo interface.
class FakeBluetoothChooser : public mojom::FakeBluetoothChooser,
                             public BluetoothChooser {
 public:
  // FakeBluetoothChooserFactory will create an instance of this class when its
  // CreateFakeBluetoothChooser() method is called. It will maintain ownership
  // of the instance temporarily until the chooser is opened. When the chooser
  // is opened, ownership of this instance will shift to the caller of
  // WebContentsDelegate::RunBluetoothChooser.
  FakeBluetoothChooser(
      mojo::PendingReceiver<mojom::FakeBluetoothChooser> receiver,
      mojo::PendingAssociatedRemote<mojom::FakeBluetoothChooserClient> client);

  // Resets the test scan duration to timeout immediately and sends a
  // |CHOOSER_CLOSED| event to the client.
  ~FakeBluetoothChooser() override;

  // Sets the EventHandler that will handle events produced by the chooser, and
  // sends a |CHOOSER_OPENED| event to the client with the |origin|.
  void OnRunBluetoothChooser(const EventHandler& event_handler,
                             const url::Origin& origin);

  // mojom::FakeBluetoothChooser overrides:
  void SelectPeripheral(const std::string& peripheral_address) override;
  void Cancel() override;
  void Rescan() override;

  // BluetoothChooser overrides:

  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

 private:
  // Stores the callback function that handles chooser events.
  EventHandler event_handler_;

  mojo::Receiver<mojom::FakeBluetoothChooser> receiver_;

  // Stores the associated pointer to the client that will be receiving events
  // from FakeBluetoothChooser.
  mojo::AssociatedRemote<mojom::FakeBluetoothChooserClient> client_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothChooser);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_CHOOSER_H_
