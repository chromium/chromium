// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/stack_trace.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace content {

namespace {

// Removes reports from |device| if the report IDs match the IDs in the
// protected report ID lists. If all of the reports are removed from a
// collection, the collection is also removed.
void RemoveProtectedReports(device::mojom::HidDeviceInfo& device,
                            bool is_fido_allowed) {
  std::vector<device::mojom::HidCollectionInfoPtr> collections;
  for (auto& collection : device.collections) {
    const bool is_fido =
        collection->usage->usage_page == device::mojom::kPageFido;
    std::vector<device::mojom::HidReportDescriptionPtr> input_reports;
    for (auto& report : collection->input_reports) {
      if ((is_fido && is_fido_allowed) ||
          !device.protected_input_report_ids.has_value() ||
          !base::Contains(*device.protected_input_report_ids,
                          report->report_id)) {
        input_reports.push_back(std::move(report));
      }
    }
    std::vector<device::mojom::HidReportDescriptionPtr> output_reports;
    for (auto& report : collection->output_reports) {
      if ((is_fido && is_fido_allowed) ||
          !device.protected_output_report_ids.has_value() ||
          !base::Contains(*device.protected_output_report_ids,
                          report->report_id)) {
        output_reports.push_back(std::move(report));
      }
    }
    std::vector<device::mojom::HidReportDescriptionPtr> feature_reports;
    for (auto& report : collection->feature_reports) {
      if ((is_fido && is_fido_allowed) ||
          !device.protected_feature_report_ids.has_value() ||
          !base::Contains(*device.protected_feature_report_ids,
                          report->report_id)) {
        feature_reports.push_back(std::move(report));
      }
    }
    // Only keep the collection if it has at least one report.
    if (!input_reports.empty() || !output_reports.empty() ||
        !feature_reports.empty()) {
      collection->input_reports = std::move(input_reports);
      collection->output_reports = std::move(output_reports);
      collection->feature_reports = std::move(feature_reports);
      collections.push_back(std::move(collection));
    }
  }
  device.collections = std::move(collections);
}

}  // namespace

HidService::HidService(RenderFrameHost* render_frame_host,
                       mojo::PendingReceiver<blink::mojom::HidService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      origin_(render_frame_host->GetMainFrame()->GetLastCommittedOrigin()) {
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
          blink::mojom::PermissionsPolicyFeature::kHid)) {
    mojo::ReportBadMessage("Permissions policy blocks access to HID.");
    return;
  }

  // Avoid creating the HidService if there is no HID delegate to provide the
  // implementation.
  if (!GetContentClient()->browser()->GetHidDelegate())
    return;

  if (render_frame_host->IsNestedWithinFencedFrame()) {
    // The renderer is supposed to disallow the use of hid services when inside
    // a fenced frame. Anything getting past the renderer checks must be marked
    // as a bad request.
    mojo::ReportBadMessage("WebHID is not allowed in a fenced frame tree.");
    return;
  }

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
      ->GetHidManager(render_frame_host())
      ->GetDevices(base::BindOnce(&HidService::FinishGetDevices,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void HidService::RequestDevice(
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
    RequestDeviceCallback callback) {
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->CanRequestDevicePermission(render_frame_host())) {
    std::move(callback).Run(std::vector<device::mojom::HidDeviceInfoPtr>());
    return;
  }

  chooser_ = delegate->RunChooser(
      render_frame_host(), std::move(filters), std::move(exclusion_filters),
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

  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  delegate->GetHidManager(render_frame_host())
      ->Connect(
          device_guid, std::move(client), std::move(watcher),
          /*allow_protected_reports=*/false,
          delegate->IsFidoAllowedForOrigin(render_frame_host(), origin_),
          base::BindOnce(&HidService::FinishConnect, weak_factory_.GetWeakPtr(),
                         std::move(callback)));
}

void HidService::Forget(device::mojom::HidDeviceInfoPtr device_info,
                        ForgetCallback callback) {
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  delegate->RevokeDevicePermission(render_frame_host(), *device_info);
  std::move(callback).Run();
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
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->HasDevicePermission(render_frame_host(), device_info))
    return;

  auto filtered_device_info = device_info.Clone();
  RemoveProtectedReports(
      *filtered_device_info,
      delegate->IsFidoAllowedForOrigin(render_frame_host(), origin_));
  if (filtered_device_info->collections.empty())
    return;

  for (auto& client : clients_)
    client->DeviceAdded(filtered_device_info->Clone());
}

void HidService::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device_info) {
  size_t watchers_removed =
      base::EraseIf(watcher_ids_, [&](const auto& watcher_entry) {
        if (watcher_entry.first != device_info.guid)
          return false;

        watchers_.Remove(watcher_entry.second);
        return true;
      });

  // If needed, decrement the active frame count.
  if (watchers_removed > 0)
    OnWatcherRemoved(/*cleanup_watcher_ids=*/false);

  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->HasDevicePermission(render_frame_host(), device_info)) {
    return;
  }

  auto filtered_device_info = device_info.Clone();
  RemoveProtectedReports(
      *filtered_device_info,
      delegate->IsFidoAllowedForOrigin(render_frame_host(), origin_));
  if (filtered_device_info->collections.empty())
    return;

  for (auto& client : clients_)
    client->DeviceRemoved(filtered_device_info->Clone());
}

