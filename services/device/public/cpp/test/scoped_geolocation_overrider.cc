// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/device_service.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "url/origin.h"

namespace device {

// This class is a fake implementation of GeolocationContext and Geolocation
// mojo interfaces for those tests which want to set an override geoposition
// value and verify their code where there are geolocation mojo calls.
class ScopedGeolocationOverrider::FakeGeolocationContext
    : public mojom::GeolocationContext {
 public:
  explicit FakeGeolocationContext(mojom::GeopositionResultPtr result);
  ~FakeGeolocationContext() override;

  void UpdateLocation(mojom::GeopositionResultPtr result);
  const mojom::GeopositionResult* GetGeoposition() const;

  void Pause();
  void Resume();

  size_t GetGeolocationInstanceCount() const;

  void BindForOverrideService(
      mojo::PendingReceiver<mojom::GeolocationContext> receiver);
  void OnDisconnect(FakeGeolocation* impl);

  // mojom::GeolocationContext implementation:
  void BindGeolocation(mojo::PendingReceiver<mojom::Geolocation> receiver,
                       const GURL& requesting_url,
                       mojom::GeolocationClientId client_id) override;
  void OnPermissionRevoked(const url::Origin& origin) override;

  void SetOverride(mojom::GeopositionResultPtr result) override;
  void ClearOverride() override;

  bool is_paused() const { return is_paused_; }
  void set_close_callback(base::RepeatingClosure callback) {
    close_callback_ = std::move(callback);
  }

 private:
  mojom::GeopositionResultPtr result_;
  // |override_result_| enables overriding the override set by this class, as
  // required by the mojom::GeolocationContext interface.
  mojom::GeopositionResultPtr override_result_;
  std::set<std::unique_ptr<FakeGeolocation>, base::UniquePtrComparator> impls_;
  mojo::ReceiverSet<mojom::GeolocationContext> context_receivers_;
  bool is_paused_ = false;
  base::RepeatingClosure close_callback_;
};

class ScopedGeolocationOverrider::FakeGeolocation : public mojom::Geolocation {
 public:
  FakeGeolocation(mojo::PendingReceiver<mojom::Geolocation> receiver,
                  const GURL& requesting_url,
                  FakeGeolocationContext* context);
  ~FakeGeolocation() override;

  void OnDisconnect();
  void OnResume();

  void UpdateLocation();
  void OnPermissionRevoked();

  // mojom::Geolocation implementation:
  void QueryNextPosition(QueryNextPositionCallback callback) override;
  void SetHighAccuracy(bool high_accuracy) override;
  const GURL& url() { return url_; }

 private:
  void RunPositionCallbackIfNeeded();

