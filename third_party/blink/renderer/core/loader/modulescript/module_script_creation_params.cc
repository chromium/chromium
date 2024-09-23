// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"

namespace blink {

String ModuleScriptCreationParams::ModuleTypeToString(
    const ModuleType module_type) {
  switch (module_type) {
    case ModuleType::kJavaScript:
      return "JavaScript";
    case ModuleType::kJSON:
      return "JSON";
    case ModuleType::kCSS:
      return "CSS";
    case ModuleType::kInvalid:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

}  // namespace blink
