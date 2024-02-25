// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HID_HID_SERVICE_H_
#define CONTENT_BROWSER_HID_HID_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/hid_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "url/origin.h"

namespace content {

class HidChooser;

// HidService provides an implementation of the HidService mojom interface. This
// interface is used by Blink to implement the WebHID API.
class CONTENT_EXPORT HidService : public blink::mojom::HidService,
                                  public device::mojom::HidConnectionWatcher,
                                  public HidDelegate::Observer {
 public:
  explicit HidService(RenderFrameHostImpl*);
  HidService(base::WeakPtr<ServiceWorkerVersion>, const url::Origin&);
  HidService(HidService&) = delete;
  HidService& operator=(HidService&) = delete;
  ~HidService() override;

  // Use this when creating from a document.
  static void Create(RenderFrameHostImpl*,
                     mojo::PendingReceiver<blink::mojom::HidService>);

  // Use this when creating from a service worker, which doesn't have
  // RenderFrameHost.
  static void Create(base::WeakPtr<ServiceWorkerVersion>,
                     const url::Origin&,
                     mojo::PendingReceiver<blink::mojom::HidService>);

  // Removes reports from `device` if the report IDs match the IDs in the
  // protected report ID lists. If all of the reports are removed from a
  // collection, the collection is also removed.
  static void RemoveProtectedReports(device::mojom::HidDeviceInfo& device,
                                     bool is_fido_allowed);

  // blink::mojom::HidService:
  void RegisterClient(
      mojo::PendingAssociatedRemote<device::mojom::HidManagerClient> client)
      override;
  void GetDevices(GetDevicesCallback callback) override;
  void RequestDevice(
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      RequestDeviceCallback callback) override;
  void Connect(const std::string& device_guid,
               mojo::PendingRemote<device::mojom::HidConnectionClient> client,
               ConnectCallback callback) override;
  void Forget(device::mojom::HidDeviceInfoPtr device_info,
              ForgetCallback callback) override;

  // HidDelegate::Observer:
  void OnDeviceAdded(const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnDeviceChanged(
      const device::mojom::HidDeviceInfo& device_info) override;
  void OnHidManagerConnectionError() override;
  void OnPermissionRevoked(const url::Origin& origin) override;

  const mojo::AssociatedRemoteSet<device::mojom::HidManagerClient>& clients()
      const {
    return clients_;
  }

  base::WeakPtr<content::ServiceWorkerVersion> service_worker_version() {
    return service_worker_version_;
  }

  const mojo::ReceiverSet<device::mojom::HidConnectionWatcher>&
  GetWatchersForTesting() {
    return watchers_;
  }

 private:
  HidService(RenderFrameHostImpl* render_frame_host,
             base::WeakPtr<ServiceWorkerVersion> service_worker_version,
             const url::Origin& origin);

  void OnWatcherRemoved(bool cleanup_watcher_ids, size_t watchers_removed);

  // Increment the activity reference count of the associated frame or service
  // worker.
  void IncrementActivityCount();

  // Decrement the activity reference count of the associated frame or service
  // worker.
  void DecrementActivityCount();

  void FinishGetDevices(GetDevicesCallback callback,
                        std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void FinishRequestDevice(
      RequestDeviceCallback callback,
      std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void FinishConnect(
      ConnectCallback callback,
      mojo::PendingRemote<device::mojom::HidConnection> connection);

  // Get the BrowserContext this HidService belongs to. It returns nullptr if
  // the BrowserContext is destroyed.
  BrowserContext* GetBrowserContext();

  // When RenderFrameHost pointed by |render_frame_host| is destroyed, the
  // bound HidService will be destroyed first. It should be safe to access
  // |render_frame_host_| whenever it is not null.
  const raw_ptr<RenderFrameHostImpl> render_frame_host_;

  // The ServiceWorkerVersion of the service worker this HidService belongs
  // to.
  const base::WeakPtr<content::ServiceWorkerVersion> service_worker_version_;

  // The request uuid for keeping service worker alive.
  std::optional<base::Uuid> service_worker_activity_request_uuid_;

  // The last shown HID chooser UI.
  std::unique_ptr<HidChooser> chooser_;
  url::Origin origin_;

  // Used to bind with Blink.
  mojo::AssociatedRemoteSet<device::mojom::HidManagerClient> clients_;

  // Each pipe here watches a connection created by Connect() in order to notify
  // the WebContentsImpl when an active connection indicator should be shown.
  mojo::ReceiverSet<device::mojom::HidConnectionWatcher> watchers_;

  // Maps every receiver to a guid to allow closing particular connections when
  // the user revokes a permission.
  std::multimap<std::string, mojo::ReceiverId> watcher_ids_;

  base::WeakPtrFactory<HidService> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_HID_HID_SERVICE_H_
