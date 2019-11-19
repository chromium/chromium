// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/text_input_client_observer.h"

#include <stddef.h>

#include <memory>

#include "build/build_config.h"
#include "content/common/text_input_client_messages.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/mac/web_substring_util.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

namespace {
uint32_t GetCurrentCursorPositionInFrame(blink::WebLocalFrame* local_frame) {
  blink::WebRange range = local_frame->SelectionRange();
  return range.IsNull() ? 0U : static_cast<uint32_t>(range.StartOffset());
}
}

TextInputClientObserver::TextInputClientObserver(RenderWidget* render_widget)
    : render_widget_(render_widget) {}

TextInputClientObserver::~TextInputClientObserver() = default;

bool TextInputClientObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TextInputClientObserver, message)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringAtPoint,
                        OnStringAtPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_CharacterIndexForPoint,
                        OnCharacterIndexForPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_FirstRectForCharacterRange,
                        OnFirstRectForCharacterRange)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringForRange, OnStringForRange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool TextInputClientObserver::Send(IPC::Message* message) {
  // This class is attached to the main frame RenderWidget, but sends and
  // receives messages while the main frame is remote (and the RenderWidget is
  // undead). The messages are not received on RenderWidgetHostImpl, so there's
  // no need to send through RenderWidget or use its routing id. We avoid this
  // problem then by sending directly through RenderThread instead of through
  // RenderWidget::Send().
  // TODO(crbug.com/669219): This class should not be used while the main frame
  // is remote.
  return RenderThread::Get()->Send(message);
}

blink::WebFrameWidget* TextInputClientObserver::GetWebFrameWidget() const {
  if (!render_widget_)
    return nullptr;
  return static_cast<blink::WebFrameWidget*>(render_widget_->GetWebWidget());
}

blink::WebLocalFrame* TextInputClientObserver::GetFocusedFrame() const {
  if (auto* frame_widget = GetWebFrameWidget()) {
    blink::WebLocalFrame* local_root = frame_widget->LocalRoot();
    blink::WebLocalFrame* focused = local_root->View()->FocusedFrame();
    return focused->LocalRoot() == local_root ? focused : nullptr;
  }
  return nullptr;
}

#if BUILDFLAG(ENABLE_PLUGINS)
PepperPluginInstanceImpl* TextInputClientObserver::GetFocusedPepperPlugin()
    const {
  blink::WebLocalFrame* frame = GetFocusedFrame();
  if (!frame)
    return nullptr;
  return RenderFrameImpl::FromWebFrame(frame)->focused_pepper_plugin();
}
#endif

void TextInputClientObserver::OnStringAtPoint(gfx::Point point) {
  blink::WebPoint baseline_point;
  NSAttributedString* string = nil;

  if (auto* frame_widget = GetWebFrameWidget()) {
    string = blink::WebSubstringUtil::AttributedWordAtPoint(frame_widget, point,
                                                            baseline_point);
  }

  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringAtPoint(
      MSG_ROUTING_NONE, *encoded.get(), baseline_point));
}

void TextInputClientObserver::OnCharacterIndexForPoint(gfx::Point point) {
  blink::WebPoint web_point(point);
  uint32_t index = 0U;
  if (auto* frame = GetFocusedFrame())
    index = static_cast<uint32_t>(frame->CharacterIndexForPoint(web_point));

  Send(new TextInputClientReplyMsg_GotCharacterIndexForPoint(MSG_ROUTING_NONE,
                                                             index));
}

void TextInputClientObserver::OnFirstRectForCharacterRange(gfx::Range range) {
  gfx::Rect rect;
#if BUILDFLAG(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_plugin = GetFocusedPepperPlugin();
  if (focused_plugin) {
    rect = focused_plugin->GetCaretBounds();
  } else
#endif
  {
    blink::WebLocalFrame* frame = GetFocusedFrame();
    // TODO(yabinh): Null check should not be necessary.
    // See crbug.com/304341
    if (frame) {
      blink::WebRect web_rect;
      // When request range is invalid we will try to obtain it from current
      // frame selection. The fallback value will be 0.
      uint32_t start = range.IsValid() ? range.start()
                                       : GetCurrentCursorPositionInFrame(frame);
      frame->FirstRectForCharacterRange(start, range.length(), web_rect);
      rect = web_rect;
    }
  }
  Send(
      new TextInputClientReplyMsg_GotFirstRectForRange(MSG_ROUTING_NONE, rect));
}

void TextInputClientObserver::OnStringForRange(gfx::Range range) {
  blink::WebPoint baseline_point;
  NSAttributedString* string = nil;
  blink::WebLocalFrame* frame = GetFocusedFrame();
  // TODO(yabinh): Null check should not be necessary.
  // See crbug.com/304341
  if (frame) {
    string = blink::WebSubstringUtil::AttributedSubstringInRange(
        frame, range.start(), range.length(), &baseline_point);
  }
  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringForRange(
      MSG_ROUTING_NONE, *encoded.get(), baseline_point));
}

}  // namespace content
