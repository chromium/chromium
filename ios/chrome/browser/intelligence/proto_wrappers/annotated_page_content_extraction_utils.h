// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_ANNOTATED_PAGE_CONTENT_EXTRACTION_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_ANNOTATED_PAGE_CONTENT_EXTRACTION_UTILS_H_

#import <optional>
#import <string>
#import <vector>

#import "base/functional/callback_forward.h"
#import "base/values.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "url/origin.h"

class FrameGrafter;

// Util functions to populate the APC proto nodes out of the annotated page
// content objects extracted from the renderer. Basically, maps from JSON to
// proto.

// Populates the given `destination_node` from the `node_content` JSON
// dictionary content representing the annotated page content extracted from the
// renderer. It recursively processes children, handles nodes with attributes
// like text, anchors, images, and roles, and registers placeholders for remote
// iframes if grafting is enabled.
void PopulateAPCNodeFromContentTree(
    const base::DictValue& node_content,
    const url::Origin& origin,
    FrameGrafter& grafter,
    optimization_guide::proto::ContentNode* destination_node);

// Populates `destination_frame_data_node` from the
// `frame_data_content` (JSON dict) representing the frame data extracted
// from the renderer.
void PopulateFrameDataNode(
    const base::DictValue& frame_data_content,
    const url::Origin& origin,
    optimization_guide::proto::FrameData* destination_frame_data_node);

// Populate a PageInteractionInfo node from the `value` object from the
// renderer.
void PopulatePageInteractionInfoNode(
    const base::DictValue& page_interaction_info_content,
    optimization_guide::proto::PageInteractionInfo*
        destination_page_interaction_info_node);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_ANNOTATED_PAGE_CONTENT_EXTRACTION_UTILS_H_
