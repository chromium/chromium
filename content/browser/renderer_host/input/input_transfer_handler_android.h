// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_TRANSFER_HANDLER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_TRANSFER_HANDLER_ANDROID_H_

#include <memory>
#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/input/viz_touch_state.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/transfer_input_to_viz_result.h"
#include "content/public/browser/render_widget_host.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/events/android/motion_event_android.h"

namespace content {

class InputTransferHandlerAndroidClient {
 public:
  virtual gpu::SurfaceHandle GetRootSurfaceHandle() = 0;
  virtual void SendStateOnTouchTransfer(const ui::MotionEvent& event,
                                        bool browser_would_have_handled) = 0;
  virtual bool IsMojoRIRDelegateConnectionSetup() = 0;
};

// The class assumes transfer input to viz is supported, so instantiate only
// when |input::IsTransferInputToVizSupported()| returns true.
// The class is responsible for coordinating with java side InputTransferHandler
// which may end up issuing |WindowManager.transferTouchGesture| Android API
// call. In case of a successful transfer, rest of the touch sequence is
// consumed until a TOUCH_CANCEL is seen. InputTransferHandlerAndroid is owned
// by RenderWidgetHostViewAndroid, instantiated if
// |input::IsTransferInputToVizSupported()| returns true.
class CONTENT_EXPORT InputTransferHandlerAndroid {
 public:
  class JniDelegate {
   public:
    virtual ~JniDelegate() = default;
    virtual int MaybeTransferInputToViz(int surface_id) = 0;
    virtual int TransferInputToViz(int surface_id) = 0;
  };

  explicit InputTransferHandlerAndroid(
      InputTransferHandlerAndroidClient* client);
  virtual ~InputTransferHandlerAndroid();

  // Virtual for testing.
  virtual bool OnTouchEvent(const ui::MotionEventAndroid& event,
                            bool is_ignoring_input_events = false);

  void set_jni_delegate_for_testing(std::unique_ptr<JniDelegate> delegate) {
    jni_delegate_ = std::move(delegate);
  }

  static constexpr const char* kTouchMovesSeenHistogram =
      "Android.InputOnViz.Browser.TouchMovesSeenAfterTransfer";
  static constexpr const char* kEventsAfterTransferHistogram =
      "Android.InputOnViz.Browser.EventsAfterTransfer";
  static constexpr const char* kTransferInputToVizResultHistogram =
      "Android.InputOnViz.Browser.TransferInputToVizResult2";
  static constexpr const char* kEventsInDroppedSequenceHistogram =
      "Android.InputOnViz.Browser.NumEventsInDroppedSequence2";
  static constexpr const char* kEventTypesInDroppedSequenceHistogram =
      "Android.InputOnViz.Browser.EventTypesInDroppedSequence";
  static constexpr const char* kTouchSequenceDroppedReasonHistogram =
      "Android.InputOnViz.Browser.SequenceDroppedReason3";
  static constexpr const char* kNewSequenceTransferredByOSHistogram =
      "Android.InputOnViz.Browser.NewSequenceTransferredByOS";

  bool touch_transferred() {
    return handler_state_ == HandlerState::kConsumeEventsUntilCancel;
  }
  bool FilterRedundantDownEvent(const ui::MotionEvent& event);

  void OnDetachedFromWindow();

  enum class RequestInputBackReason {
    kStartDragAndDropGesture = 0,
    kStartTouchSelectionDragGesture = 1,
    kStartOverscrollGestures = 2,
  };
  void RequestInputBack(RequestInputBackReason reason);

  // Virtual for testing.
  // This is "potentially" active due to a race: Viz might have ended its
  // previous sequence but not yet updated shared memory. If the Browser then
  // sees a new DOWN event, it cannot distinguish a stale "active" state from a
  // genuine multi-touch. The caller must reconcile this ambiguity (e.g., via
  // `browser_would_have_handled=true`).
  virtual bool IsTouchSequencePotentiallyActiveOnViz() const;

