// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

namespace content {

HidService::HidService(RenderFrameHost* render_frame_host,
                       mojo::PendingReceiver<blink::mojom::HidService> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {
  watchers_.set_disconnect_handler(base::BindRepeating(
      &HidService::OnWatcherConnectionError, base::Unretained(this)));
}

HidService::~HidService() {
  // The remaining watchers will be closed from this end.
  if (!watchers_.empty())
    DecrementActiveFrameCount();
}

// static
void HidService::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  DCHECK(render_frame_host);

  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kHid)) {
    mojo::ReportBadMessage("Feature policy blocks access to HID.");
    return;
  }

  // Avoid creating the HidService if there is no HID delegate to provide the
  // implementation.
  if (!GetContentClient()->browser()->GetHidDelegate())
    return;

  // HidService owns itself. It will self-destruct when a mojo interface error
  // occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new HidService(render_frame_host, std::move(receiver));
}

void HidService::GetDevices(GetDevicesCallback callback) {
  GetContentClient()
      ->browser()
      ->GetHidDelegate()
      ->GetHidManager(web_contents())
      ->GetDevices(base::BindOnce(&HidService::FinishGetDevices,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void HidService::RequestDevice(
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    RequestDeviceCallback callback) {
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->CanRequestDevicePermission(web_contents(), origin())) {
    std::move(callback).Run(nullptr);
    return;
  }

  chooser_ = delegate->RunChooser(
      render_frame_host(), std::move(filters),
      base::BindOnce(&HidService::FinishRequestDevice,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HidService::Connect(
    const std::string& device_guid,
    mojo::PendingRemote<device::mojom::HidConnectionClient> client,
    ConnectCallback callback) {
  if (watchers_.empty()) {
    auto* web_contents_impl = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(render_frame_host()));
    web_contents_impl->IncrementHidActiveFrameCount();
  }

  mojo::PendingRemote<device::mojom::HidConnectionWatcher> watcher;
  watchers_.Add(this, watcher.InitWithNewPipeAndPassReceiver());
  GetContentClient()
      ->browser()
      ->GetHidDelegate()
      ->GetHidManager(web_contents())
      ->Connect(
          device_guid, std::move(client), std::move(watcher),
          base::BindOnce(&HidService::FinishConnect, weak_factory_.GetWeakPtr(),
                         std::move(callback)));
}

void HidService::OnWatcherConnectionError() {
  if (watchers_.empty())
    DecrementActiveFrameCount();
}

void HidService::DecrementActiveFrameCount() {
  auto* web_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host()));
  web_contents_impl->DecrementHidActiveFrameCount();
}

void HidService::FinishGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  std::vector<device::mojom::HidDeviceInfoPtr> result;
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  for (auto& device : devices) {
    if (delegate->HasDevicePermission(web_contents(), origin(), *device))
      result.push_back(std::move(device));
  }

  std::move(callback).Run(std::move(result));
}

void HidService::FinishRequestDevice(RequestDeviceCallback callback,
                                     device::mojom::HidDeviceInfoPtr device) {
  if (!device) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(device));
}

void HidService::FinishConnect(
    ConnectCallback callback,
    mojo::PendingRemote<device::mojom::HidConnection> connection) {
  if (!connection) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  std::move(callback).Run(std::move(connection));
}

}  // namespace content
