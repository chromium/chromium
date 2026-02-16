// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FRAME_GRAFTER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FRAME_GRAFTER_H_

#import <map>
#import <vector>

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

// TODO(crbug.com/458081684): Move away from all autofill dependencies once
// the migration in ios/web is done for frame registration.

// Grafter that receives placeholders for frame content and frame content to
// construct the frame tree.
//
// The algorithm works by matching "placeholders" (locations in the frame tree
// where a child frame should go) with the actual "content" of those frames.
// - When a parent frame is processed, it registers a placeholder for its child
//   frames using their `RemoteFrameToken` via `RegisterPlaceholder`.
// - When a child frame is processed, it adds its content associated with its
//   `LocalFrameToken` via `AddContent`.
// - Call ResolveUnregisteredContent() when it is determined that (1)
//   all the frames were processed, (2) their content extracted, and (3) frame
//   registration was completed (where RemoteFrameTokens are mapped to their
//   corresponding LocalFrameToken).
//
// See "Keeping the iframes’ tree hierarchy" in http://shortn/_YOb7kQCI0i.
class FrameGrafter {
 public:
  struct FrameContent {
    optimization_guide::proto::ContentNode content;
    optimization_guide::proto::FrameData frame_data;
  };

  FrameGrafter();
  ~FrameGrafter();

  // Registers a placeholder for a frame's content using its
  // `RemoteFrameToken`. There can't be more than 1 placeholder per `token`, so
  // double registrations are ignored. Make sure that the `placeholder` remains
  // valid until calling ResolveUnregisteredContent().
  void RegisterPlaceholder(autofill::RemoteFrameToken token,
                           optimization_guide::proto::ContentNode* placeholder);

  // Declares content for a frame identified by `token`. The grafter creates and
  // owns the content. Returns a pointer to the created content node if `token`
  // is declared for the first time, and nullptr otherwise because content can't
  // be declared nor handled more than once. The returned pointer is owned by
  // the FrameGrafter and remains valid until ResolveUnregisteredContent() is
  // called.
  FrameContent* DeclareContent(autofill::LocalFrameToken token);

  // Returns the remote frame tokens for all registered placeholders.
  std::vector<autofill::RemoteFrameToken> GetRemoteFrames() const;

  // Resolves all unregistered content by using `mapping_lookup` to map
  // placeholders to their content (RemoteFrameToken => LocalFrameToken
  // mapping). The `placer` is used to complete grafting of content that doesn't
  // match a placeholder. Call this function when it is determined that (1) all
  // the frames were processed, (2) their content extracted, and (3) frame
  // registration was completed.
  void ResolveUnregisteredContent(
      base::RepeatingCallback<std::optional<autofill::LocalFrameToken>(
          autofill::RemoteFrameToken)> mapping_lookup,
      base::RepeatingCallback<void(FrameContent unregistered)> placer);

 private:
  // Frame content that wasn't claimed yet (unregistered).
  std::map<autofill::LocalFrameToken, FrameContent> unregistered_content_;
  // Placeholders waiting for content.
  std::map<autofill::RemoteFrameToken,
           raw_ptr<optimization_guide::proto::ContentNode>>
      placeholders_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FRAME_GRAFTER_H_
