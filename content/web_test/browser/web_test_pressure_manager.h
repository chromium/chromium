// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_PRESSURE_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_PRESSURE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager_automation.mojom.h"

namespace content {

class ScopedVirtualPressureSourceForDevTools;
class WebContents;

// Implementation of the WebPressureManagerAutomation Mojo interface for use by
// Blink's InternalsComputePressure. It implements the required method calls
// exposed by testdriver.js by forwarding the calls to
// WebContentsPressureManagerProxy.
class WebTestPressureManager
    : public blink::test::mojom::WebPressureManagerAutomation,
      public WebContentsUserData<WebTestPressureManager> {
 public:
  static WebTestPressureManager* GetOrCreate(WebContents*);

  WebTestPressureManager(const WebTestPressureManager&) = delete;
  WebTestPressureManager& operator=(const WebTestPressureManager&) = delete;

  ~WebTestPressureManager() override;

  void Bind(
      mojo::PendingReceiver<blink::test::mojom::WebPressureManagerAutomation>
          receiver);

  // blink::mojom::WebPressureManagerAutomation overrides.
  void CreateVirtualPressureSource(
      device::mojom::PressureSource source,
      device::mojom::VirtualPressureSourceMetadataPtr metadata,
      CreateVirtualPressureSourceCallback callback) override;
  void RemoveVirtualPressureSource(
      device::mojom::PressureSource source,
      RemoveVirtualPressureSourceCallback callback) override;
  void UpdateVirtualPressureSourceState(
      device::mojom::PressureSource source,
      device::mojom::PressureState state,
      UpdateVirtualPressureSourceStateCallback callback) override;

 private:
  explicit WebTestPressureManager(WebContents* web_contents);

  base::flat_map<device::mojom::PressureSource,
                 std::unique_ptr<ScopedVirtualPressureSourceForDevTools>>
      pressure_source_overrides_;

  mojo::ReceiverSet<blink::test::mojom::WebPressureManagerAutomation>
      receivers_;

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_PRESSURE_MANAGER_H_
