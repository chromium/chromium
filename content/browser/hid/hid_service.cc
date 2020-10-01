// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_service.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/stack_trace.h"
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
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      requesting_origin_(render_frame_host->GetLastCommittedOrigin()),
      embedding_origin_(
          render_frame_host->GetMainFrame()->GetLastCommittedOrigin()) {
  watchers_.set_disconnect_handler(
      base::BindRepeating(&HidService::OnWatcherRemoved, base::Unretained(this),
                          true /* cleanup_watcher_ids */));

  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (delegate)
    delegate->AddObserver(render_frame_host, this);
}

HidService::~HidService() {
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (delegate)
    delegate->RemoveObserver(render_frame_host(), this);

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

void HidService::RegisterClient(
    mojo::PendingAssociatedRemote<device::mojom::HidManagerClient> client) {
  clients_.Add(std::move(client));
}

void HidService::GetDevices(GetDevicesCallback callback) {
  GetContentClient()
      ->browser()
      ->GetHidDelegate()
      ->GetHidManager(WebContents::FromRenderFrameHost(render_frame_host()))
      ->GetDevices(base::BindOnce(&HidService::FinishGetDevices,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void HidService::RequestDevice(
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    RequestDeviceCallback callback) {
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->CanRequestDevicePermission(
          WebContents::FromRenderFrameHost(render_frame_host()), origin())) {
    std::move(callback).Run(std::vector<device::mojom::HidDeviceInfoPtr>());
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
  mojo::ReceiverId receiver_id =
      watchers_.Add(this, watcher.InitWithNewPipeAndPassReceiver());
  watcher_ids_.insert({device_guid, receiver_id});

  GetContentClient()
      ->browser()
      ->GetHidDelegate()
      ->GetHidManager(WebContents::FromRenderFrameHost(render_frame_host()))
      ->Connect(
          device_guid, std::move(client), std::move(watcher),
          base::BindOnce(&HidService::FinishConnect, weak_factory_.GetWeakPtr(),
                         std::move(callback)));
}

void HidService::OnWatcherRemoved(bool cleanup_watcher_ids) {
  if (watchers_.empty())
    DecrementActiveFrameCount();

  if (cleanup_watcher_ids) {
    // Clean up any associated |watchers_ids_| entries.
    base::EraseIf(watcher_ids_, [&](const auto& watcher_entry) {
      return watcher_entry.second == watchers_.current_receiver();
    });
  }
}

void HidService::DecrementActiveFrameCount() {
  auto* web_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host()));
  web_contents_impl->DecrementHidActiveFrameCount();
}

void HidService::OnDeviceAdded(
    const device::mojom::HidDeviceInfo& device_info) {
  if (!GetContentClient()->browser()->GetHidDelegate()->HasDevicePermission(
          WebContents::FromRenderFrameHost(render_frame_host()), origin(),
          device_info)) {
    return;
  }

  for (auto& client : clients_)
    client->DeviceAdded(device_info.Clone());
}

void HidService::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device_info) {
  if (!GetContentClient()->browser()->GetHidDelegate()->HasDevicePermission(
          WebContents::FromRenderFrameHost(render_frame_host()), origin(),
          device_info)) {
    return;
  }

  for (auto& client : clients_)
    client->DeviceRemoved(device_info.Clone());
}

void HidService::OnHidManagerConnectionError() {
  // Close the connection with Blink.
  clients_.Clear();
}

void HidService::OnPermissionRevoked(const url::Origin& requesting_origin,
                                     const url::Origin& embedding_origin) {
  if (requesting_origin_ != requesting_origin ||
      embedding_origin_ != embedding_origin) {
    return;
  }

  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host());

  base::EraseIf(watcher_ids_, [&](const auto& watcher_entry) {
    const auto* device_info =
        delegate->GetDeviceInfo(web_contents, watcher_entry.first);
    if (!device_info)
      return true;

    if (delegate->HasDevicePermission(web_contents, origin(), *device_info)) {
      return false;
    }

    watchers_.Remove(watcher_entry.second);
    return true;
  });

  // If needed decrement the active frame count.
  OnWatcherRemoved(false /* cleanup_watcher_ids */);
}

void HidService::FinishGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  std::vector<device::mojom::HidDeviceInfoPtr> result;
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  for (auto& device : devices) {
    if (delegate->HasDevicePermission(
            WebContents::FromRenderFrameHost(render_frame_host()), origin(),
            *device))
      result.push_back(std::move(device));
  }

  std::move(callback).Run(std::move(result));
}

void HidService::FinishRequestDevice(
    RequestDeviceCallback callback,
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  std::move(callback).Run(std::move(devices));
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
