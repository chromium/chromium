// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/backgrounded_scene_rendering/backgrounded_scene_rendering_api.h"

namespace ios::provider {

bool IsBackgroundedSceneRenderingSupported(
    const BackgroundedSceneRenderingConfig& config) {
  return false;
}

bool PrepareWebViewConfigurationForBackgroundedSceneRendering(
    WKWebViewConfiguration* configuration,
    const BackgroundedSceneRenderingConfig& config) {
  return false;
}

void StartBackgroundedSceneRendering(
    WKWebView* web_view,
    const BackgroundedSceneRenderingConfig& config) {}

void BackgroundedSceneRenderingDidCommitNavigation(WKWebView* web_view) {}

void StopBackgroundedSceneRendering(WKWebView* web_view) {}

bool TakeBackgroundedSceneCustomSnapshot(
    WKWebView* web_view,
    WKSnapshotConfiguration* configuration,
    const BackgroundedSceneRenderingConfig& config,
    void (^completion)(UIImage* snapshot, NSError* error)) {
  return false;
}

}  // namespace ios::provider
