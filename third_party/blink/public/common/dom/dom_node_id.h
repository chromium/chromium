// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_DOM_DOM_NODE_ID_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_DOM_DOM_NODE_ID_H_

#include "base/types/id_type.h"

namespace blink {

// Uniquely identifies a DOM node.
// Note: A DOMNodeIdType is unique only within a renderer process.
// DOMNodeIdType rather than just "DOMNodeId" to distinguish it from the
// internally used DOMNodeId which is an `int`.
// TODO(crbug.com/532946448): Replace usage of the Blink-internal type with this
// one.
using DOMNodeIdType = base::IdType32<class DOMNodeIdTypeTag>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DOM_DOM_NODE_ID_H_
