/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_LISTENER_H_

#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_helper_mixin.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

class HRTFDatabaseLoader;
class PannerHandler;

// AudioListener maintains the state of the listener in the audio scene as
// defined in the OpenAL specification.

class AudioListener : public ScriptWrappable, public InspectorHelperMixin {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AudioListener(BaseAudioContext&);
  ~AudioListener() override;

  // Location of the listener
  AudioParam* positionX() const { return position_x_; }
  AudioParam* positionY() const { return position_y_; }
  AudioParam* positionZ() const { return position_z_; }

  // Forward direction vector of the listener
  AudioParam* forwardX() const { return forward_x_; }
  AudioParam* forwardY() const { return forward_y_; }
  AudioParam* forwardZ() const { return forward_z_; }

  // Up direction vector for the listener
  AudioParam* upX() const { return up_x_; }
  AudioParam* upY() const { return up_y_; }
  AudioParam* upZ() const { return up_z_; }

  // True if any of AudioParams have automations.
  bool HasSampleAccurateValues() const;

  // True if any of the AudioParams are set for a-rate automations
  // (the default).
  bool IsAudioRate() const;

  // Update the internal state of the listener, including updating the dirty
  // state of all PannerNodes if necessary.
  void UpdateState();

  bool IsListenerDirty() const { return is_listener_dirty_; }

  const gfx::Point3F GetPosition() const {
    return gfx::Point3F(position_x_->value(), position_y_->value(),
                        position_z_->value());
  }
  const gfx::Vector3dF Orientation() const {
    return gfx::Vector3dF(forward_x_->value(), forward_y_->value(),
                          forward_z_->value());
  }
  const gfx::Vector3dF UpVector() const {
    return gfx::Vector3dF(up_x_->value(), up_y_->value(), up_z_->value());
  }

  const float* GetPositionXValues(uint32_t frames_to_process);
  const float* GetPositionYValues(uint32_t frames_to_process);
  const float* GetPositionZValues(uint32_t frames_to_process);

  const float* GetForwardXValues(uint32_t frames_to_process);
  const float* GetForwardYValues(uint32_t frames_to_process);
  const float* GetForwardZValues(uint32_t frames_to_process);

  const float* GetUpXValues(uint32_t frames_to_process);
  const float* GetUpYValues(uint32_t frames_to_process);
  const float* GetUpZValues(uint32_t frames_to_process);

  // Position
  void setPosition(float x, float y, float z, ExceptionState& exceptionState) {
    setPosition(gfx::Point3F(x, y, z), exceptionState);
  }

  // Orientation and Up-vector
  void setOrientation(float x,
                      float y,
                      float z,
                      float up_x,
                      float up_y,
                      float up_z,
                      ExceptionState& exceptionState) {
    setOrientation(gfx::Vector3dF(x, y, z), exceptionState);
    SetUpVector(gfx::Vector3dF(up_x, up_y, up_z), exceptionState);
  }

  base::Lock& ListenerLock() { return listener_lock_; }
  void AddPanner(PannerHandler&);
  void RemovePanner(PannerHandler&);

  // HRTF DB loader
  HRTFDatabaseLoader* HrtfDatabaseLoader() {
    return hrtf_database_loader_.get();
  }
  void CreateAndLoadHRTFDatabaseLoader(float);
  bool IsHRTFDatabaseLoaded();
  void WaitForHRTFDatabaseLoaderThreadCompletion();

  // InspectorHelperMixin: Note that this object belongs to a BaseAudioContext,
  // so these methods get called by the parent context.
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

  void Trace(Visitor*) const override;

 private:
  void setPosition(const gfx::Point3F&, ExceptionState&);
  void setOrientation(const gfx::Vector3dF&, ExceptionState&);
  void SetUpVector(const gfx::Vector3dF&, ExceptionState&);

  void MarkPannersAsDirty(unsigned) EXCLUSIVE_LOCKS_REQUIRED(listener_lock_);

  // Location of the listener
  Member<AudioParam> position_x_;
  Member<AudioParam> position_y_;
  Member<AudioParam> position_z_;

  // Forward direction vector of the listener
  Member<AudioParam> forward_x_;
  Member<AudioParam> forward_y_;
  Member<AudioParam> forward_z_;

  // Up direction vector for the listener
  Member<AudioParam> up_x_;
  Member<AudioParam> up_y_;
  Member<AudioParam> up_z_;

  // The position, forward, and up vectors from the last rendering quantum.
  gfx::Point3F last_position_ GUARDED_BY(listener_lock_);
  gfx::Vector3dF last_forward_ GUARDED_BY(listener_lock_);
  gfx::Vector3dF last_up_ GUARDED_BY(listener_lock_);

  // Last time that the automations were updated.
  double last_update_time_ = -1;

  // Set every rendering quantum if the listener has moved in any way
  // (position, forward, or up).  This should only be read or written to from
  // the audio thread.
  bool is_listener_dirty_ = false;

  void UpdateValuesIfNeeded(uint32_t frames_to_process);

  AudioFloatArray position_x_values_;
  AudioFloatArray position_y_values_;
  AudioFloatArray position_z_values_;

  AudioFloatArray forward_x_values_;
  AudioFloatArray forward_y_values_;
  AudioFloatArray forward_z_values_;

  AudioFloatArray up_x_values_;
  AudioFloatArray up_y_values_;
  AudioFloatArray up_z_values_;

  // Synchronize a panner's process() with setting of the state of the listener.
  mutable base::Lock listener_lock_;
  // List for pannerNodes in context. This is updated only in the main thread,
  // and can be referred in audio thread.
  // These raw pointers are safe because PannerHandler::uninitialize()
  // unregisters it from m_panners.
  HashSet<PannerHandler*> panners_;
  // HRTF DB loader for panner node.
  scoped_refptr<HRTFDatabaseLoader> hrtf_database_loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_LISTENER_H_
