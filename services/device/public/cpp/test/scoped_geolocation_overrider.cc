// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/device_service.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"

namespace device {

// This class is a fake implementation of GeolocationContext and Geolocation
// mojo interfaces for those tests which want to set an override geoposition
// value and verify their code where there are geolocation mojo calls.
class ScopedGeolocationOverrider::FakeGeolocationContext
    : public mojom::GeolocationContext {
 public:
  explicit FakeGeolocationContext(mojom::GeopositionPtr position);
  ~FakeGeolocationContext() override;

  void UpdateLocation(mojom::GeopositionPtr position);
  const mojom::Geoposition* GetGeoposition() const;

  void Pause();
  void Resume();

  size_t GetGeolocationInstanceCount() const;

  void BindForOverrideService(
      mojo::PendingReceiver<mojom::GeolocationContext> receiver);
  void OnDisconnect(FakeGeolocation* impl);

  // mojom::GeolocationContext implementation:
  void BindGeolocation(mojo::PendingReceiver<mojom::Geolocation> receiver,
                       const GURL& requesting_url) override;
  void SetOverride(mojom::GeopositionPtr geoposition) override;
  void ClearOverride() override;

  bool is_paused() const { return is_paused_; }
  void set_close_callback(base::RepeatingClosure callback) {
    close_callback_ = std::move(callback);
  }

 private:
  mojom::GeopositionPtr position_;
  // |override_position_| enables overriding the override set by this class, as
  // required by the mojom::GeolocationContext interface.
  mojom::GeopositionPtr override_position_;
  std::set<std::unique_ptr<FakeGeolocation>, base::UniquePtrComparator> impls_;
  mojo::ReceiverSet<mojom::GeolocationContext> context_receivers_;
  bool is_paused_ = false;
  base::RepeatingClosure close_callback_;
};

class ScopedGeolocationOverrider::FakeGeolocation : public mojom::Geolocation {
 public:
  FakeGeolocation(mojo::PendingReceiver<mojom::Geolocation> receiver,
                  FakeGeolocationContext* context);
  ~FakeGeolocation() override;

  void OnDisconnect();
  void OnResume();

  void UpdateLocation();

  // mojom::Geolocation implementation:
  void QueryNextPosition(QueryNextPositionCallback callback) override;
  void SetHighAccuracy(bool high_accuracy) override;

 private:
  void RunPositionCallbackIfNeeded();

  raw_ptr<FakeGeolocationContext> context_;
  bool needs_update_ = true;
  QueryNextPositionCallback position_callback_;
  mojo::Receiver<mojom::Geolocation> receiver_{this};
};

ScopedGeolocationOverrider::ScopedGeolocationOverrider(
    mojom::GeopositionPtr position) {
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

  OverrideGeolocation(std::move(position));
}

ScopedGeolocationOverrider::~ScopedGeolocationOverrider() {
  DeviceService::OverrideGeolocationContextBinderForTesting(
      base::NullCallback());
}

void ScopedGeolocationOverrider::OverrideGeolocation(
    mojom::GeopositionPtr position) {
  geolocation_context_ =
      std::make_unique<FakeGeolocationContext>(std::move(position));
  DeviceService::OverrideGeolocationContextBinderForTesting(
      base::BindRepeating(&FakeGeolocationContext::BindForOverrideService,
                          base::Unretained(geolocation_context_.get())));
}

void ScopedGeolocationOverrider::UpdateLocation(
    mojom::GeopositionPtr position) {
  geolocation_context_->UpdateLocation(std::move(position));
}

void ScopedGeolocationOverrider::UpdateLocation(double latitude,
                                                double longitude) {
  auto position = mojom::Geoposition::New();
  position->latitude = latitude;
  position->longitude = longitude;
  position->altitude = 0.;
  position->accuracy = 0.;
  position->timestamp = base::Time::Now();

  UpdateLocation(std::move(position));
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
    mojom::GeopositionPtr position)
    : position_(std::move(position)) {
  if (position_) {
    position_->valid = false;
    if (ValidateGeoposition(*position_))
      position_->valid = true;
  }
}

ScopedGeolocationOverrider::FakeGeolocationContext::~FakeGeolocationContext() {}

void ScopedGeolocationOverrider::FakeGeolocationContext::UpdateLocation(
    mojom::GeopositionPtr position) {
  position_ = std::move(position);

  if (!position_)
    return;

  position_->valid = false;
  if (ValidateGeoposition(*position_))
    position_->valid = true;

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

const mojom::Geoposition*
ScopedGeolocationOverrider::FakeGeolocationContext::GetGeoposition() const {
  if (override_position_)
    return override_position_.get();

  return position_.get();
}

void ScopedGeolocationOverrider::FakeGeolocationContext::BindForOverrideService(
    mojo::PendingReceiver<mojom::GeolocationContext> receiver) {
  context_receivers_.Add(this, std::move(receiver));
}

void ScopedGeolocationOverrider::FakeGeolocationContext::BindGeolocation(
    mojo::PendingReceiver<mojom::Geolocation> receiver,
    const GURL& requesting_origin) {
  impls_.insert(std::make_unique<FakeGeolocation>(std::move(receiver), this));
}

void ScopedGeolocationOverrider::FakeGeolocationContext::SetOverride(
    mojom::GeopositionPtr geoposition) {
  override_position_ = std::move(geoposition);
  if (override_position_.is_null())
    return;

  override_position_->valid = false;
  if (ValidateGeoposition(*override_position_))
    override_position_->valid = true;

  for (auto& impl : impls_) {
    impl->UpdateLocation();
  }
}

void ScopedGeolocationOverrider::FakeGeolocationContext::ClearOverride() {
  override_position_.reset();
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
    FakeGeolocationContext* context)
    : context_(context) {
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

  const mojom::Geoposition* position = context_->GetGeoposition();
  if (!position)
    return;

  std::move(position_callback_).Run(position->Clone());
  needs_update_ = false;
}

void ScopedGeolocationOverrider::FakeGeolocation::UpdateLocation() {
  // Needs update for new position.
  needs_update_ = true;

  RunPositionCallbackIfNeeded();
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
