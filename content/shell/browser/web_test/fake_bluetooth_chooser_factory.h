// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_CHOOSER_FACTORY_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_CHOOSER_FACTORY_H_

#include <memory>

#include "content/shell/common/web_test/fake_bluetooth_chooser.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class FakeBluetoothChooser;

// Implementation of FakeBluetoothChooserFactory in
// src/content/shell/common/web_test/fake_bluetooth_chooser.mojom to create
// FakeBluetoothChoosers with a
// mojo::PendingAssociatedRemote<FakeBluetoothChooserClient> that they can use
// to send events to the client.
//
// The implementation details for FakeBluetoothChooser can be found in the Web
// Bluetooth Test Scanning design document.
// https://docs.google.com/document/d/1XFl_4ZAgO8ddM6U53A9AfUuZeWgJnlYD5wtbXqEpzeg
//
// Intended to only be used through the FakeBluetoothChooser Mojo interface.
class FakeBluetoothChooserFactory : public mojom::FakeBluetoothChooserFactory {
 public:
  ~FakeBluetoothChooserFactory() override;

  // WebTestContentBrowserClient will create an instance of this class when a
  // receiver is bound. It will maintain ownership of the instance.
  static std::unique_ptr<FakeBluetoothChooserFactory> Create(
      mojo::PendingReceiver<mojom::FakeBluetoothChooserFactory> receiver) {
    return base::WrapUnique(
        new FakeBluetoothChooserFactory(std::move(receiver)));
  }

  // Creates an instance of FakeBluetoothChooser and stores it in
  // |next_fake_bluetooth_chooser_|. This will DCHECK if
  // |next_fake_bluetooth_chooser_| is not null.
  void CreateFakeBluetoothChooser(
      mojo::PendingReceiver<mojom::FakeBluetoothChooser> receiver,
      mojo::PendingAssociatedRemote<mojom::FakeBluetoothChooserClient> client)
      override;

  // Transfers ownership of |next_fake_bluetooth_chooser_| to the caller.
  std::unique_ptr<FakeBluetoothChooser> GetNextFakeBluetoothChooser();

 private:
  explicit FakeBluetoothChooserFactory(
      mojo::PendingReceiver<mojom::FakeBluetoothChooserFactory> receiver);

  mojo::Receiver<mojom::FakeBluetoothChooserFactory> receiver_;

  std::unique_ptr<FakeBluetoothChooser> next_fake_bluetooth_chooser_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_FAKE_BLUETOOTH_CHOOSER_FACTORY_H_
