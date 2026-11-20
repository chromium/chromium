// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BACKGROUNDED_SCENE_RENDERING_BACKGROUNDED_SCENE_RENDERING_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BACKGROUNDED_SCENE_RENDERING_BACKGROUNDED_SCENE_RENDERING_API_H_

#import <Foundation/Foundation.h>

#import <string>

#import "base/time/time.h"

@class UIImage;
@class WKSnapshotConfiguration;
@class WKWebView;
@class WKWebViewConfiguration;

// Provider API that keeps a `WKWebView` rendering, and lets it be snapshotted,
// while its `UIWindowScene` is in the background.

namespace ios::provider {

// Tuning for backgrounded scene rendering.
struct BackgroundedSceneRenderingConfig {
  // Whether web views keep rendering while their scene is in the background.
  // Does not affect custom snapshots.
  bool background_rendering_enabled = true;
  // Enables `TakeBackgroundedSceneCustomSnapshot()`, independently of
  // `background_rendering_enabled`.
  bool custom_snapshot_enabled = true;
  // How often a started web view is refreshed. Non-positive values fall back
  // to the default.
  base::TimeDelta refresh_interval = base::Seconds(90);
  // Whether refreshes also run while the scene is in the foreground.
  bool active_in_foreground = false;
  // Comma-separated exact iOS versions on which this API is disabled.
  std::string blocked_ios_versions;
};

// Returns whether web views can be kept rendering in the background on this
// device with `config`. Ignores `custom_snapshot_enabled`. Has no side effects.
bool IsBackgroundedSceneRenderingSupported(
    const BackgroundedSceneRenderingConfig& config);

// Prepares `configuration` so that a web view created from it can keep
// rendering in the background. Returns whether it was prepared; if false,
// `configuration` is left unmodified.
bool PrepareWebViewConfigurationForBackgroundedSceneRendering(
    WKWebViewConfiguration* configuration,
    const BackgroundedSceneRenderingConfig& config);

// Starts periodically refreshing `web_view`, which must have been created from
// a prepared configuration. No-op if already started or not supported.
void StartBackgroundedSceneRendering(
    WKWebView* web_view,
    const BackgroundedSceneRenderingConfig& config);

// Notifies that `web_view` committed a main frame navigation, which refreshes
// it immediately and restarts the refresh timer. No-op if not started.
void BackgroundedSceneRenderingDidCommitNavigation(WKWebView* web_view);

// Stops refreshing `web_view` and clears its state. No-op if not started.
void StopBackgroundedSceneRendering(WKWebView* web_view);

// Snapshots `web_view` while its scene is in the background; the web view does
// not need to be prepared. Returns whether the snapshot is handled. If true,
// `completion` is called once, asynchronously, on the main thread. If false,
// `completion` is never called and the caller should take a standard snapshot.
bool TakeBackgroundedSceneCustomSnapshot(
    WKWebView* web_view,
    WKSnapshotConfiguration* configuration,
    const BackgroundedSceneRenderingConfig& config,
    void (^completion)(UIImage* snapshot, NSError* error));

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BACKGROUNDED_SCENE_RENDERING_BACKGROUNDED_SCENE_RENDERING_API_H_
