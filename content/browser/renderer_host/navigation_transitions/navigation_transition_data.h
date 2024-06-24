// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_DATA_H_

#include <optional>

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

// Holds the relevant information about a navigation transition. Just like the
// `NavigationEntryScreenshot`, this struct is not persistent on the
// `NavigationEntry` (i.e. can't be restored).
class NavigationTransitionData {
 public:
  NavigationTransitionData() = default;
  ~NavigationTransitionData() = default;
  NavigationTransitionData(NavigationTransitionData&&) = delete;
  NavigationTransitionData& operator=(NavigationTransitionData&&) = default;
  NavigationTransitionData(const NavigationTransitionData&) = delete;
  NavigationTransitionData& operator=(const NavigationTransitionData&) = delete;

  void SetSameDocumentNavigationEntryScreenshotToken(
      const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
          token);

  const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
  same_document_navigation_entry_screenshot_token() const {
    return same_document_navigation_entry_screenshot_token_;
  }

  void set_is_copied_from_embedder(bool is_copied_from_embedder) {
    is_copied_from_embedder_ = is_copied_from_embedder;
  }
  bool is_copied_from_embedder() const { return is_copied_from_embedder_; }

  void set_main_frame_background_color(
      const std::optional<SkColor4f>& main_frame_background_color) {
    main_frame_background_color_ = main_frame_background_color;
  }
  const std::optional<SkColor4f>& main_frame_background_color() const {
    return main_frame_background_color_;
  }

 private:
  // Whether this screenshot is supplied by the embedder.
  bool is_copied_from_embedder_ = false;

  // Used to compose a fallback screenshot when no valid screenshot available.
  std::optional<SkColor4f> main_frame_background_color_;

  // Used to map a screenshot for the last frame of this navigation entry
  // captured in Viz and sent back to the browser process. The token is set when
  // `DidCommitSameDocumentNavigation` is received in the browser process from
  // the renderer; and reset when its corresponding screenshot is received by
  // the browser process from Viz.
  std::optional<blink::SameDocNavigationScreenshotDestinationToken>
      same_document_navigation_entry_screenshot_token_;

  // TODO(https://crbug.com/40262175): We might want to move the
  // `NavigationEntryScreenshot` here as well when we make the screenshot
  // disk-persistent.
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_DATA_H_
