/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PANNER_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PANNER_NODE_H_

#include <memory>
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/cone_effect.h"
#include "third_party/blink/renderer/platform/audio/distance_effect.h"
#include "third_party/blink/renderer/platform/audio/panner.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class BaseAudioContext;
class PannerOptions;

// PannerNode is an AudioNode with one input and one output.
// It positions a sound in 3D space, with the exact effect dependent on the
// panning model.  It has a position and an orientation in 3D space which is
// relative to the position and orientation of the context's AudioListener.  A
// distance effect will attenuate the gain as the position moves away from the
// listener.  A cone effect will attenuate the gain as the orientation moves
// away from the listener.  All of these effects follow the OpenAL specification
// very closely.

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
  // AudioListener* Listener();
  CrossThreadPersistent<AudioListener> Listener() const;

  bool SetPanningModel(Panner::PanningModel);  // Returns true on success.
  bool SetDistanceModel(unsigned);  // Returns true on success.

  void CalculateAzimuthElevation(double* out_azimuth,
                                 double* out_elevation,
                                 const FloatPoint3D& position,
                                 const FloatPoint3D& listener_position,
                                 const FloatPoint3D& listener_forward,
                                 const FloatPoint3D& listener_up);

  // Returns the combined distance and cone gain attenuation.
  float CalculateDistanceConeGain(const FloatPoint3D& position,
                                  const FloatPoint3D& orientation,
                                  const FloatPoint3D& listener_position);

  void AzimuthElevation(double* out_azimuth, double* out_elevation);
  float DistanceConeGain();

  bool IsAzimuthElevationDirty() const { return is_azimuth_elevation_dirty_; }
  bool IsDistanceConeGainDirty() const { return is_distance_cone_gain_dirty_; }
  void UpdateDirtyState();

  // AudioListener is held alive by PannerNode.
  CrossThreadWeakPersistent<AudioListener> listener_;
  std::unique_ptr<Panner> panner_;
  Panner::PanningModel panning_model_;
  unsigned distance_model_;

  bool is_azimuth_elevation_dirty_;
  bool is_distance_cone_gain_dirty_;

  // Gain
  DistanceEffect distance_effect_;
  ConeEffect cone_effect_;

  // Cached values
  double cached_azimuth_;
  double cached_elevation_;
  float cached_distance_cone_gain_;

  const FloatPoint3D GetPosition() const {
    auto x = position_x_->IsAudioRate() ? position_x_->FinalValue()
                                        : position_x_->Value();
    auto y = position_y_->IsAudioRate() ? position_y_->FinalValue()
                                        : position_y_->Value();
    auto z = position_z_->IsAudioRate() ? position_z_->FinalValue()
                                        : position_z_->Value();

    return FloatPoint3D(x, y, z);
  }

  const FloatPoint3D Orientation() const {
    auto x = orientation_x_->IsAudioRate() ? orientation_x_->FinalValue()
                                           : orientation_x_->Value();
    auto y = orientation_y_->IsAudioRate() ? orientation_y_->FinalValue()
                                           : orientation_y_->Value();
    auto z = orientation_z_->IsAudioRate() ? orientation_z_->FinalValue()
                                           : orientation_z_->Value();

    return FloatPoint3D(x, y, z);
  }

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

  FloatPoint3D last_position_;
  FloatPoint3D last_orientation_;

  // Synchronize process() with setting of the panning model, source's location
  // information, listener, distance parameters and sound cones.
  mutable Mutex process_lock_;
};

class PannerNode final : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PannerNode* Create(BaseAudioContext&, ExceptionState&);
  static PannerNode* Create(BaseAudioContext*,
                            const PannerOptions*,
                            ExceptionState&);
  PannerHandler& GetPannerHandler() const;

  PannerNode(BaseAudioContext&);

  void Trace(Visitor*) const override;

  // Uses a 3D cartesian coordinate system
  AudioParam* positionX() const { return position_x_; }
  AudioParam* positionY() const { return position_y_; }
  AudioParam* positionZ() const { return position_z_; }

  AudioParam* orientationX() const { return orientation_x_; }
  AudioParam* orientationY() const { return orientation_y_; }
  AudioParam* orientationZ() const { return orientation_z_; }

  String panningModel() const;
  void setPanningModel(const String&);
  void setPosition(float x, float y, float z, ExceptionState&);
  void setOrientation(float x, float y, float z, ExceptionState&);
  String distanceModel() const;
  void setDistanceModel(const String&);
  double refDistance() const;
  void setRefDistance(double, ExceptionState&);
  double maxDistance() const;
  void setMaxDistance(double, ExceptionState&);
  double rolloffFactor() const;
  void setRolloffFactor(double, ExceptionState&);
  double coneInnerAngle() const;
  void setConeInnerAngle(double);
  double coneOuterAngle() const;
  void setConeOuterAngle(double);
  double coneOuterGain() const;
  void setConeOuterGain(double, ExceptionState&);

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  Member<AudioParam> position_x_;
  Member<AudioParam> position_y_;
  Member<AudioParam> position_z_;

  Member<AudioParam> orientation_x_;
  Member<AudioParam> orientation_y_;
  Member<AudioParam> orientation_z_;

  // This listener is held alive here to allow referencing it from PannerHandler
  // via weak reference.
  Member<AudioListener> listener_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_PANNER_NODE_H_
