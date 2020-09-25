// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_UTIL_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_UTIL_H_

#include "extensions/common/api/automation.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"

namespace extensions {

bool IsEventTypeHandledByAXEventGenerator(ax::mojom::Event event_type);

bool ShouldIgnoreGeneratedEvent(ui::AXEventGenerator::Event event_type);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_UTIL_H_
