// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PANNER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PANNER_HANDLER_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/cone_effect.h"
#include "third_party/blink/renderer/platform/audio/distance_effect.h"
#include "third_party/blink/renderer/platform/audio/panner.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

class AudioBus;
class AudioListener;
class AudioParamHandler;

class PannerHandler final : public AudioHandler {
 public:
  // These enums are used to distinguish what cached values of panner are dirty.
  enum {
    kAzimuthElevationDirty = 0x1,
    kDistanceConeGainDirty = 0x2,
  };

  static scoped_refptr<PannerHandler> Create(AudioNode&,
                                             float sample_rate,
                                             AudioParamHandler& position_x,
                                             AudioParamHandler& position_y,
                                             AudioParamHandler& position_z,
                                             AudioParamHandler& orientation_x,
                                             AudioParamHandler& orientation_y,
                                             AudioParamHandler& orientation_z);

  PannerHandler(const PannerHandler&) = delete;
  PannerHandler& operator=(const PannerHandler&) = delete;

  ~PannerHandler() override;

  // AudioHandler
  void ProcessIfNecessary(uint32_t frames_to_process) override;
  void Process(uint32_t frames_to_process) override;
  void ProcessSampleAccurateValues(AudioBus* destination,
                                   const AudioBus* source,
                                   uint32_t frames_to_process);
  void ProcessOnlyAudioParams(uint32_t frames_to_process) override;
  void Initialize() override;
  void Uninitialize() override;

  // Panning model
  String PanningModel() const;
  void SetPanningModel(const String&);

  // Position and orientation
  void SetPosition(float x, float y, float z, ExceptionState&);
  void SetOrientation(float x, float y, float z, ExceptionState&);

  // Distance parameters
  String DistanceModel() const;
  void SetDistanceModel(const String&);

  double RefDistance() { return distance_effect_.RefDistance(); }
  void SetRefDistance(double);

  double MaxDistance() { return distance_effect_.MaxDistance(); }
  void SetMaxDistance(double);

  double RolloffFactor() { return distance_effect_.RolloffFactor(); }
  void SetRolloffFactor(double);

  // Sound cones - angles in degrees
  double ConeInnerAngle() const { return cone_effect_.InnerAngle(); }
  void SetConeInnerAngle(double);

  double ConeOuterAngle() const { return cone_effect_.OuterAngle(); }
  void SetConeOuterAngle(double);

  double ConeOuterGain() const { return cone_effect_.OuterGain(); }
  void SetConeOuterGain(double);

  void MarkPannerAsDirty(unsigned);

  double TailTime() const override { return panner_ ? panner_->TailTime() : 0; }
  double LatencyTime() const override {
    return panner_ ? panner_->LatencyTime() : 0;
  }
  bool RequiresTailProcessing() const final;

  void SetChannelCount(unsigned, ExceptionState&) final;
  void SetChannelCountMode(const String&, ExceptionState&) final;

 private:
  PannerHandler(AudioNode&,
                float sample_rate,
                AudioParamHandler& position_x,
                AudioParamHandler& position_y,
                AudioParamHandler& position_z,
                AudioParamHandler& orientation_x,
                AudioParamHandler& orientation_y,
                AudioParamHandler& orientation_z);

  // BaseAudioContext's listener
  CrossThreadPersistent<AudioListener> Listener() const;

  bool SetPanningModel(Panner::PanningModel);  // Returns true on success.
  bool SetDistanceModel(unsigned);             // Returns true on success.

  void CalculateAzimuthElevation(double* out_azimuth,
                                 double* out_elevation,
                                 const gfx::Point3F& position,
                                 const gfx::Point3F& listener_position,
                                 const gfx::Vector3dF& listener_forward,
                                 const gfx::Vector3dF& listener_up);

  // Returns the combined distance and cone gain attenuation.
  float CalculateDistanceConeGain(const gfx::Point3F& position,
                                  const gfx::Vector3dF& orientation,
                                  const gfx::Point3F& listener_position);

  void AzimuthElevation(double* out_azimuth, double* out_elevation);
  float DistanceConeGain();

  bool IsAzimuthElevationDirty() const { return is_azimuth_elevation_dirty_; }
  bool IsDistanceConeGainDirty() const { return is_distance_cone_gain_dirty_; }
  void UpdateDirtyState();

  // AudioListener is held alive by PannerNode.
  CrossThreadWeakPersistent<AudioListener> listener_;
  std::unique_ptr<Panner> panner_;
  Panner::PanningModel panning_model_;
  unsigned distance_model_ = DistanceEffect::kModelInverse;

  bool is_azimuth_elevation_dirty_ = true;
  bool is_distance_cone_gain_dirty_ = true;

  // Gain
  DistanceEffect distance_effect_;
  ConeEffect cone_effect_;

  // Cached values
  double cached_azimuth_ = 0;
  double cached_elevation_ = 0;
  float cached_distance_cone_gain_ = 1.0f;

  gfx::Point3F GetPosition() const;
  gfx::Vector3dF Orientation() const;

  // True if any of this panner's AudioParams have automations.
  bool HasSampleAccurateValues() const;

  // True if any of the panner's AudioParams are set for a-rate
  // automations (the default).
  bool IsAudioRate() const;

  scoped_refptr<AudioParamHandler> position_x_;
  scoped_refptr<AudioParamHandler> position_y_;
  scoped_refptr<AudioParamHandler> position_z_;

  scoped_refptr<AudioParamHandler> orientation_x_;
  scoped_refptr<AudioParamHandler> orientation_y_;
  scoped_refptr<AudioParamHandler> orientation_z_;

  gfx::Point3F last_position_;
  gfx::Vector3dF last_orientation_;

  // Synchronize process() with setting of the panning model, source's location
  // information, listener, distance parameters and sound cones.
  mutable base::Lock process_lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PANNER_HANDLER_H_
