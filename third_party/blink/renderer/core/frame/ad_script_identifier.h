// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_SCRIPT_IDENTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_SCRIPT_IDENTIFIER_H_

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-inspector.h"

namespace blink {

// Used to uniquely identify ad script on the stack.
struct CORE_EXPORT AdScriptIdentifier {
  static constexpr int kEmptyId = -1;

  // Creates an empty/unspecified identifier.
  AdScriptIdentifier();

  AdScriptIdentifier(const v8_inspector::V8DebuggerId& context_id,
                     int id,
                     String name);

  bool operator==(const AdScriptIdentifier& other) const;

  // v8's debugging id for the v8::Context.
  v8_inspector::V8DebuggerId context_id;

  // The script's v8 identifier.
  int id;

  // The script's url (or generated name based on id if inline script). This is
  // a convenience field useful for intervention messages and debugging, as only
  // `context_id` and `id` are needed to identify the script. This field is not
  // used for equality and hash comparisons.
  String name;
};

template <>
struct HashTraits<AdScriptIdentifier> : GenericHashTraits<AdScriptIdentifier> {
  static unsigned GetHash(const AdScriptIdentifier& script_id) {
    std::pair<int64_t, int64_t> p = script_id.context_id.pair();
    int64_t arr[] = {p.first, p.second, script_id.id};
    return base::FastHash(base::as_byte_span(arr));
  }

  static void ConstructDeletedValue(AdScriptIdentifier& script_id) {
    script_id = AdScriptIdentifier();
  }

  static bool IsDeletedValue(const AdScriptIdentifier& script_id) {
    return script_id.id == AdScriptIdentifier::kEmptyId;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_SCRIPT_IDENTIFIER_H_
