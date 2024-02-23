// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hid/hid_service.h"

#include <map>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/service_worker/service_worker_hid_delegate_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace content {

// Deletes the HidService when the connected document is destroyed.
class DocumentHelper
    : public content::DocumentService<blink::mojom::HidService> {
 public:
  DocumentHelper(std::unique_ptr<HidService> parent,
                 RenderFrameHost& render_frame_host,
                 mojo::PendingReceiver<blink::mojom::HidService> receiver)
      : DocumentService(render_frame_host, std::move(receiver)),
        parent_(std::move(parent)) {
    DCHECK(parent_);
  }
  ~DocumentHelper() override = default;

  // blink::mojom::HidService:
  void RegisterClient(
      mojo::PendingAssociatedRemote<device::mojom::HidManagerClient> client)
      override {
    parent_->RegisterClient(std::move(client));
  }
  void GetDevices(GetDevicesCallback callback) override {
    parent_->GetDevices(std::move(callback));
  }
  void RequestDevice(
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      RequestDeviceCallback callback) override {
    parent_->RequestDevice(std::move(filters), std::move(exclusion_filters),
                           std::move(callback));
  }
  void Connect(const std::string& device_guid,
               mojo::PendingRemote<device::mojom::HidConnectionClient> client,
               ConnectCallback callback) override {
    parent_->Connect(device_guid, std::move(client), std::move(callback));
  }
  void Forget(device::mojom::HidDeviceInfoPtr device_info,
              ForgetCallback callback) override {
    parent_->Forget(std::move(device_info), std::move(callback));
  }

 private:
  const std::unique_ptr<HidService> parent_;
};

HidService::HidService(
    RenderFrameHostImpl* render_frame_host,
    base::WeakPtr<ServiceWorkerVersion> service_worker_version,
    const url::Origin& origin)
    : render_frame_host_(render_frame_host),
      service_worker_version_(std::move(service_worker_version)),
      origin_(origin) {
  watchers_.set_disconnect_handler(base::BindRepeating(
      &HidService::OnWatcherRemoved, base::Unretained(this),
      /* cleanup_watcher_ids=*/true, /*watchers_removed=*/1));

  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (delegate && render_frame_host_) {
    delegate->AddObserver(GetBrowserContext(), this);
  } else if (service_worker_version_) {
    // For service worker case, it relies on ServiceWorkerHidDelegateObserver to
    // be the broker between HidDelegate and HidService.
    auto context = service_worker_version_->context();
    if (context) {
      context->hid_delegate_observer()->RegisterHidService(
          service_worker_version_->registration_id(),
          weak_factory_.GetWeakPtr());
    }
  }
}

HidService::HidService(RenderFrameHostImpl* render_frame_host)
    : HidService(render_frame_host,
                 /*service_worker_version=*/nullptr,
                 render_frame_host->GetMainFrame()->GetLastCommittedOrigin()) {}

HidService::HidService(
    base::WeakPtr<ServiceWorkerVersion> service_worker_version,
    const url::Origin& origin)
    : HidService(/*render_frame_host=*/nullptr,
                 std::move(service_worker_version),
                 origin) {}

HidService::~HidService() {
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (delegate && render_frame_host_) {
    delegate->RemoveObserver(GetBrowserContext(), this);
  }

  // Update connection count and active frame count tracking as remaining
  // watchers will be closed from this end.
  if (!watchers_.empty())
    DecrementActivityCount();
  for (size_t i = 0; i < watchers_.size(); i++) {
    delegate->DecrementConnectionCount(GetBrowserContext(), origin_);
  }
}

// static
void HidService::Create(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  CHECK(render_frame_host);

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

  if (render_frame_host->GetOutermostMainFrame()
          ->GetLastCommittedOrigin()
          .opaque()) {
    mojo::ReportBadMessage("WebHID is not allowed from an opaque origin.");
    return;
  }

  // DocumentHelper observes the lifetime of the document connected to
  // `render_frame_host` and destroys the HidService when the Mojo connection is
  // disconnected, RenderFrameHost is deleted, or the RenderFrameHost commits a
  // cross-document navigation. It forwards its Mojo interface to HidService.
  new DocumentHelper(std::make_unique<HidService>(render_frame_host),
                     *render_frame_host, std::move(receiver));
}

// static
void HidService::Create(
    base::WeakPtr<ServiceWorkerVersion> service_worker_version,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  DCHECK(service_worker_version);

  if (origin.opaque()) {
    // Service worker should not be available to a window/worker client which
    // origin is opaque according to Service Worker specification.
    mojo::ReportBadMessage("WebHID is blocked in an opaque origin.");
    return;
  }

  // Avoid creating the HidService if there is no HID delegate to provide
  // the implementation.
  if (!GetContentClient()->browser()->GetHidDelegate())
    return;

  // This makes HidService a self-owned receiver so it will self-destruct when a
  // mojo interface error occurs.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<HidService>(std::move(service_worker_version), origin),
      std::move(receiver));
}

