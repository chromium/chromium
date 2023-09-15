// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "third_party/blink/public/mojom/geolocation/geolocation_service.mojom.h"
#include "url/origin.h"

namespace blink {
namespace mojom {
enum class PermissionStatus;
}
}  // namespace blink

namespace content {
class RenderFrameHost;

class GeolocationServiceImplContext {
 public:
  GeolocationServiceImplContext();

  GeolocationServiceImplContext(const GeolocationServiceImplContext&) = delete;
  GeolocationServiceImplContext& operator=(
      const GeolocationServiceImplContext&) = delete;

  ~GeolocationServiceImplContext();
  using PermissionCallback =
      base::OnceCallback<void(blink::mojom::PermissionStatus)>;
  void RequestPermission(RenderFrameHost* render_frame_host,
                         bool user_gesture,
                         PermissionCallback callback);

 private:
  bool has_pending_permission_request_ = false;

  void HandlePermissionStatus(PermissionCallback callback,
                              blink::mojom::PermissionStatus permission_status);

  base::WeakPtrFactory<GeolocationServiceImplContext> weak_factory_{this};
};

class CONTENT_EXPORT GeolocationServiceImpl
    : public blink::mojom::GeolocationService {
 public:
  GeolocationServiceImpl(device::mojom::GeolocationContext* geolocation_context,
                         RenderFrameHost* render_frame_host);

  GeolocationServiceImpl(const GeolocationServiceImpl&) = delete;
  GeolocationServiceImpl& operator=(const GeolocationServiceImpl&) = delete;

  ~GeolocationServiceImpl() override;

  // Binds to the GeolocationService.
  void Bind(mojo::PendingReceiver<blink::mojom::GeolocationService> receiver);

  // Creates a Geolocation instance.
  // This may not be called a second time until the Geolocation instance has
  // been created.
  void CreateGeolocation(
      mojo::PendingReceiver<device::mojom::Geolocation> receiver,
      bool user_gesture,
      CreateGeolocationCallback callback) override;

  void HandlePermissionStatusChange(
      blink::mojom::PermissionStatus permission_status);

 private:
  // Creates the Geolocation Service.
  void CreateGeolocationWithPermissionStatus(
      mojo::PendingReceiver<device::mojom::Geolocation> receiver,
      CreateGeolocationCallback callback,
      blink::mojom::PermissionStatus permission_status);

  raw_ptr<device::mojom::GeolocationContext, DanglingUntriaged>
      geolocation_context_;

  // Used to subscribe to permission status changes.
  PermissionController::SubscriptionId subscription_id_;

  // Tracks the origin for which a granted permission is being observed. Used to
  // terminate access upon permission revocation.
  url::Origin requesting_origin_;

  // Note: |render_frame_host_| owns |this| instance.
  const raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_;

  // Along with each GeolocationService, we store a
  // GeolocationServiceImplContext which primarily exists to manage a
  // Permission Request ID.
  mojo::ReceiverSet<blink::mojom::GeolocationService,
                    std::unique_ptr<GeolocationServiceImplContext>>
      receiver_set_;

  base::WeakPtrFactory<GeolocationServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_SERVICE_IMPL_H_
