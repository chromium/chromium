// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_context.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/geolocation/geolocation_impl.h"

namespace device {

GeolocationContext::GeolocationContext() = default;

GeolocationContext::~GeolocationContext() = default;

// static
void GeolocationContext::Create(
    mojo::PendingReceiver<mojom::GeolocationContext> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<GeolocationContext>(),
                              std::move(receiver));
}

void GeolocationContext::BindGeolocation(
    mojo::PendingReceiver<mojom::Geolocation> receiver) {
  GeolocationImpl* impl = new GeolocationImpl(std::move(receiver), this);
  impls_.push_back(base::WrapUnique<GeolocationImpl>(impl));
  if (geoposition_override_)
    impl->SetOverride(*geoposition_override_);
  else
    impl->StartListeningForUpdates();
}

void GeolocationContext::OnConnectionError(GeolocationImpl* impl) {
  auto it = std::find_if(impls_.begin(), impls_.end(),
                         [impl](const std::unique_ptr<GeolocationImpl>& gi) {
                           return impl == gi.get();
                         });
  DCHECK(it != impls_.end());
  impls_.erase(it);
}

void GeolocationContext::SetOverride(mojom::GeopositionPtr geoposition) {
  geoposition_override_ = std::move(geoposition);
  for (auto& impl : impls_) {
    impl->SetOverride(*geoposition_override_);
  }
}

void GeolocationContext::ClearOverride() {
  geoposition_override_.reset();
  for (auto& impl : impls_) {
    impl->ClearOverride();
  }
}

}  // namespace device
