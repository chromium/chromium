// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_SCREEN_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_SCREEN_H_

#include <cstdint>
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class CrosWindowManagement;

class CrosScreen : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CrosScreen(CrosWindowManagement* manager,
             mojom::blink::CrosScreenInfoPtr screen);

  void Trace(Visitor*) const override;

  int32_t availWidth() { return screen_->work_area.width(); }

  int32_t availHeight() { return screen_->work_area.height(); }

  int32_t width() { return screen_->bounds.width(); }

  int32_t height() { return screen_->bounds.height(); }

  int32_t left() { return screen_->bounds.x(); }

  int32_t top() { return screen_->bounds.y(); }

  bool isPrimary() { return screen_->is_primary; }

 private:
  Member<CrosWindowManagement> window_management_;

  mojom::blink::CrosScreenInfoPtr screen_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_SCREEN_H_