// static
void HidService::RemoveProtectedReports(device::mojom::HidDeviceInfo& device,
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

void HidService::RegisterClient(
    mojo::PendingAssociatedRemote<device::mojom::HidManagerClient> client) {
  clients_.Add(std::move(client));
  if (service_worker_version_ && service_worker_version_->context()) {
    // HidService is expected to have only one HidManagerClient when it is for a
    // service worker. One renderer side of a service worker has its own
    // associated HidService.
    CHECK_EQ(1u, clients_.size());
    // When a service worker is woken up by a device connection event, the
    // client might not have yet registered with the HidService or the
    // HidService hasn't been created yet when service worker is in running
    // state. This is because service worker is set to running state after
    // script evaluation but inter-processes request triggered from the script
    // evaluation that creates HidService or registers a client might not be
    // done in the browser process. To handle this situation, pending callbacks
    // are stored and to be processed when registering the client.
    service_worker_version_->context()
        ->hid_delegate_observer()
        ->ProcessPendingCallbacks(service_worker_version_.get());
  }
}

void HidService::GetDevices(GetDevicesCallback callback) {
  auto* browser_context = GetBrowserContext();

  if (!browser_context) {
    std::move(callback).Run({});
    return;
  }
  GetContentClient()
      ->browser()
      ->GetHidDelegate()
      ->GetHidManager(browser_context)
      ->GetDevices(base::BindOnce(&HidService::FinishGetDevices,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void HidService::RequestDevice(
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
    RequestDeviceCallback callback) {
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!render_frame_host_ ||
      !delegate->CanRequestDevicePermission(GetBrowserContext(), origin_)) {
    std::move(callback).Run(std::vector<device::mojom::HidDeviceInfoPtr>());
    return;
  }
  chooser_ = GetContentClient()->browser()->GetHidDelegate()->RunChooser(
      render_frame_host_, std::move(filters), std::move(exclusion_filters),
      base::BindOnce(&HidService::FinishRequestDevice,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HidService::Connect(
    const std::string& device_guid,
    mojo::PendingRemote<device::mojom::HidConnectionClient> client,
    ConnectCallback callback) {
  auto* browser_context = GetBrowserContext();

  if (!browser_context) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  auto* device_info = delegate->GetDeviceInfo(browser_context, device_guid);
  if (!device_info ||
      !delegate->HasDevicePermission(browser_context, render_frame_host_,
                                     origin_, *device_info)) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  if (watchers_.empty()) {
    IncrementActivityCount();
  }

  delegate->IncrementConnectionCount(browser_context, origin_);

  mojo::PendingRemote<device::mojom::HidConnectionWatcher> watcher;
  mojo::ReceiverId receiver_id =
      watchers_.Add(this, watcher.InitWithNewPipeAndPassReceiver());
  watcher_ids_.insert({device_guid, receiver_id});

  delegate->GetHidManager(browser_context)
      ->Connect(
          device_guid, std::move(client), std::move(watcher),
          /*allow_protected_reports=*/false,
          delegate->IsFidoAllowedForOrigin(browser_context, origin_),
          base::BindOnce(&HidService::FinishConnect, weak_factory_.GetWeakPtr(),
                         std::move(callback)));
}

void HidService::Forget(device::mojom::HidDeviceInfoPtr device_info,
                        ForgetCallback callback) {
  auto* browser_context = GetBrowserContext();

  if (browser_context) {
    GetContentClient()->browser()->GetHidDelegate()->RevokeDevicePermission(
        browser_context, render_frame_host_, origin_, *device_info);
  }
  std::move(callback).Run();
}

void HidService::OnWatcherRemoved(bool cleanup_watcher_ids,
                                  size_t watchers_removed) {
  if (watchers_.empty())
    DecrementActivityCount();

  // When |cleanup_watcher_ids| is true, it is the case like watcher disconnect
  // handler where the entry in |watchers_| is removed but |watcher_ids_| isn't
  // yet, so the entry in |watcher_ids_| needs to be removed.
  if (cleanup_watcher_ids) {
    // Clean up any associated |watchers_ids_| entries.
    std::erase_if(watcher_ids_, [&](const auto& watcher_entry) {
      return watcher_entry.second == watchers_.current_receiver();
    });
  }

  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  for (size_t i = 0; i < watchers_removed; i++) {
    delegate->DecrementConnectionCount(GetBrowserContext(), origin_);
  }
}

void HidService::IncrementActivityCount() {
  if (render_frame_host_) {
    auto* web_contents_impl =
        WebContentsImpl::FromRenderFrameHostImpl(render_frame_host_);
    web_contents_impl->IncrementHidActiveFrameCount();
  } else if (service_worker_version_) {
    CHECK(!service_worker_activity_request_uuid_);
    service_worker_activity_request_uuid_ = base::Uuid::GenerateRandomV4();
    service_worker_version_->StartExternalRequest(
        *service_worker_activity_request_uuid_,
        ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout);
  }
}

void HidService::DecrementActivityCount() {
  if (render_frame_host_) {
    auto* web_contents_impl =
        WebContentsImpl::FromRenderFrameHostImpl(render_frame_host_);
    web_contents_impl->DecrementHidActiveFrameCount();
  } else if (service_worker_version_) {
    CHECK(service_worker_activity_request_uuid_);
    service_worker_version_->FinishExternalRequest(
        *service_worker_activity_request_uuid_);
    service_worker_activity_request_uuid_.reset();
  }
}

void HidService::OnDeviceAdded(
    const device::mojom::HidDeviceInfo& device_info) {
  auto* browser_context = GetBrowserContext();
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->HasDevicePermission(browser_context, render_frame_host_,
                                     origin_, device_info)) {
    return;
  }

  auto filtered_device_info = device_info.Clone();
  RemoveProtectedReports(
      *filtered_device_info,
      delegate->IsFidoAllowedForOrigin(browser_context, origin_));
  if (filtered_device_info->collections.empty())
    return;

  for (auto& client : clients_)
    client->DeviceAdded(filtered_device_info->Clone());
}

void HidService::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device_info) {
  size_t watchers_removed =
      std::erase_if(watcher_ids_, [&](const auto& watcher_entry) {
        if (watcher_entry.first != device_info.guid)
          return false;

        watchers_.Remove(watcher_entry.second);
        return true;
      });

  // If needed, decrement the active frame count.
  if (watchers_removed > 0)
    OnWatcherRemoved(/*cleanup_watcher_ids=*/false, watchers_removed);

  auto* browser_context = GetBrowserContext();
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  if (!delegate->HasDevicePermission(browser_context, render_frame_host_,
                                     origin_, device_info)) {
    return;
  }

  auto filtered_device_info = device_info.Clone();
  RemoveProtectedReports(
      *filtered_device_info,
      delegate->IsFidoAllowedForOrigin(browser_context, origin_));
  if (filtered_device_info->collections.empty())
    return;

  for (auto& client : clients_)
    client->DeviceRemoved(filtered_device_info->Clone());
}

void HidService::OnDeviceChanged(
    const device::mojom::HidDeviceInfo& device_info) {
  auto* browser_context = GetBrowserContext();
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();
  const bool has_device_permission = delegate->HasDevicePermission(
      browser_context, render_frame_host_, origin_, device_info);

  device::mojom::HidDeviceInfoPtr filtered_device_info;
  if (has_device_permission) {
    filtered_device_info = device_info.Clone();
    RemoveProtectedReports(
        *filtered_device_info,
        delegate->IsFidoAllowedForOrigin(browser_context, origin_));
  }

  if (!has_device_permission || filtered_device_info->collections.empty()) {
    // Changing the device information has caused permissions to be revoked.
    size_t watchers_removed =
        std::erase_if(watcher_ids_, [&](const auto& watcher_entry) {
          if (watcher_entry.first != device_info.guid)
            return false;

          watchers_.Remove(watcher_entry.second);
          return true;
        });

    // If needed, decrement the active frame count.
    if (watchers_removed > 0)
      OnWatcherRemoved(/*cleanup_watcher_ids=*/false, watchers_removed);

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

  auto* browser_context = GetBrowserContext();
  HidDelegate* delegate = GetContentClient()->browser()->GetHidDelegate();

  size_t watchers_removed =
      std::erase_if(watcher_ids_, [&](const auto& watcher_entry) {
        const auto* device_info =
            delegate->GetDeviceInfo(browser_context, watcher_entry.first);
        if (!device_info)
          return true;

        if (delegate->HasDevicePermission(browser_context, render_frame_host_,
                                          origin_, *device_info)) {
          return false;
        }

        watchers_.Remove(watcher_entry.second);
        return true;
      });

  // If needed decrement the active frame count.
  if (watchers_removed > 0)
    OnWatcherRemoved(/*cleanup_watcher_ids=*/false, watchers_removed);
}

void HidService::FinishGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  auto* browser_context = GetBrowserContext();
  auto* delegate = GetContentClient()->browser()->GetHidDelegate();

  bool is_fido_allowed =
      delegate->IsFidoAllowedForOrigin(browser_context, origin_);
  std::vector<device::mojom::HidDeviceInfoPtr> result;
  for (auto& device : devices) {
    RemoveProtectedReports(*device, is_fido_allowed);
    if (device->collections.empty())
      continue;

    if (delegate->HasDevicePermission(browser_context, render_frame_host_,
                                      origin_, *device)) {
      result.push_back(std::move(device));
    }
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

BrowserContext* HidService::GetBrowserContext() {
  if (render_frame_host_) {
    return render_frame_host_->GetBrowserContext();
  }
  if (service_worker_version_ && service_worker_version_->context()) {
    return service_worker_version_->context()->wrapper()->browser_context();
  }
  return nullptr;
}

}  // namespace content
