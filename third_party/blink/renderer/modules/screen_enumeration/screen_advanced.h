// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_SCREEN_ADVANCED_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_SCREEN_ADVANCED_H_

#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalDOMWindow;

// Interface exposing advanced per-screen information.
// https://github.com/webscreens/window-placement
class MODULES_EXPORT ScreenAdvanced final : public Screen {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ScreenAdvanced(LocalDOMWindow* window, int64_t display_id);

  // Web-exposed interface (Screen overrides):
  int height() const override;
  int width() const override;
  unsigned colorDepth() const override;
  unsigned pixelDepth() const override;
  int availLeft() const override;
  int availTop() const override;
  int availHeight() const override;
  int availWidth() const override;
  bool isExtended() const override;

  // Web-exposed interface (additional per-screen information):
  int left() const;
  int top() const;
  bool isPrimary() const;
  bool isInternal() const;
  float devicePixelRatio() const;
  const String& id() const;
  Vector<String> pointerTypes() const;
  const String& label() const;

  // Not web-exposed; for internal usage only (see Screen).
  int64_t DisplayId() const override;
  void UpdateDisplayId(int64_t display_id) { display_id_ = display_id; }

 private:
  int64_t display_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_SCREEN_ADVANCED_H_
