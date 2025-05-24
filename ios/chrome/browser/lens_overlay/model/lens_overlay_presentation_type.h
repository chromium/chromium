// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_PRESENTATION_TYPE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_PRESENTATION_TYPE_H_

#import <UIKit/UIKit.h>

namespace lens {

// The possible strategies for presenting the overlay container.
enum class ContainerPresentationType {
  // The Lens Overlay covers the entire screen real estate.
  kFullscreenCover = 0,
  // The Lens Overlay overlaps the web content area.
  kContentAreaCover = 1,
};

// The possible strategies for presenting the results page.
enum class ResultPagePresentationType {
  // The results page is presented in a edge-attached bottom sheet.
  kEdgeAttachedBottomSheet = 0,
  // The results are displayed in a side panel.
  kSidePanel = 1,
};

// Deducts the required container presentation for the `UITraitEnvironment`
// conforming entitygit di.
ContainerPresentationType ContainerPresentationFor(
    id<UITraitEnvironment> environment);

// Deducts the required result age presentation for the `UITraitEnvironment`
// conforming entity.
ResultPagePresentationType ResultPagePresentationFor(
    id<UITraitEnvironment> environment);

}  // namespace lens

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_PRESENTATION_TYPE_H_
