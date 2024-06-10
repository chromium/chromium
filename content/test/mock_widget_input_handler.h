// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WIDGET_INPUT_HANDLER_H_
#define CONTENT_TEST_MOCK_WIDGET_INPUT_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"

namespace content {

class MockWidgetInputHandler : public blink::mojom::WidgetInputHandler {
 public:
  MockWidgetInputHandler();
  MockWidgetInputHandler(
      mojo::PendingReceiver<blink::mojom::WidgetInputHandler> receiver,
      mojo::PendingRemote<blink::mojom::WidgetInputHandlerHost> host);

  MockWidgetInputHandler(const MockWidgetInputHandler&) = delete;
  MockWidgetInputHandler& operator=(const MockWidgetInputHandler&) = delete;

  ~MockWidgetInputHandler() override;

  class DispatchedEditCommandMessage;
  class DispatchedEventMessage;
  class DispatchedFocusMessage;
  class DispatchedIMEMessage;
  class DispatchedRequestCompositionUpdatesMessage;
  class DispatchedFinishComposingMessage;

  // Abstract storage of a received call on the MockWidgetInputHandler
  // interface.
  class DispatchedMessage {
   public:
    explicit DispatchedMessage(const std::string& name);

    DispatchedMessage(const DispatchedMessage&) = delete;
    DispatchedMessage& operator=(const DispatchedMessage&) = delete;

    virtual ~DispatchedMessage();

    // Cast this to a DispatchedEditCommandMessage if it is one, null
    // otherwise.
    virtual DispatchedEditCommandMessage* ToEditCommand();

    // Cast this to a DispatchedEventMessage if it is one, null otherwise.
    virtual DispatchedEventMessage* ToEvent();

    // Cast this to an DispatchedFocusMessage if it is one, null otherwise.
    virtual DispatchedFocusMessage* ToFocus();

    // Cast this to an DispatchedIMEMessage if it is one, null otherwise.
    virtual DispatchedIMEMessage* ToIME();

    // Cast this to a DispatchedRequestCompositionUpdateMessage if it is one,
    // null otherwise.
    virtual DispatchedRequestCompositionUpdatesMessage*
    ToRequestCompositionUpdates();

    // Cast this to a DispatchedFinishComposingMessage if it is one,
    // null otherwise.
    virtual DispatchedFinishComposingMessage* ToFinishComposing();

    // Return the name associated with this message. It will either match
    // the message call name (eg. MouseCaptureLost) or the name of an
    // input event (eg. GestureScrollBegin).
    const std::string& name() const { return name_; }

   private:
    std::string name_;
  };

  // A DispatchedMessage that stores the IME compositing parameters
  // that were invoked with.
  class DispatchedIMEMessage : public DispatchedMessage {
   public:
    DispatchedIMEMessage(const std::string& name,
                         const std::u16string& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& range,
                         int32_t start,
                         int32_t end);

    DispatchedIMEMessage(const DispatchedIMEMessage&) = delete;
    DispatchedIMEMessage& operator=(const DispatchedIMEMessage&) = delete;

    ~DispatchedIMEMessage() override;

    // Override and return |this|.
    DispatchedIMEMessage* ToIME() override;

    // Returns if this message matches the parameters passed in.
    bool Matches(const std::u16string& text,
                 const std::vector<ui::ImeTextSpan>& ime_text_spans,
                 const gfx::Range& range,
                 int32_t start,
                 int32_t end) const;

   private:
    std::u16string text_;
    std::vector<ui::ImeTextSpan> text_spans_;
    gfx::Range range_;
    int32_t start_;
    int32_t end_;
  };

  // A DispatchedMessage that stores the IME compositing parameters
  // that were invoked with.
  class DispatchedEditCommandMessage : public DispatchedMessage {
   public:
    explicit DispatchedEditCommandMessage(
        std::vector<blink::mojom::EditCommandPtr> commands);

    DispatchedEditCommandMessage(const DispatchedEditCommandMessage&) = delete;
    DispatchedEditCommandMessage& operator=(
        const DispatchedEditCommandMessage&) = delete;

    ~DispatchedEditCommandMessage() override;

    // Override and return |this|.
    DispatchedEditCommandMessage* ToEditCommand() override;

    const std::vector<blink::mojom::EditCommandPtr>& Commands() const;

   private:
    std::vector<blink::mojom::EditCommandPtr> commands_;
  };

  // A DispatchedMessage that stores the focus parameters
  // that were invoked with.
  class DispatchedFocusMessage : public DispatchedMessage {
   public:
    explicit DispatchedFocusMessage(bool focused);

    DispatchedFocusMessage(const DispatchedFocusMessage&) = delete;
    DispatchedFocusMessage& operator=(const DispatchedFocusMessage&) = delete;

    ~DispatchedFocusMessage() override;

    // Override and return |this|.
    DispatchedFocusMessage* ToFocus() override;

    bool focused() const { return focused_; }

   private:
    const bool focused_;
  };

  // A DispatchedMessage that stores the InputEvent and callback
  // that was passed to the MockWidgetInputHandler interface.
  class DispatchedEventMessage : public DispatchedMessage {
   public:
    DispatchedEventMessage(std::unique_ptr<blink::WebCoalescedInputEvent> event,
                           DispatchEventCallback callback);

