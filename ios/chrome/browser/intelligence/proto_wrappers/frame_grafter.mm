// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/not_fatal_until.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

// TODO(crbug.com/458081684): Move to using the ios/web frame id system.

namespace {

// Merges the content from `content` into the `placeholder`.
//
// Uses `std::move` to transfer ownership of the content tree. This is critical
// because `content.content` may contain nested placeholders (pointers to
// specific nodes within the tree) that are registered in `FrameGrafter`.
// Using `CopyFrom` would create new objects at new addresses, invalidating
// those pointers and causing use-after-free crashes when resolving nested
// frames.
void MergeContent(optimization_guide::proto::ContentNode* placeholder,
                  FrameGrafter::FrameContent&& frame_content,
                  autofill::RemoteFrameToken document_id,
                  FrameGrafter* grafter = nullptr) {
  if (placeholder->content_attributes().attribute_type() ==
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    if (frame_content.content.content_attributes()
            .iframe_data()
            .has_redacted_frame_metadata()) {
      if (grafter) {
        grafter->RecordCrossSiteIframePlaceholder(*placeholder);
      }
      placeholder->mutable_content_attributes()
          ->mutable_iframe_data()
          ->mutable_redacted_frame_metadata()
          ->CopyFrom(frame_content.content.content_attributes()
                         .iframe_data()
                         .redacted_frame_metadata());
    } else {
      // Rich Extraction: The placeholder is already assigned as an
      // iframe, this means that the data in the `placeholder` is already
      // partially set so do a partial merge in this case. The iframe content
      // tree needs to be added as a child of the attributed iframe ContentNode.
      // Content starts from the page root (like for the main frame).
      optimization_guide::proto::FrameData* frame_data =
          placeholder->mutable_content_attributes()
              ->mutable_iframe_data()
              ->mutable_frame_data();
      frame_data->Swap(&frame_content.frame_data);
      // Set the document identifier here because it is not available in the
      // frame data extracted for the entire page which is the case for
      // cross-origin frames that require grafting.
      frame_data->mutable_document_identifier()->set_serialized_token(
          document_id.ToString());
      *placeholder->add_children_nodes() = std::move(frame_content.content);
    }

  } else {
    // Light Extraction: The placeholder doesn't hold any partial data,
    // it means that the whole ContentNode for the iframe is provided from the
    // content stored in `unregistered_content_`.
    *placeholder = std::move(frame_content.content);
  }
}
}  // namespace

FrameGrafter::FrameGrafter() = default;
FrameGrafter::~FrameGrafter() = default;

void FrameGrafter::RegisterPlaceholder(
    autofill::RemoteFrameToken token,
    optimization_guide::proto::ContentNode* placeholder) {
  if (placeholders_.contains(token)) {
    // TODO(crbug.com/473793284): Add a metric for this.
    // Can't register a placeholder for the `token` more than once.
    return;
  }
  placeholders_[token] = placeholder;
}

FrameGrafter::FrameContent* FrameGrafter::DeclareContent(
    autofill::LocalFrameToken token) {
  if (unregistered_content_.contains(token)) {
    // TODO(crbug.com/473793284): Add a metric for this.
    // Can't add content for the `token` more than once.
    return nullptr;
  }
  // Cache the frame content to be fulfilled later when a placeholder is
  // registered. Is the responsibility of the caller to populate the content.
  return &unregistered_content_[token];
}

std::vector<autofill::RemoteFrameToken> FrameGrafter::GetRemoteFrames() const {
  std::vector<autofill::RemoteFrameToken> tokens;
  tokens.reserve(placeholders_.size());
  for (const auto& [token, _] : placeholders_) {
    tokens.push_back(token);
  }
  return tokens;
}

void FrameGrafter::ResolveUnregisteredContent(
    base::RepeatingCallback<std::optional<autofill::LocalFrameToken>(
        autofill::RemoteFrameToken)> mapping_lookup,
    base::RepeatingCallback<void(FrameContent unregistered)> placer,
    base::RepeatingCallback<
        void(const autofill::RemoteFrameToken& remote_token,
             optimization_guide::proto::ContentNode* unresolved)>
        unresolved_placeholder_handler) {
  // Try to fulfill placeholders by resolving remote tokens to local tokens.
  for (auto it = placeholders_.begin(); it != placeholders_.end();) {
    autofill::RemoteFrameToken remote_token = it->first;
    std::optional<autofill::LocalFrameToken> local_token =
        mapping_lookup.Run(remote_token);

    if (local_token) {
      if (auto content_it = unregistered_content_.find(*local_token);
          content_it != unregistered_content_.end()) {
        // Fulfill the placeholder.
        MergeContent(it->second, std::move(content_it->second), remote_token,
                     this);
        unregistered_content_.erase(content_it);
        it = placeholders_.erase(it);
        continue;
      }
    }
    ++it;
  }

  for (auto& [remote_token, placeholder] : placeholders_) {
    unresolved_placeholder_handler.Run(remote_token, placeholder);
  }

  // TODO(crbug.com/473796618): Add a metric for when content has to be placed.
  // Place the remaining unregistered content using the `placer`.
  for (auto& [_, o] : unregistered_content_) {
    placer.Run(std::move(o));
  }
  unregistered_content_.clear();
  placeholders_.clear();
}

