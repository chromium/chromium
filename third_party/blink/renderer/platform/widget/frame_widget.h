// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_FRAME_WIDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_FRAME_WIDGET_H_

#include "mojo/public/mojom/base/text_direction.mojom-blink.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-blink.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_swap_result.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/base/ime/mojom/text_input_state.mojom-blink.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom-blink.h"

namespace cc {
class AnimationHost;
class Layer;
class PaintImage;
}  // namespace cc

namespace blink {

// In interface exposed within Blink from local root frames that provides
// local-root specific things related to compositing and input. This
// class extends the FrameWidgetInputHandler implementation. All API calls
// on this class occur on the main thread. input/FrameWidgetInputHandlerImpl
// which also implements the FrameWidgetInputHandler interface runs on the
// compositor thread and proxies calls to this class.
class PLATFORM_EXPORT FrameWidget
    : public mojom::blink::FrameWidgetInputHandler {
 public:
  ~FrameWidget() override;

  // Returns the WebWidgetClient, which is implemented outside of blink.
  virtual WebWidgetClient* Client() const = 0;

  // Returns the compositors's AnimationHost for the widget.
  virtual cc::AnimationHost* AnimationHost() const = 0;

  // Set the browser's behavior when overscroll happens, e.g. whether to glow
  // or navigate.
  virtual void SetOverscrollBehavior(
      const cc::OverscrollBehavior& overscroll_behavior) = 0;

  // Posts a task with the given delay, then calls ScheduleAnimation() on the
  // Client().
  virtual void RequestAnimationAfterDelay(const base::TimeDelta&) = 0;

  // Sets the root layer. The |layer| can be null when detaching the root layer.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> layer) = 0;

  // Used to update the active selection bounds. Pass a default-constructed
  // LayerSelection to clear it.
  virtual void RegisterSelection(cc::LayerSelection selection) = 0;

  // Image decode functionality.
  virtual void RequestDecode(const cc::PaintImage&,
                             base::OnceCallback<void(bool)>) = 0;

  // Forwards to WebFrameWidget::NotifySwapAndPresentationTime().
  // The |callback| will be fired when the corresponding renderer frame is
  // submitted (still called "swapped") to the display compositor (either with
  // DidSwap or DidNotSwap).
  virtual void NotifySwapAndPresentationTimeInBlink(
      WebReportTimeCallback swap_callback,
      WebReportTimeCallback presentation_callback) = 0;

  // Enable or disable BeginMainFrameNotExpected signals from the compositor,
  // which are consumed by the blink scheduler.
  virtual void RequestBeginMainFrameNotExpected(bool request) = 0;

  // A stable numeric Id for the local root's compositor. For tracing/debugging
  // purposes.
  virtual int GetLayerTreeId() = 0;

  // Set or get what event handlers exist in the document contained in the
  // WebWidget in order to inform the compositor thread if it is able to handle
  // an input event, or it needs to pass it to the main thread to be handled.
  // The class is the type of input event, and for each class there is a
  // properties defining if the compositor thread can handle the event.
  virtual void SetEventListenerProperties(cc::EventListenerClass,
                                          cc::EventListenerProperties) = 0;
  virtual cc::EventListenerProperties EventListenerProperties(
      cc::EventListenerClass) const = 0;

  // Returns the DisplayMode in use for the widget.
  virtual mojom::blink::DisplayMode DisplayMode() const = 0;

  // Returns the window segments for the widget.
  virtual const WebVector<gfx::Rect>& WindowSegments() const = 0;

  // Sets the ink metadata on the layer tree host
  virtual void SetDelegatedInkMetadata(
      std::unique_ptr<viz::DelegatedInkMetadata> metadata) = 0;

  // Called when the main thread overscrolled.
  virtual void DidOverscroll(const gfx::Vector2dF& overscroll_delta,
                             const gfx::Vector2dF& accumulated_overscroll,
                             const gfx::PointF& position,
                             const gfx::Vector2dF& velocity) = 0;

  // Requests that a gesture of |injected_type| be reissued at a later point in
  // time. |injected_type| is required to be one of
  // GestureScroll{Begin,Update,End}. The dispatched gesture will scroll the
  // ScrollableArea identified by |scrollable_area_element_id| by the given
  // delta + granularity.
  virtual void InjectGestureScrollEvent(
      WebGestureDevice device,
      const gfx::Vector2dF& delta,
      ui::ScrollGranularity granularity,
      cc::ElementId scrollable_area_element_id,
      WebInputEvent::Type injected_type) = 0;

  // Called when the cursor for the widget changes.
  virtual void DidChangeCursor(const ui::Cursor&) = 0;

  // Return the composition character in window coordinates.
  virtual void GetCompositionCharacterBoundsInWindow(
      Vector<gfx::Rect>* bounds) = 0;

  virtual gfx::Range CompositionRange() = 0;
  // Returns ime_text_spans and corresponding window coordinates for the list
  // of given spans.
  virtual Vector<ui::mojom::blink::ImeTextSpanInfoPtr> GetImeTextSpansInfo(
      const WebVector<ui::ImeTextSpan>& ime_text_spans) = 0;
  virtual WebTextInputInfo TextInputInfo() = 0;
  virtual ui::mojom::blink::VirtualKeyboardVisibilityRequest
  GetLastVirtualKeyboardVisibilityRequest() = 0;
  virtual bool ShouldSuppressKeyboardForFocusedElement() = 0;

  // Return the edit context bounds in window coordinates.
  virtual void GetEditContextBoundsInWindow(
      base::Optional<gfx::Rect>* control_bounds,
      base::Optional<gfx::Rect>* selection_bounds) = 0;

  virtual int32_t ComputeWebTextInputNextPreviousFlags() = 0;
  virtual void ResetVirtualKeyboardVisibilityRequest() = 0;

  // Return the selection bounds in window coordinates. Returns true if the
  // bounds returned were different than the passed in focus and anchor bounds.
  virtual bool GetSelectionBoundsInWindow(gfx::Rect* focus,
                                          gfx::Rect* anchor,
                                          base::i18n::TextDirection* focus_dir,
                                          base::i18n::TextDirection* anchor_dir,
                                          bool* is_anchor_first) = 0;

  // Clear any cached text input state.
  virtual void ClearTextInputState() = 0;

  // This message sends a string being composed with an input method.
  virtual bool SetComposition(const String& text,
                              const Vector<ui::ImeTextSpan>& ime_text_spans,
                              const gfx::Range& replacement_range,
                              int selection_start,
                              int selection_end) = 0;

  // This message deletes the current composition, inserts specified text, and
  // moves the cursor.
  virtual void CommitText(const String& text,
                          const Vector<ui::ImeTextSpan>& ime_text_spans,
                          const gfx::Range& replacement_range,
                          int relative_cursor_pos) = 0;

  // This message inserts the ongoing composition.
  virtual void FinishComposingText(bool keep_selection) = 0;

  virtual bool IsProvisional() = 0;
  virtual uint64_t GetScrollableContainerIdAt(const gfx::PointF& point) = 0;

  virtual bool ShouldHandleImeEvents() { return false; }

  virtual void SetEditCommandsForNextKeyEvent(
      Vector<mojom::blink::EditCommandPtr> edit_commands) = 0;

  // Returns information about the screen where this widget is being displayed.
  virtual const ScreenInfo& GetScreenInfo() = 0;

  // Called to get the position of the widget's window in screen
  // coordinates. Note, the window includes any decorations such as borders,
  // scrollbars, URL bar, tab strip, etc. if they exist.
  virtual gfx::Rect WindowRect() = 0;

  // Called to get the view rect in screen coordinates. This is the actual
  // content view area, i.e. doesn't include any window decorations.
  virtual gfx::Rect ViewRect() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_FRAME_WIDGET_H_
