// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/metadata.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"

namespace blink {

ScriptValue Metadata::modificationTime(ScriptState* script_state) const {
  // The test FileSystemProviderApiTest.GetMetadata assumes
  // metadata.modificationTime returns a Date object with an invalid state.
  // Passing Time::Max() here creates such a Date object.
  base::Time time =
      platform_metadata_.modification_time.value_or(base::Time::Max());
  return ScriptValue(script_state->GetIsolate(),
                     ToV8Traits<IDLNullable<IDLDate>>::ToV8(
                         script_state, std::optional<base::Time>(time)));
}

}  // namespace blink
