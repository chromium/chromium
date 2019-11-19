// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERIAL_SERIAL_TEST_UTILS_H_
#define CONTENT_BROWSER_SERIAL_SERIAL_TEST_UTILS_H_

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/serial_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSerialDelegate : public SerialDelegate {
 public:
  MockSerialDelegate();
  ~MockSerialDelegate() override;

  std::unique_ptr<SerialChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      SerialChooser::Callback callback) override;

  MOCK_METHOD0(RunChooserInternal, device::mojom::SerialPortInfoPtr());
  MOCK_METHOD1(CanRequestPortPermission, bool(RenderFrameHost* frame));
  MOCK_METHOD2(HasPortPermission,
               bool(content::RenderFrameHost* frame,
                    const device::mojom::SerialPortInfo& port));
  MOCK_METHOD1(
      GetPortManager,
      device::mojom::SerialPortManager*(content::RenderFrameHost* frame));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSerialDelegate);
};

class SerialTestContentBrowserClient : public ContentBrowserClient {
 public:
  SerialTestContentBrowserClient();
  ~SerialTestContentBrowserClient() override;

  MockSerialDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  SerialDelegate* GetSerialDelegate() override;

 private:
  MockSerialDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(SerialTestContentBrowserClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERIAL_SERIAL_TEST_UTILS_H_
