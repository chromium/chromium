// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CAPTURE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CAPTURE_CONTROLLER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_start_focus_behavior.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT CaptureController final : public ScriptWrappable,
                                               public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CaptureController* Create(ExecutionContext*);

  explicit CaptureController(ExecutionContext*);

  // IDL interface
  // https://w3c.github.io/mediacapture-screen-share/#dom-capturecontroller-setfocusbehavior
  void setFocusBehavior(V8CaptureStartFocusBehavior, ExceptionState&);

  void SetIsBound(bool value) { is_bound_ = value; }
  bool IsBound() const { return is_bound_; }

  // When getDisplayMedia() is resolved, `video_track_` and `descriptor_id_` are
  // set.
  void SetVideoTrack(MediaStreamTrack* video_track, std::string descriptor_id);

  // Close the window of opportunity to make the focus decision.
  // Further calls to setFocusBehavior() will raise an exception.
  // https://w3c.github.io/mediacapture-screen-share/#dfn-finalize-focus-decision-algorithm
  void FinalizeFocusDecision();

  void Trace(Visitor* visitor) const override;

 private:
  // Whether this CaptureController has been passed to a getDisplayMedia() call.
  // This helps enforce the requirement that any CaptureController may only
  // be used with a single getDisplayMedia() call.
  // Checking `video_track_` for nullness and/or `descriptor_id_` for emptiness
  // would not have sufficed, as `is_bound_` is flipped even if the promise
  // ends up being rejected.
  bool is_bound_ = false;

  // The video track returned by getDisplayMedia(). It is saved to check whether
  // it's live and capturing a tab or a window.
  WeakMember<MediaStreamTrack> video_track_;

  // Identity browser-side the device request to focus or not the captured
  // surface.
  std::string descriptor_id_;

  // Set by setFocusBehavior(). Indicates whether the app wishes the
  // captured surface to be focused or not once getDisplayMedia() is
  // resolved. If left unset, the default behavior will be used.
  // The default behavior is not specified, and may change.
  // Presently, the default is to focus.
  absl::optional<V8CaptureStartFocusBehavior> focus_behavior_;

  // Track whether the window of opportunity to call setFocusBehavior() is still
  // open. Once set to true, this never changes.
  bool focus_decision_finalized_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CAPTURE_CONTROLLER_H_
