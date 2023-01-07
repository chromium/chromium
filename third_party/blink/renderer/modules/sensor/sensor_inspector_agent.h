// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_INSPECTOR_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_INSPECTOR_AGENT_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LocalDOMWindow;
class LocalFrame;
class SensorProviderProxy;

class SensorInspectorAgent : public GarbageCollected<SensorInspectorAgent> {
 public:
  explicit SensorInspectorAgent(LocalDOMWindow* window);

  SensorInspectorAgent(const SensorInspectorAgent&) = delete;
  SensorInspectorAgent& operator=(const SensorInspectorAgent&) = delete;

  virtual void Trace(Visitor*) const;

  void DidCommitLoadForLocalFrame(LocalFrame* frame);

  void SetOrientationSensorOverride(double alpha, double beta, double gamma);

  void Disable();

 private:
  Member<SensorProviderProxy> provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_INSPECTOR_AGENT_H_
