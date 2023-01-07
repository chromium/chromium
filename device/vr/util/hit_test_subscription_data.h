// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_UTIL_HIT_TEST_SUBSCRIPTION_DATA_H_
#define DEVICE_VR_UTIL_HIT_TEST_SUBSCRIPTION_DATA_H_

#include "base/component_export.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

using HitTestSubscriptionId = base::IdTypeU64<class HitTestSubscriptionTag>;

struct COMPONENT_EXPORT(DEVICE_VR_UTIL) HitTestSubscriptionData {
  mojom::XRNativeOriginInformationPtr native_origin_information;
  const std::vector<mojom::EntityTypeForHitTest> entity_types;
  mojom::XRRayPtr ray;

  HitTestSubscriptionData(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);
  HitTestSubscriptionData(HitTestSubscriptionData&& other);
  ~HitTestSubscriptionData();
};

struct COMPONENT_EXPORT(DEVICE_VR_UTIL) TransientInputHitTestSubscriptionData {
  const std::string profile_name;
  const std::vector<mojom::EntityTypeForHitTest> entity_types;
  mojom::XRRayPtr ray;

  TransientInputHitTestSubscriptionData(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);
  TransientInputHitTestSubscriptionData(
      TransientInputHitTestSubscriptionData&& other);
  ~TransientInputHitTestSubscriptionData();
};

}  // namespace device

#endif  // DEVICE_VR_UTIL_HIT_TEST_SUBSCRIPTION_DATA_H_
