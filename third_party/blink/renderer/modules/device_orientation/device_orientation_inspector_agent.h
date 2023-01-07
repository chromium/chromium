// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_ORIENTATION_INSPECTOR_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_ORIENTATION_INSPECTOR_AGENT_H_

#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/device_orientation.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class DeviceOrientationController;
class InspectedFrames;
class SensorInspectorAgent;

class MODULES_EXPORT DeviceOrientationInspectorAgent final
    : public InspectorBaseAgent<protocol::DeviceOrientation::Metainfo> {
 public:
  explicit DeviceOrientationInspectorAgent(InspectedFrames*);

  DeviceOrientationInspectorAgent(const DeviceOrientationInspectorAgent&) =
      delete;
  DeviceOrientationInspectorAgent& operator=(
      const DeviceOrientationInspectorAgent&) = delete;

  ~DeviceOrientationInspectorAgent() override;
  void Trace(Visitor*) const override;

  // Protocol methods.
  protocol::Response setDeviceOrientationOverride(double,
                                                  double,
                                                  double) override;
  protocol::Response clearDeviceOrientationOverride() override;

  protocol::Response disable() override;
  void Restore() override;
  void DidCommitLoadForLocalFrame(LocalFrame*) override;

 private:
  DeviceOrientationController& Controller();

  Member<InspectedFrames> inspected_frames_;
  Member<SensorInspectorAgent> sensor_agent_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Double alpha_;
  InspectorAgentState::Double beta_;
  InspectorAgentState::Double gamma_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_ORIENTATION_DEVICE_ORIENTATION_INSPECTOR_AGENT_H_
