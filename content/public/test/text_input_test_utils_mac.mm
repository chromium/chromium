// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/text_input_test_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/test/test_utils.h"
#include "ui/base/mojom/attributed_string.mojom.h"

namespace content {

TextInputTestLocalFrame::TextInputTestLocalFrame() = default;

TextInputTestLocalFrame::~TextInputTestLocalFrame() = default;

void TextInputTestLocalFrame::SetUp(
    content::RenderFrameHost* render_frame_host) {
  local_frame_ = std::move(
      static_cast<RenderFrameHostImpl*>(render_frame_host)->local_frame_);
  FakeLocalFrame::Init(render_frame_host->GetRemoteAssociatedInterfaces());
}

void TextInputTestLocalFrame::WaitForGetStringForRange() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TextInputTestLocalFrame::SetStringForRangeCallback(
    base::RepeatingClosure callback) {
  string_for_range_callback_ = std::move(callback);
}

void TextInputTestLocalFrame::GetStringForRange(
    const gfx::Range& range,
    GetStringForRangeCallback callback) {
  local_frame_->GetStringForRange(
      range,
      base::BindOnce(
          [](TextInputTestLocalFrame* frame, GetStringForRangeCallback callback,
             ui::mojom::AttributedStringPtr attributed_string,
             const gfx::Point& point) {
            // If |string_for_range_callback_| is set, it should be called
            // first.
            if (!frame->string_for_range_callback_.is_null())
              std::move(frame->string_for_range_callback_).Run();

            // Updates the string from the range and calls |callback|.
            frame->SetStringFromRange(
                base::UTF16ToUTF8(attributed_string ? attributed_string->string
                                                    : std::u16string()));
            std::move(callback).Run(std::move(attributed_string), gfx::Point());

            // Calls |quit_closure_|.
            if (frame->quit_closure_)
              std::move(frame->quit_closure_).Run();
          },
          base::Unretained(this), std::move(callback)));
}

void AskForLookUpDictionaryForRange(RenderWidgetHostView* tab_view,
                                    const gfx::Range& range) {
  RenderWidgetHostViewMac* tab_view_mac =
      static_cast<RenderWidgetHostViewMac*>(tab_view);
  tab_view_mac->LookUpDictionaryOverlayFromRange(range);
}

}  // namespace content
