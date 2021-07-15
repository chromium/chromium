/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/frame/screen.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "ui/display/screen_info.h"

namespace blink {

namespace {

const display::ScreenInfo& GetScreenInfo(LocalFrame& frame) {
  return frame.GetChromeClient().GetScreenInfo(frame);
}

}  // namespace

Screen::Screen(LocalDOMWindow* window) : ExecutionContextClient(window) {}

int Screen::height() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  const display::ScreenInfo& screen_info = GetScreenInfo(*frame);
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(screen_info.rect.height() * screen_info.device_scale_factor));
  }
  return screen_info.rect.height();
}

int Screen::width() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  const display::ScreenInfo& screen_info = GetScreenInfo(*frame);
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(screen_info.rect.width() * screen_info.device_scale_factor));
  }
  return screen_info.rect.width();
}

unsigned Screen::colorDepth() const {
  if (!DomWindow())
    return 0;
  return static_cast<unsigned>(GetScreenInfo(*DomWindow()->GetFrame()).depth);
}

unsigned Screen::pixelDepth() const {
  return colorDepth();
}

int Screen::availLeft() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  const display::ScreenInfo& screen_info = GetScreenInfo(*frame);
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(lroundf(screen_info.available_rect.x() *
                                    screen_info.device_scale_factor));
  }
  return static_cast<int>(screen_info.available_rect.x());
}

int Screen::availTop() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  const display::ScreenInfo& screen_info = GetScreenInfo(*frame);
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(lroundf(screen_info.available_rect.y() *
                                    screen_info.device_scale_factor));
  }
  return static_cast<int>(screen_info.available_rect.y());
}

int Screen::availHeight() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  const display::ScreenInfo& screen_info = GetScreenInfo(*frame);
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(lroundf(screen_info.available_rect.height() *
                                    screen_info.device_scale_factor));
  }
  return screen_info.available_rect.height();
}

int Screen::availWidth() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  const display::ScreenInfo& screen_info = GetScreenInfo(*frame);
  if (frame->GetSettings()->GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(lroundf(screen_info.available_rect.width() *
                                    screen_info.device_scale_factor));
  }
  return screen_info.available_rect.width();
}

void Screen::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  Supplementable<Screen>::Trace(visitor);
}

const WTF::AtomicString& Screen::InterfaceName() const {
  return event_target_names::kScreen;
}

ExecutionContext* Screen::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

bool Screen::isExtended() const {
  if (!DomWindow())
    return false;
  LocalFrame* frame = DomWindow()->GetFrame();

  auto* context = GetExecutionContext();
  if (!context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kWindowPlacement)) {
    return false;
  }

  return GetScreenInfo(*frame).is_extended;
}

int64_t Screen::DisplayId() const {
  if (!DomWindow())
    return kInvalidDisplayId;
  return GetScreenInfo(*DomWindow()->GetFrame()).display_id;
}

}  // namespace blink
