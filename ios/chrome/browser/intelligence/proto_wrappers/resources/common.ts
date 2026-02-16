// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common things used by the page context extraction scripts.

import {registerChildFrame} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';

export const NONCE_ATTR = 'data-__gCrWeb-annotatedPageContent-processed';

// Matches kMaximumParsingRecursionDepth in
// ios/web/js_messaging/web_view_js_utils.mm, which is effectively the max
// depth of the response payload json object.
export const MAX_APC_RESPONSE_DEPTH = 200;

// Maximum depth that we expect for an APC node object. This is just an
// estimation used to calculate the maximal depth for nesting APC nodes.
// Deeper apc nodes are allowed but may exceed MAX_APC_RESPONSE_DEPTH when
// inserted in the APC tree which information may get truncated when parsed on
// the native side, wasting message payload.
//
// Example of a single node structure:
// {
//    // Level 1: PageContentNode
//    "contentAttributes": {
//      // Level 2: PageContentAttributes
//      "formControlData": {
//        // Level 3: PageContentFormControlData
//        "selectOptions": [
//          // Level 4: PageContentSelectOption[]
//          {
//            // Level 5: PageContentSelectOption
//            "value": "..."
//          }
//        ]
//      }
//    },
//    // "childrenNodes": [...] (Excluded from this constant calculation)
// }
//
// We see here that a single node can have a depth of 5. We double this value
// to be safe and use 10 as the maximum depth for a single node.
export const MAX_APC_NODE_DEPTH = 10;

// Cost in terms of depth for each APC node that has children and forms a tree
// of nodes. Parsing the list object costs 1 then parsing the child nodes
// within the list costs 1, which gives a total cost of 2.
export const APC_NODE_DEPTH_COST = 2;


/**
 * Returns the remote frame token for the remote `frame` element and triggers
 * a registration.
 *
 * @param frame The iframe element to get the token for.
 * @return The unique remote frame token string.
 */
export function getRemoteFrameRemoteToken(frame: HTMLIFrameElement): string {
  return registerChildFrame(frame);
}
