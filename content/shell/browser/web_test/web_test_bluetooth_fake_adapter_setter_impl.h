// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_FAKE_ADAPTER_SETTER_IMPL_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_FAKE_ADAPTER_SETTER_IMPL_H_

#include "base/macros.h"
#include "content/shell/common/web_test/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class WebTestBluetoothFakeAdapterSetterImpl
    : public mojom::WebTestBluetoothFakeAdapterSetter {
 public:
  WebTestBluetoothFakeAdapterSetterImpl();
  ~WebTestBluetoothFakeAdapterSetterImpl() override;

  static void Create(
      mojo::PendingReceiver<mojom::WebTestBluetoothFakeAdapterSetter> receiver);

 private:
  void Set(const std::string& adapter_name, SetCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(WebTestBluetoothFakeAdapterSetterImpl);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_FAKE_ADAPTER_SETTER_IMPL_H_
