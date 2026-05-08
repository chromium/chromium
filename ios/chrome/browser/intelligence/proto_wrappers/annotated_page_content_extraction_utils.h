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
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/autofill_data_extraction_utils.h"
#import "url/origin.h"

namespace web {
class WebState;
}

namespace autofill {
class ChildFrameRegistrar;
}

class FrameGrafter;

// Information about the focus status of a frame, collected during the initial
// parsing of the annotated page content JSON payload.
struct FrameFocusInfo {
  // The identifier (remote frame token string) of the frame. This may be empty
  // initially for cross-origin frames until they are resolved by the grafter.
  std::string document_id;
  // The local frame token of the frame. This is primarily used to resolve the
  // `document_id` for cross-origin frames during the final assembly phase.
  std::optional<autofill::LocalFrameToken> local_token;
};

// Result of populating frame data, containing status information like focus.
struct FrameDataNodeResult {
  bool is_focused = false;
};

// Util functions to populate the APC proto nodes out of the annotated page
// content objects extracted from the renderer. Basically, maps from JSON to
// proto.

// Populates the given `destination_node` from the `node_content` JSON
// dictionary content representing the annotated page content extracted from the
// renderer. It recursively processes children, handles nodes with attributes
// like text, anchors, images, and roles, and registers placeholders for remote
// iframes if grafting is enabled. Uses `on_frame_extracted` to notify the
// caller when a frame with its focused status and document ID is extracted.
void PopulateAPCNodeFromContentTree(
    const base::DictValue& node_content,
    const url::Origin& origin,
    FrameGrafter& grafter,
    AutofillExtractionContext* context,
    optimization_guide::proto::ContentNode* destination_node,
    const base::RepeatingCallback<void(bool is_focused,
                                       const std::string& document_id)>&
        on_frame_extracted);

// Populates `destination_frame_data_node` from the
// `frame_data_content` (JSON dict) representing the frame data extracted
// from the renderer. Returns a FrameDataNodeResult containing the focus state.
FrameDataNodeResult PopulateFrameDataNode(
    const base::DictValue& frame_data_content,
    const url::Origin& origin,
    optimization_guide::proto::FrameData* destination_frame_data_node);

// Populate a PageInteractionInfo node from the `value` object from the
// renderer.
void PopulatePageInteractionInfoNode(
    const base::DictValue& page_interaction_info_content,
    optimization_guide::proto::PageInteractionInfo*
        destination_page_interaction_info_node);

// Populate the BoundingRect node for the viewport geometry from the
// `viewport_geometry_content` object from the renderer.
void PopulateViewportGeometryNode(
    const base::DictValue& viewport_geometry_content,
    optimization_guide::proto::BoundingRect*
        destination_viewport_geometry_node);

// Populate Autofill Address and Credit Card information from the profile.
void PopulateAutofillInformation(
    web::WebState* web_state,
    optimization_guide::proto::AutofillInformation* autofill_information);

// Resolves all cross-site frame content by using `registrar` to map
// placeholders to their content in `grafter`, and places them as children of
// `apc`'s root node.
void ResolveCrossSiteFrameContent(
    FrameGrafter& grafter,
    autofill::ChildFrameRegistrar* registrar,
    optimization_guide::proto::AnnotatedPageContent* apc);

// Resolves the focused frame by mapping local tokens to remote tokens and
// sets the focused_frame in `apc`.
void ResolveFocusedFrame(
    std::vector<FrameFocusInfo>& focused_frame_infos,
    const std::vector<autofill::RemoteFrameToken>& remote_frames,
    autofill::ChildFrameRegistrar* registrar,
    optimization_guide::proto::AnnotatedPageContent* apc);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_ANNOTATED_PAGE_CONTENT_EXTRACTION_UTILS_H_