  const GURL url_;
  raw_ptr<FakeGeolocationContext> context_;
  bool needs_update_ = true;
  QueryNextPositionCallback position_callback_;
  mojo::Receiver<mojom::Geolocation> receiver_{this};
};

ScopedGeolocationOverrider::ScopedGeolocationOverrider(
    mojom::GeopositionResultPtr position) {
  OverrideGeolocation(std::move(position));
}

ScopedGeolocationOverrider::ScopedGeolocationOverrider(double latitude,
                                                       double longitude) {
  auto position = mojom::Geoposition::New();
  position->latitude = latitude;
  position->longitude = longitude;
  position->altitude = 0.;
  position->accuracy = 0.;
  position->timestamp = base::Time::Now();
  OverrideGeolocation(
      mojom::GeopositionResult::NewPosition(std::move(position)));
}

ScopedGeolocationOverrider::~ScopedGeolocationOverrider() {
  DeviceService::OverrideGeolocationContextBinderForTesting(
      base::NullCallback());
}

void ScopedGeolocationOverrider::OverrideGeolocation(
    mojom::GeopositionResultPtr result) {
  geolocation_context_ =
      std::make_unique<FakeGeolocationContext>(std::move(result));
  DeviceService::OverrideGeolocationContextBinderForTesting(
      base::BindRepeating(&FakeGeolocationContext::BindForOverrideService,
                          base::Unretained(geolocation_context_.get())));
}

void ScopedGeolocationOverrider::UpdateLocation(
    mojom::GeopositionResultPtr result) {
  geolocation_context_->UpdateLocation(std::move(result));
}

void ScopedGeolocationOverrider::UpdateLocation(double latitude,
                                                double longitude) {
  auto position = mojom::Geoposition::New();
  position->latitude = latitude;
  position->longitude = longitude;
  position->altitude = 0.;
  position->accuracy = 0.;
  position->timestamp = base::Time::Now();
  UpdateLocation(mojom::GeopositionResult::NewPosition(std::move(position)));
}

void ScopedGeolocationOverrider::Pause() {
  geolocation_context_->Pause();
}

void ScopedGeolocationOverrider::Resume() {
  geolocation_context_->Resume();
}

size_t ScopedGeolocationOverrider::GetGeolocationInstanceCount() const {
  return geolocation_context_->GetGeolocationInstanceCount();
}

void ScopedGeolocationOverrider::SetGeolocationCloseCallback(
    base::RepeatingClosure closure) {
  geolocation_context_->set_close_callback(std::move(closure));
}

ScopedGeolocationOverrider::FakeGeolocationContext::FakeGeolocationContext(
    mojom::GeopositionResultPtr result)
    : result_(std::move(result)) {}

ScopedGeolocationOverrider::FakeGeolocationContext::~FakeGeolocationContext() {}

void ScopedGeolocationOverrider::FakeGeolocationContext::UpdateLocation(
    mojom::GeopositionResultPtr result) {
  result_ = std::move(result);

  if (!result_) {
    return;
  }

  for (auto& impl : impls_) {
    impl->UpdateLocation();
  }
}

void ScopedGeolocationOverrider::FakeGeolocationContext::OnDisconnect(
    FakeGeolocation* impl) {
  // Note: We can't use set::erase() here, since FakeGeolocation* is not
  //       the impls_::key_type.
  auto it = impls_.find(impl);
  impls_.erase(it);

  if (!close_callback_.is_null())
    close_callback_.Run();
}

const mojom::GeopositionResult*
ScopedGeolocationOverrider::FakeGeolocationContext::GetGeoposition() const {
  if (override_result_) {
    return override_result_.get();
  }

  return result_.get();
}

void ScopedGeolocationOverrider::FakeGeolocationContext::BindForOverrideService(
    mojo::PendingReceiver<mojom::GeolocationContext> receiver) {
  context_receivers_.Add(this, std::move(receiver));
}

void ScopedGeolocationOverrider::FakeGeolocationContext::BindGeolocation(
    mojo::PendingReceiver<mojom::Geolocation> receiver,
    const GURL& requesting_origin,
    mojom::GeolocationClientId client_id) {
  impls_.insert(std::make_unique<FakeGeolocation>(std::move(receiver),
                                                  requesting_origin, this));
}

void ScopedGeolocationOverrider::FakeGeolocationContext::OnPermissionRevoked(
    const url::Origin& origin) {
  std::erase_if(impls_, [&origin](const auto& impl) {
    if (!origin.IsSameOriginWith(impl->url())) {
      return false;
    }
    // Invoke the position callback with kPermissionDenied before removing.
    impl->OnPermissionRevoked();
    return true;
  });
}

void ScopedGeolocationOverrider::FakeGeolocationContext::SetOverride(
    mojom::GeopositionResultPtr result) {
  override_result_ = std::move(result);
  if (override_result_.is_null()) {
    return;
  }

  for (auto& impl : impls_) {
    impl->UpdateLocation();
  }
}

void ScopedGeolocationOverrider::FakeGeolocationContext::ClearOverride() {
  override_result_.reset();
}

void ScopedGeolocationOverrider::FakeGeolocationContext::Pause() {
  is_paused_ = true;
}

void ScopedGeolocationOverrider::FakeGeolocationContext::Resume() {
  is_paused_ = false;
  for (auto& impl : impls_) {
    impl->OnResume();
  }
}

size_t ScopedGeolocationOverrider::FakeGeolocationContext::
    GetGeolocationInstanceCount() const {
  return impls_.size();
}

ScopedGeolocationOverrider::FakeGeolocation::FakeGeolocation(
    mojo::PendingReceiver<mojom::Geolocation> receiver,
    const GURL& requesting_url,
    FakeGeolocationContext* context)
    : url_(requesting_url), context_(context) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&ScopedGeolocationOverrider::FakeGeolocation::OnDisconnect,
                     base::Unretained(this)));
}

ScopedGeolocationOverrider::FakeGeolocation::~FakeGeolocation() {}

void ScopedGeolocationOverrider::FakeGeolocation::OnDisconnect() {
  context_->OnDisconnect(this);
}

void ScopedGeolocationOverrider::FakeGeolocation::OnResume() {
  DCHECK(!context_->is_paused());
  RunPositionCallbackIfNeeded();
}

void ScopedGeolocationOverrider::FakeGeolocation::
    RunPositionCallbackIfNeeded() {
  // No need to run position callback if paused or no new position pending.
  if (context_->is_paused() || !needs_update_)
    return;

  if (position_callback_.is_null())
    return;

  const mojom::GeopositionResult* result = context_->GetGeoposition();
  if (!result) {
    return;
  }

  std::move(position_callback_).Run(result->Clone());
  needs_update_ = false;
}

void ScopedGeolocationOverrider::FakeGeolocation::UpdateLocation() {
  // Needs update for new position.
  needs_update_ = true;

  RunPositionCallbackIfNeeded();
}

void ScopedGeolocationOverrider::FakeGeolocation::OnPermissionRevoked() {
  if (!position_callback_.is_null()) {
    std::move(position_callback_)
        .Run(mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
            mojom::GeopositionErrorCode::kPermissionDenied,
            /*error_message=*/"", /*error_technical=*/"")));
  }
}

void ScopedGeolocationOverrider::FakeGeolocation::QueryNextPosition(
    QueryNextPositionCallback callback) {
  // Pending callbacks might be overrided.
  position_callback_ = std::move(callback);

  RunPositionCallbackIfNeeded();
}

void ScopedGeolocationOverrider::FakeGeolocation::SetHighAccuracy(
    bool high_accuracy) {}

}  // namespace device
