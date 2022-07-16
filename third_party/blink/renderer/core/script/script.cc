// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/script.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/script_type_names.h"

namespace blink {

absl::optional<mojom::blink::ScriptType> Script::ParseScriptType(
    const String& script_type) {
  if (script_type == script_type_names::kClassic)
    return mojom::blink::ScriptType::kClassic;
  if (script_type == script_type_names::kModule)
    return mojom::blink::ScriptType::kModule;
  return absl::nullopt;
}

}  // namespace blink
