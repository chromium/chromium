// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HID_HID_SERVICE_H_
#define CONTENT_BROWSER_HID_HID_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
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
  HidService(HidService&) = delete;
  HidService& operator=(HidService&) = delete;
  ~HidService() override;

  // Use this when creating from a document.
  static void Create(RenderFrameHostImpl*,
                     mojo::PendingReceiver<blink::mojom::HidService>);

  // Use this when creating from a service worker, which doesn't have
  // RenderFrameHost.
  static void Create(BrowserContext*,
                     const url::Origin&,
                     mojo::PendingReceiver<blink::mojom::HidService>);

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

 private:
  HidService(BrowserContext*,
             const url::Origin&,
             RenderFrameHostImpl*,
             mojo::PendingReceiver<blink::mojom::HidService>);

  void OnWatcherRemoved(bool cleanup_watcher_ids);
  void OnServiceDisconnected();
  void IncrementActiveFrameCount();
  void DecrementActiveFrameCount();

  void FinishGetDevices(GetDevicesCallback callback,
                        std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void FinishRequestDevice(
      RequestDeviceCallback callback,
      std::vector<device::mojom::HidDeviceInfoPtr> devices);
  void FinishConnect(
      ConnectCallback callback,
      mojo::PendingRemote<device::mojom::HidConnection> connection);

  // The browser_context pointed by |browser_context_| always outlives
  // HidService itself.
  const raw_ptr<BrowserContext> browser_context_;

  // When render_frame_host pointed by |render_frame_host| destroys, the bound
  // HidService will be destroyed first. It should be safe to access
  // |render_frame_host_| whenever it is not null.
  const raw_ptr<RenderFrameHostImpl> render_frame_host_;

  // When created from a document, `receiver_` is not bound. Instead, the
  // receiver is transferred to a DocumentService which manages the Mojo
  // connection and observes the document lifecycle. The DocumentService ensures
  // HidService is destroyed when the Mojo connection is disconnected,
  // renderFrameHost is deleted, or the RenderFrameHost commits a cross-document
  // navigation. The DocumentService forwards its Mojo interface to HidService.
  //
  // When created from a service worker, `receiver_` is bound and no
  // DocumentService is created. HidService self-destructs when the Mojo
  // connection is disconnected.
  mojo::Receiver<blink::mojom::HidService> receiver_{this};

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
