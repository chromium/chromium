// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_pressure_manager.h"

#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {

WebTestPressureManager::WebTestPressureManager(WebContents* web_contents)
    : WebContentsUserData<WebTestPressureManager>(*web_contents) {}

WebTestPressureManager::~WebTestPressureManager() = default;

// static
WebTestPressureManager* WebTestPressureManager::GetOrCreate(
    WebContents* web_contents) {
  WebContentsUserData<WebTestPressureManager>::CreateForWebContents(
      web_contents);
  return WebContentsUserData<WebTestPressureManager>::FromWebContents(
      web_contents);
}

void WebTestPressureManager::Bind(
    mojo::PendingReceiver<blink::test::mojom::WebPressureManagerAutomation>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void WebTestPressureManager::CreateVirtualPressureSource(
    device::mojom::PressureSource source,
    device::mojom::VirtualPressureSourceMetadataPtr metadata,
    CreateVirtualPressureSourceCallback callback) {
  if (pressure_source_overrides_.contains(source)) {
    std::move(callback).Run(
        blink::test::mojom::CreateVirtualPressureSourceResult::
            kSourceTypeAlreadyOverridden);
    return;
  }

  auto virtual_pressure_source =
      WebContentsPressureManagerProxy::GetOrCreate(&GetWebContents())
          ->CreateVirtualPressureSourceForDevTools(source, std::move(metadata));
  CHECK(virtual_pressure_source);
  pressure_source_overrides_[source] = std::move(virtual_pressure_source);

  std::move(callback).Run(
      blink::test::mojom::CreateVirtualPressureSourceResult::kSuccess);
}

void WebTestPressureManager::RemoveVirtualPressureSource(
    device::mojom::PressureSource source,
    RemoveVirtualPressureSourceCallback callback) {
  pressure_source_overrides_.erase(source);
  std::move(callback).Run();
}

void WebTestPressureManager::UpdateVirtualPressureSourceState(
    device::mojom::PressureSource source,
    device::mojom::PressureState state,
    UpdateVirtualPressureSourceStateCallback callback) {
  auto it = pressure_source_overrides_.find(source);
  if (it == pressure_source_overrides_.end()) {
    std::move(callback).Run(
        blink::test::mojom::UpdateVirtualPressureSourceStateResult::
            kSourceTypeNotOverridden);
    return;
  }

  it->second->UpdateVirtualPressureSourceState(
      state,
      base::BindOnce(std::move(callback),
                     blink::test::mojom::
                         UpdateVirtualPressureSourceStateResult::kSuccess));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebTestPressureManager);

}  // namespace content