void HidService::OnDeviceChanged(
    const device::mojom::HidDeviceInfo& device_info) {
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  const bool has_device_permission =
      delegate->HasDevicePermission(render_frame_host(), device_info);

  device::mojom::HidDeviceInfoPtr filtered_device_info;
  if (has_device_permission) {
    filtered_device_info = device_info.Clone();
    RemoveProtectedReports(
        *filtered_device_info,
        delegate->IsFidoAllowedForOrigin(render_frame_host(), origin_));
  }

  if (!has_device_permission || filtered_device_info->collections.empty()) {
    // Changing the device information has caused permissions to be revoked.
    size_t watchers_removed =
        base::EraseIf(watcher_ids_, [&](const auto& watcher_entry) {
          if (watcher_entry.first != device_info.guid)
            return false;

          watchers_.Remove(watcher_entry.second);
          return true;
        });

    // If needed, decrement the active frame count.
    if (watchers_removed > 0)
      OnWatcherRemoved(/*cleanup_watcher_ids=*/false);

    return;
  }

  for (auto& client : clients_)
    client->DeviceChanged(filtered_device_info->Clone());
}

void HidService::OnHidManagerConnectionError() {
  // Close the connection with Blink.
  clients_.Clear();
}

void HidService::OnPermissionRevoked(const url::Origin& origin) {
  if (origin_ != origin) {
    return;
  }

  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  auto* rfh = render_frame_host();

  size_t watchers_removed =
      base::EraseIf(watcher_ids_, [&](const auto& watcher_entry) {
        const auto* device_info =
            delegate->GetDeviceInfo(rfh, watcher_entry.first);
        if (!device_info)
          return true;

        if (delegate->HasDevicePermission(rfh, *device_info)) {
          return false;
        }

        watchers_.Remove(watcher_entry.second);
        return true;
      });

  // If needed decrement the active frame count.
  if (watchers_removed > 0)
    OnWatcherRemoved(/*cleanup_watcher_ids=*/false);
}

void HidService::FinishGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();

  bool is_fido_allowed =
      delegate->IsFidoAllowedForOrigin(render_frame_host(), origin_);
  std::vector<device::mojom::HidDeviceInfoPtr> result;
  for (auto& device : devices) {
    RemoveProtectedReports(*device, is_fido_allowed);
    if (device->collections.empty())
      continue;

    if (delegate->HasDevicePermission(render_frame_host(), *device))
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
