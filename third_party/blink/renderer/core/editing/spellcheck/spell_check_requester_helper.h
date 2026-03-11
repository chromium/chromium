// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_SPELL_CHECK_REQUESTER_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_SPELL_CHECK_REQUESTER_HELPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"

namespace blink {

class Document;

// Returns true if the spelling markers info should be sent to the spell checker
// service. This is required
CORE_EXPORT bool ShouldSendSpellingMarkersInfo();

// Returns the spelling and grammar markers intersecting the given range.
CORE_EXPORT DocumentMarkerVector
GetSpellingMarkersFromRange(const Document& document,
                            ContainerNode* container,
                            const EphemeralRange& range);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_SPELL_CHECK_REQUESTER_HELPER_H_