  RenderWidgetHost::InputEventObserver& GetInputObserver() {
    return input_observer_;
  }

 protected:
  // Virtual for testing.
  virtual const viz::VizTouchState* GetVizTouchState() const;

 private:
  class InputObserver : public RenderWidgetHost::InputEventObserver {
   public:
    explicit InputObserver(InputTransferHandlerAndroid& transfer_handler);
    ~InputObserver() override;
    // Start RenderWidgetHost::InputEventObserver overrides
    void OnInputEvent(const RenderWidgetHost& host,
                      const blink::WebInputEvent& event) override;
    // End RenderWidgetHost::InputEventObserver overrides

   private:
    const raw_ref<InputTransferHandlerAndroid> transfer_handler_;
  };

  void Reset();
  void OnTouchTransferredSuccessfully(const ui::MotionEventAndroid& event,
                                      bool browser_would_have_handled);

  void EmitTransferResultHistogramAndTraceEvent(
      TransferInputToVizResult result);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(TransferredSequenceType)
  enum class TransferredSequenceType {
    kActionDown = 0,
    kPointerDown = 1,
    kMaxValue = kPointerDown,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:TransferredSequenceType)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(InputOnVizSequenceDroppedReason)
  enum class InputOnVizSequenceDroppedReason {
    kActiveSeqOnVizAbnormalDownTime = 0,
    kFailedToTransferPotentialPointer = 1,
    kAndroidOSTransferredANewSequence = 2,
    kMaxValue = kAndroidOSTransferredANewSequence,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:InputOnVizSequenceDroppedReason,
  //   //base/tracing/protos/chrome_track_event.proto:InputOnVizSequenceDroppedReason
  // )

  void EmitSequenceDroppedReasonTraceEvent(
      InputOnVizSequenceDroppedReason reason);

  void OnStartDroppingSequence(const ui::MotionEventAndroid& event,
                               InputOnVizSequenceDroppedReason reason);

  void DropCurrentSequence(const ui::MotionEventAndroid& event);
  bool ConsumeEventsUntilCancel(const ui::MotionEventAndroid& event);
  void ConsumeSequence(const ui::MotionEventAndroid& event);

  friend class MockInputTransferHandler;
  InputTransferHandlerAndroid();

  raw_ptr<InputTransferHandlerAndroidClient> client_ = nullptr;
  // Stores the down time of first down event of the most recent touch sequence
  // transferred to VizCompositor. See
  // (https://developer.android.com/reference/android/view/MotionEvent#getDownTime())
  base::TimeTicks cached_transferred_sequence_down_time_ms_;
  // When a touch sequence is successfully transferred to Viz then current time
  // is written into `last_successful_transfer_time_`.
  // Used to detect when a touch cancel might have been missed and unblock
  // processing of touch sequences occurring later than this time.
  base::TimeTicks last_successful_transfer_time_;

  int num_events_in_dropped_sequence_ = 0;

  enum class HandlerState {
    // Handler is just passively listening for events in this state.
    kIdle,
    // The sequence is being dropped since a potential pointer sequence failed
    // to transfer.
    kDroppingCurrentSequence,
    // The touch sequence was transferred to Viz and the handler is consuming
    // rest of sequence that might hit Browser.
    kConsumeEventsUntilCancel,
    // Consume current sequence until an action cancel or action up comes in.
    kConsumeSequence,
  } handler_state_ = HandlerState::kIdle;

  bool requested_input_back_ = false;
  std::optional<RequestInputBackReason> requested_input_back_reason_ =
      std::nullopt;
  int touch_moves_seen_after_transfer_ = 0;
  std::unique_ptr<JniDelegate> jni_delegate_ = nullptr;

  // In cases where system transfers a different sequence than the one requested
  // by Chrome, a new state is transferred corresponding to the potential
  // transferred touch sequence. To create new state in such scenarios this
  // variable is being used.
  bool last_sent_browser_would_have_handled_ = false;

  InputObserver input_observer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_TRANSFER_HANDLER_ANDROID_H_
