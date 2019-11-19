// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROVIDER_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROVIDER_PROXY_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor.mojom-blink-forward.h"
#include "services/device/public/mojom/sensor_provider.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class SensorProxy;

// This class wraps 'SensorProvider' mojo interface and it manages
// 'SensorProxy' instances.
class MODULES_EXPORT SensorProviderProxy final
    : public GarbageCollected<SensorProviderProxy>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(SensorProviderProxy);

 public:
  static const char kSupplementName[];

  static SensorProviderProxy* From(Document*);

  explicit SensorProviderProxy(Document&);
  ~SensorProviderProxy();

  SensorProxy* CreateSensorProxy(device::mojom::blink::SensorType, Page*);
  SensorProxy* GetSensorProxy(device::mojom::blink::SensorType);

  void set_inspector_mode(bool flag) { inspector_mode_ = flag; }
  bool inspector_mode() const { return inspector_mode_; }

  void Trace(blink::Visitor*) override;

 private:
  friend class SensorProxy;

  // For SensorProviderProxy friends' use.
  device::mojom::blink::SensorProvider* sensor_provider() const {
    return sensor_provider_.get();
  }
  void RemoveSensorProxy(SensorProxy* proxy);
  using SensorsSet = HeapHashSet<WeakMember<SensorProxy>>;
  const SensorsSet& sensor_proxies() const { return sensor_proxies_; }

  // For SensorProviderProxy personal use.
  void InitializeIfNeeded();
  bool IsInitialized() const { return sensor_provider_.is_bound(); }
  void OnSensorProviderConnectionError();
  SensorsSet sensor_proxies_;

  mojo::Remote<device::mojom::blink::SensorProvider> sensor_provider_;
  bool inspector_mode_;

  DISALLOW_COPY_AND_ASSIGN(SensorProviderProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROVIDER_PROXY_H_
