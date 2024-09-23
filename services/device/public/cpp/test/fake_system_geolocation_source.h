// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SYSTEM_GEOLOCATION_SOURCE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SYSTEM_GEOLOCATION_SOURCE_H_

#include "build/build_config.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source.h"

namespace device {
class FakeSystemGeolocationSource : public SystemGeolocationSource {
 public:
  FakeSystemGeolocationSource();
  FakeSystemGeolocationSource(const FakeSystemGeolocationSource&) = delete;
  FakeSystemGeolocationSource& operator=(const FakeSystemGeolocationSource&) =
      delete;
  ~FakeSystemGeolocationSource() override;

  void RegisterPermissionUpdateCallback(
      PermissionUpdateCallback callback) override;
  void SetSystemPermission(LocationSystemPermissionStatus status);

#if BUILDFLAG(IS_APPLE)
  void StartWatchingPosition(bool high_accuracy) override;
  void StopWatchingPosition() override;
  bool watching_position() { return watching_position_; }

  void AddPositionUpdateObserver(PositionObserver* observer) override;
  void RemovePositionUpdateObserver(PositionObserver* observer) override;
  void FakePositionUpdatedForTesting(const mojom::Geoposition& position);
  void FakePositionErrorForTesting(const mojom::GeopositionError& error);
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  void RequestPermission() override {}
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)

 private:
  LocationSystemPermissionStatus status_ =
      LocationSystemPermissionStatus::kDenied;
  PermissionUpdateCallback permission_callback_;

#if BUILDFLAG(IS_APPLE)
  bool watching_position_ = false;
  scoped_refptr<PositionObserverList> position_observers_ =
      base::MakeRefCounted<PositionObserverList>();
#endif  // BUILDFLAG(IS_APPLE)
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SYSTEM_GEOLOCATION_SOURCE_H_