    DispatchedEventMessage(const DispatchedEventMessage&) = delete;
    DispatchedEventMessage& operator=(const DispatchedEventMessage&) = delete;

    ~DispatchedEventMessage() override;

    // Override and return |this|.
    DispatchedEventMessage* ToEvent() override;

    // Invoke the callback on this object with the passed in |state|.
    // The callback is called with default values for the other fields.
    void CallCallback(blink::mojom::InputEventResultState state);

    // Invoke a callback with all the arguments provided.
    void CallCallback(blink::mojom::InputEventResultSource source,
                      const ui::LatencyInfo& latency_info,
                      blink::mojom::InputEventResultState state,
                      blink::mojom::DidOverscrollParamsPtr overscroll,
                      blink::mojom::TouchActionOptionalPtr touch_action);

    // Return if the callback is set.
    bool HasCallback() const;

    // Return the associated event.
    const blink::WebCoalescedInputEvent* Event() const;

   private:
    std::unique_ptr<blink::WebCoalescedInputEvent> event_;
    DispatchEventCallback callback_;
  };

  // A DispatchedMessage that stores the RequestCompositionUpdates parameters
  // that were invoked with.
  class DispatchedRequestCompositionUpdatesMessage : public DispatchedMessage {
   public:
    DispatchedRequestCompositionUpdatesMessage(bool immediate_request,
                                               bool monitor_request);

    DispatchedRequestCompositionUpdatesMessage(
        const DispatchedRequestCompositionUpdatesMessage&) = delete;
    DispatchedRequestCompositionUpdatesMessage& operator=(
        const DispatchedRequestCompositionUpdatesMessage&) = delete;

    ~DispatchedRequestCompositionUpdatesMessage() override;

    // Override and return |this|.
    DispatchedRequestCompositionUpdatesMessage* ToRequestCompositionUpdates()
        override;

    bool immediate_request() const { return immediate_request_; }
    bool monitor_request() const { return monitor_request_; }

   private:
    const bool immediate_request_;
    const bool monitor_request_;
  };

  // A DispatchedMessage that stores the FinishComposingText parameters
  // that were invoked with.
  class DispatchedFinishComposingMessage : public DispatchedMessage {
   public:
    explicit DispatchedFinishComposingMessage(bool keep_selection);
    DispatchedFinishComposingMessage(const DispatchedFinishComposingMessage&) =
        delete;
    DispatchedFinishComposingMessage& operator=(
        const DispatchedFinishComposingMessage&) = delete;
    ~DispatchedFinishComposingMessage() override;

    // Override and return |this|.
    DispatchedFinishComposingMessage* ToFinishComposing() override;

    bool keep_selection() const { return keep_selection_; }

   private:
    const bool keep_selection_;
  };

  // blink::mojom::WidgetInputHandler override.
  void SetFocus(blink::mojom::FocusState focus_state) override;
  void MouseCaptureLost() override;
  void SetEditCommandsForNextKeyEvent(
      std::vector<blink::mojom::EditCommandPtr> commands) override;
  void CursorVisibilityChanged(bool visible) override;
  void ImeSetComposition(const std::u16string& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& range,
                         int32_t start,
                         int32_t end,
                         ImeSetCompositionCallback callback) override;
  void ImeCommitText(const std::u16string& text,
                     const std::vector<ui::ImeTextSpan>& ime_text_spans,
                     const gfx::Range& range,
                     int32_t relative_cursor_position,
                     ImeCommitTextCallback callback) override;
  void ImeFinishComposingText(bool keep_selection) override;
  void RequestTextInputStateUpdate() override;
  void RequestCompositionUpdates(bool immediate_request,
                                 bool monitor_request) override;

  void DispatchEvent(std::unique_ptr<blink::WebCoalescedInputEvent> event,
                     DispatchEventCallback callback) override;
  void DispatchNonBlockingEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void WaitForInputProcessed(WaitForInputProcessedCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void AttachSynchronousCompositor(
      mojo::PendingRemote<blink::mojom::SynchronousCompositorControlHost>
          control_host,
      mojo::PendingAssociatedRemote<blink::mojom::SynchronousCompositorHost>
          host,
      mojo::PendingAssociatedReceiver<blink::mojom::SynchronousCompositor>
          compositor_request) override;
#endif
  void GetFrameWidgetInputHandler(
      mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetInputHandler>
          interface_request) override;
  void UpdateBrowserControlsState(
      cc::BrowserControlsState constraints,
      cc::BrowserControlsState current,
      bool animate,
      const std::optional<cc::BrowserControlsOffsetTagsInfo>& offset_tags_info)
      override;

  using MessageVector = std::vector<std::unique_ptr<DispatchedMessage>>;
  MessageVector GetAndResetDispatchedMessages();

 private:
  mojo::Receiver<blink::mojom::WidgetInputHandler> receiver_{this};
  mojo::Remote<blink::mojom::WidgetInputHandlerHost> host_;
  MessageVector dispatched_messages_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WIDGET_INPUT_HANDLER_H_
