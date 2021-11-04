// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/screen_detailed.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/wtf/text/string_statics.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"

namespace blink {

namespace {

const display::ScreenInfo& GetScreenInfo(LocalFrame& frame,
                                         int64_t display_id) {
  const auto& screen_infos = frame.GetChromeClient().GetScreenInfos(frame);
  for (const auto& screen : screen_infos.screen_infos) {
    if (screen.display_id == display_id)
      return screen;
  }
  DEFINE_STATIC_LOCAL(display::ScreenInfo, kEmptyScreenInfo, ());
  return kEmptyScreenInfo;
}

}  // namespace

ScreenDetailed::ScreenDetailed(LocalDOMWindow* window, int64_t display_id)
    : Screen(window), display_id_(display_id) {}

int ScreenDetailed::height() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.height();
}

int ScreenDetailed::width() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.width();
}

unsigned ScreenDetailed::colorDepth() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).depth;
}

unsigned ScreenDetailed::pixelDepth() const {
  return colorDepth();
}

int ScreenDetailed::availLeft() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.x();
}

int ScreenDetailed::availTop() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.y();
}

int ScreenDetailed::availHeight() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.height();
}

int ScreenDetailed::availWidth() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.width();
}

bool ScreenDetailed::isExtended() const {
  if (!DomWindow())
    return false;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).is_extended;
}

int ScreenDetailed::left() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.x();
}

int ScreenDetailed::top() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.y();
}

bool ScreenDetailed::isPrimary() const {
  if (!DomWindow())
    return false;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).is_primary;
}

bool ScreenDetailed::isInternal() const {
  if (!DomWindow())
    return false;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).is_internal;
}

float ScreenDetailed::devicePixelRatio() const {
  if (!DomWindow())
    return 0.f;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).device_scale_factor;
}

const String& ScreenDetailed::label() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return g_empty_string;
}

int64_t ScreenDetailed::DisplayId() const {
  return display_id_;
}

}  // namespace blink
