// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/not_fatal_until.h"

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

optimization_guide::proto::ContentNode* FrameGrafter::DeclareContent(
    autofill::LocalFrameToken token) {
  if (unregistered_content_.contains(token)) {
    // TODO(crbug.com/473793284): Add a metric for this.
    // Can't add content for the `token` more than once.
    return nullptr;
  }
  // Cache the frame content to be fulfilled later when a placeholder is
  // registered.
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
    base::RepeatingCallback<
        void(optimization_guide::proto::ContentNode unregistered)> placer) {
  // Try to fulfill placeholders by resolving remote tokens to local tokens.
  for (auto it = placeholders_.begin(); it != placeholders_.end();) {
    std::optional<autofill::LocalFrameToken> local_token =
        mapping_lookup.Run(it->first);

    if (local_token) {
      if (auto content_it = unregistered_content_.find(*local_token);
          content_it != unregistered_content_.end()) {
        // Fulfill the placeholder.
        *it->second = std::move(content_it->second);
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