namespace {

void TraverseAndCollectFormControlRedactionBoxes(
    const optimization_guide::proto::ContentNode& node,
    CGPoint accumulated_offset,
    std::vector<RedactionBoxEntry>& redaction_boxes) {
  const auto& attributes = node.content_attributes();
  const auto& geometry = attributes.geometry();

  CGPoint next_offset = accumulated_offset;
  if (attributes.has_form_control_data()) {
    const auto& form_control = attributes.form_control_data();
    if (form_control.redaction_decision() !=
            optimization_guide::proto::
                REDACTION_DECISION_NO_REDACTION_NECESSARY &&
        geometry.has_visible_bounding_box()) {
      const auto& box = geometry.visible_bounding_box();
      redaction_boxes.push_back({
          .visible_box = CGRectMake(box.x() + accumulated_offset.x,
                                    box.y() + accumulated_offset.y, box.width(),
                                    box.height()),
          .decision = form_control.redaction_decision(),
      });
    }
  } else if (attributes.attribute_type() ==
             optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    if (!geometry.has_visible_bounding_box()) {
      return;
    }
    const auto& iframe_box = geometry.visible_bounding_box();
    next_offset.x += iframe_box.x();
    next_offset.y += iframe_box.y();
  }

  for (const auto& child_node : node.children_nodes()) {
    TraverseAndCollectFormControlRedactionBoxes(child_node, next_offset,
                                                redaction_boxes);
  }
}

}  // namespace

void FrameGrafter::TraverseAndCollectCrossSiteIframes(
    const optimization_guide::proto::ContentNode& node,
    CGPoint accumulated_offset,
    const std::map<const optimization_guide::proto::ContentNode*,
                   const CrossSiteIframeRecord*>& records_by_node,
    optimization_guide::proto::ScreenshotInfo* screenshot_info) {
  const auto& attributes = node.content_attributes();
  const auto& geometry = attributes.geometry();

  CGPoint next_offset = accumulated_offset;
  if (attributes.attribute_type() ==
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    if (!geometry.has_visible_bounding_box()) {
      return;
    }
    const auto& iframe_box = geometry.visible_bounding_box();

    auto it = records_by_node.find(&node);
    if (it != records_by_node.end()) {
      const auto* record = it->second;

      optimization_guide::proto::IframeInfo* iframe_info =
          screenshot_info->add_iframe_info();
      auto* box = iframe_info->mutable_bounding_box();
      box->set_x(iframe_box.x() + accumulated_offset.x);
      box->set_y(iframe_box.y() + accumulated_offset.y);
      box->set_width(iframe_box.width());
      box->set_height(iframe_box.height());
      box->set_is_screenshot_relative(true);
      if (!record->url.empty()) {
        iframe_info->set_url(record->url);
      }
      if (record->has_security_origin) {
        *iframe_info->mutable_security_origin() = record->security_origin;
      }
    }

    next_offset.x += iframe_box.x();
    next_offset.y += iframe_box.y();
  }

  for (const auto& child_node : node.children_nodes()) {
    TraverseAndCollectCrossSiteIframes(child_node, next_offset, records_by_node,
                                       screenshot_info);
  }
}

void FrameGrafter::RecordCrossSiteIframePlaceholder(
    const optimization_guide::proto::ContentNode& placeholder) {
  const auto& content_attributes = placeholder.content_attributes();
  if (!content_attributes.has_geometry() ||
      !content_attributes.geometry().has_visible_bounding_box()) {
    return;
  }

  CrossSiteIframeRecord record;
  record.node = &placeholder;
  if (content_attributes.has_iframe_data() &&
      content_attributes.iframe_data().has_frame_data()) {
    const auto& frame_data = content_attributes.iframe_data().frame_data();
    if (frame_data.has_url()) {
      record.url = frame_data.url();
    }
    if (frame_data.has_security_origin()) {
      record.security_origin = frame_data.security_origin();
      record.has_security_origin = true;
    }
  }
  cross_site_iframe_records_.push_back(std::move(record));
}

void FrameGrafter::PopulateScreenshotInfo(
    optimization_guide::proto::AnnotatedPageContent* apc) {
  if (!apc || cross_site_iframe_records_.empty()) {
    return;
  }

  std::map<const optimization_guide::proto::ContentNode*,
           const CrossSiteIframeRecord*>
      records_by_node;
  for (const auto& record : cross_site_iframe_records_) {
    if (record.node) {
      records_by_node[record.node] = &record;
    }
  }

  auto* screenshot_info =
      apc->mutable_gemini_in_chrome_page_metadata()->mutable_screenshot_info();
  screenshot_info->clear_iframe_info();
  if (apc->has_root_node()) {
    TraverseAndCollectCrossSiteIframes(apc->root_node(), CGPointZero,
                                       records_by_node, screenshot_info);
  }
  if (apc->has_viewport_geometry() && apc->viewport_geometry().width() > 0 &&
      apc->viewport_geometry().height() > 0) {
    screenshot_info->mutable_screenshot_size()->set_width(
        apc->viewport_geometry().width());
    screenshot_info->mutable_screenshot_size()->set_height(
        apc->viewport_geometry().height());
  }

  // Clear records to release raw pointers to placeholder nodes.
  cross_site_iframe_records_.clear();
}

void FrameGrafter::CollectFormControlRedactionBoxesFromTree(
    const optimization_guide::proto::ContentNode& root_node) {
  universal_bounding_boxes_for_redaction_.clear();
  if (!has_sensitive_fields_to_redact_) {
    return;
  }
  TraverseAndCollectFormControlRedactionBoxes(
      root_node, CGPointZero, universal_bounding_boxes_for_redaction_);
}
