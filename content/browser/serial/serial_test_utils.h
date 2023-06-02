// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERIAL_SERIAL_TEST_UTILS_H_
#define CONTENT_BROWSER_SERIAL_SERIAL_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/serial_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSerialDelegate : public SerialDelegate {
 public:
  MockSerialDelegate();

  MockSerialDelegate(const MockSerialDelegate&) = delete;
  MockSerialDelegate& operator=(const MockSerialDelegate&) = delete;

  ~MockSerialDelegate() override;

  std::unique_ptr<SerialChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids,
      SerialChooser::Callback callback) override;

  MOCK_METHOD0(RunChooserInternal, device::mojom::SerialPortInfoPtr());
  MOCK_METHOD1(CanRequestPortPermission, bool(RenderFrameHost* frame));
  MOCK_METHOD2(HasPortPermission,
               bool(RenderFrameHost* frame,
                    const device::mojom::SerialPortInfo& port));
  MOCK_METHOD2(RevokePortPermissionWebInitiated,
               void(RenderFrameHost* frame,
                    const base::UnguessableToken& token));
  MOCK_METHOD2(GetPortInfo,
               const device::mojom::SerialPortInfo*(
                   RenderFrameHost* frame,
                   const base::UnguessableToken& token));
  MOCK_METHOD1(GetPortManager,
               device::mojom::SerialPortManager*(RenderFrameHost* frame));
  MOCK_METHOD2(AddObserver, void(RenderFrameHost* frame, Observer* observer));
  MOCK_METHOD2(RemoveObserver,
               void(RenderFrameHost* frame, Observer* observer));
};

class SerialTestContentBrowserClient : public ContentBrowserClient {
 public:
  SerialTestContentBrowserClient();

  SerialTestContentBrowserClient(const SerialTestContentBrowserClient&) =
      delete;
  SerialTestContentBrowserClient& operator=(
      const SerialTestContentBrowserClient&) = delete;

  ~SerialTestContentBrowserClient() override;

  MockSerialDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  SerialDelegate* GetSerialDelegate() override;

 private:
  MockSerialDelegate delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERIAL_SERIAL_TEST_UTILS_H_
