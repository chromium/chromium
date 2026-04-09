// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_CONSTANTS_H_

namespace actor {

// Mirrored from the Desktop analogue at chrome/common/actor/actor_constants.h.
//
// 0 is not a valid DOM Node ID so it is used to indicate targeting the
// root/viewport. See the code that assigns DOM node ids at
// ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.ts.
inline constexpr int kRootElementDomNodeId = 0;

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_CONSTANTS_H_
