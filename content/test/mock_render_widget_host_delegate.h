// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RENDER_WIDGET_HOST_DELEGATE_H_
#define CONTENT_TEST_MOCK_RENDER_WIDGET_HOST_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/test/stub_render_view_host_delegate_view.h"

namespace content {

class RenderWidgetHostImpl;

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate();

  MockRenderWidgetHostDelegate(const MockRenderWidgetHostDelegate&) = delete;
  MockRenderWidgetHostDelegate& operator=(const MockRenderWidgetHostDelegate&) =
      delete;

  ~MockRenderWidgetHostDelegate() override;

  const NativeWebKeyboardEvent* last_event() const { return last_event_.get(); }
  void set_widget_host(RenderWidgetHostImpl* rwh) { rwh_ = rwh; }
  void set_is_fullscreen(bool is_fullscreen) { is_fullscreen_ = is_fullscreen; }
  void set_focused_widget(RenderWidgetHostImpl* focused_widget) {
    focused_widget_ = focused_widget;
  }
  void set_pre_handle_keyboard_event_result(
      KeyboardEventProcessingResult result) {
    pre_handle_keyboard_event_result_ = result;
  }
  void set_should_ignore_input_events(bool ignore) {
    should_ignore_input_events_ = ignore;
  }
  void CreateInputEventRouter();

  // RenderWidgetHostDelegate:
  void ResizeDueToAutoResize(RenderWidgetHostImpl* render_widget_host,
                             const gfx::Size& new_size) override;
  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) override;
  void ExecuteEditCommand(const std::string& command,
                          const absl::optional<std::u16string>& value) override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void SelectAll() override;
  RenderWidgetHostInputEventRouter* GetInputEventRouter() override;
  RenderWidgetHostImpl* GetFocusedRenderWidgetHost(
      RenderWidgetHostImpl* widget_host) override;
  void SendScreenRects() override;
  TextInputManager* GetTextInputManager() override;
  bool IsFullscreen() override;
  RenderViewHostDelegateView* GetDelegateView() override;
  VisibleTimeRequestTrigger& GetVisibleTimeRequestTrigger() override;
  bool ShouldIgnoreInputEvents() override;

 private:
  std::unique_ptr<NativeWebKeyboardEvent> last_event_;
  raw_ptr<RenderWidgetHostImpl> rwh_ = nullptr;
  std::unique_ptr<RenderWidgetHostInputEventRouter> rwh_input_event_router_;
  bool is_fullscreen_ = false;
  TextInputManager text_input_manager_;
  raw_ptr<RenderWidgetHostImpl> focused_widget_ = nullptr;
  KeyboardEventProcessingResult pre_handle_keyboard_event_result_ =
      KeyboardEventProcessingResult::NOT_HANDLED;
  StubRenderViewHostDelegateView rvh_delegate_view_;
  bool should_ignore_input_events_ = false;
  VisibleTimeRequestTrigger visible_time_request_trigger_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_RENDER_WIDGET_HOST_DELEGATE_H_
