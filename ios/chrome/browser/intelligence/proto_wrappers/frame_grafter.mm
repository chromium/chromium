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
                  autofill::RemoteFrameToken document_id) {
  if (placeholder->content_attributes().attribute_type() ==
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    // Rich Extraction:  The placeholder is already assigned as an
    // iframe, this means that the data in the `placeholder` is already
    // partially set so do a partial merge in this case. The iframe content tree
    // needs to be added as a child of the attributed iframe ContentNode.
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

  } else {
    // Light Extraction: The placeholder doesn't hold any partial data,
    // it means that the whole ContentNode for the iframe is provided from the
    // content stored in `unregistered_content_.
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
    base::RepeatingCallback<void(FrameContent unregistered)> placer) {
  // Try to fulfill placeholders by resolving remote tokens to local tokens.
  for (auto it = placeholders_.begin(); it != placeholders_.end();) {
    autofill::RemoteFrameToken remote_token = it->first;
    std::optional<autofill::LocalFrameToken> local_token =
        mapping_lookup.Run(remote_token);

    if (local_token) {
      if (auto content_it = unregistered_content_.find(*local_token);
          content_it != unregistered_content_.end()) {
        // Fulfill the placeholder.
        MergeContent(it->second, std::move(content_it->second), remote_token);
        unregistered_content_.erase(content_it);
        it = placeholders_.erase(it);
        continue;
      }
    }
    ++it;
  }

  // TODO(crbug.com/473796618): Add a metric for when content has to be placed.
  // Place the remaining unregistered content using the `placer`.
  for (auto& [_, o] : unregistered_content_) {
    placer.Run(std::move(o));
  }
  unregistered_content_.clear();
  placeholders_.clear();
}
