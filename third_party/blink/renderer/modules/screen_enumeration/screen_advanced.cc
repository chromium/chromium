// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/screen_advanced.h"

#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/common/widget/screen_infos.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/wtf/text/string_statics.h"

namespace blink {

namespace {

const ScreenInfo& GetScreenInfo(LocalFrame& frame, int64_t display_id) {
  const auto& screen_infos = frame.GetChromeClient().GetScreenInfos(frame);
  for (const auto& screen : screen_infos.screen_infos) {
    if (screen.display_id == display_id)
      return screen;
  }
  DEFINE_STATIC_LOCAL(ScreenInfo, kEmptyScreenInfo, ());
  return kEmptyScreenInfo;
}

}  // namespace

ScreenAdvanced::ScreenAdvanced(LocalDOMWindow* window, int64_t display_id)
    : Screen(window), display_id_(display_id) {}

int ScreenAdvanced::height() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.height();
}

int ScreenAdvanced::width() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.width();
}

unsigned ScreenAdvanced::colorDepth() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).depth;
}

unsigned ScreenAdvanced::pixelDepth() const {
  return colorDepth();
}

int ScreenAdvanced::availLeft() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.x();
}

int ScreenAdvanced::availTop() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.y();
}

int ScreenAdvanced::availHeight() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.height();
}

int ScreenAdvanced::availWidth() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).available_rect.width();
}

bool ScreenAdvanced::isExtended() const {
  if (!DomWindow())
    return false;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).is_extended;
}

int ScreenAdvanced::left() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.x();
}

int ScreenAdvanced::top() const {
  if (!DomWindow())
    return 0;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).rect.y();
}

bool ScreenAdvanced::isPrimary() const {
  if (!DomWindow())
    return false;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).is_primary;
}

bool ScreenAdvanced::isInternal() const {
  if (!DomWindow())
    return false;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).is_internal;
}

float ScreenAdvanced::devicePixelRatio() const {
  if (!DomWindow())
    return 0.f;
  LocalFrame* frame = DomWindow()->GetFrame();
  return GetScreenInfo(*frame, display_id_).device_scale_factor;
}

const String& ScreenAdvanced::id() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return g_empty_string;
}

Vector<String> ScreenAdvanced::pointerTypes() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return Vector<String>();
}

const String& ScreenAdvanced::label() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return g_empty_string;
}

int64_t ScreenAdvanced::DisplayId() const {
  return display_id_;
}

}  // namespace blink
