// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#import <Cocoa/Cocoa.h>

#include "base/metrics/histogram_macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "ui/gfx/animation/animation.h"

@interface NSWorkspace (Partials)

@property(readonly) BOOL accessibilityDisplayShouldDifferentiateWithoutColor;
@property(readonly) BOOL accessibilityDisplayShouldIncreaseContrast;
@property(readonly) BOOL accessibilityDisplayShouldReduceTransparency;

@end

// Only available since 10.12.
@interface NSWorkspace (AvailableSinceSierra)
@property(readonly) BOOL accessibilityDisplayShouldReduceMotion;
@end

namespace content {

namespace {
void SetupAccessibilityDisplayOptionsNotifier() {
  // We need to call into gfx::Animation and WebContentsImpl on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Listen to accessibility display options changing, so that we can update
  // the renderer for the prefers reduced motion settings.
  //
  // BrowserAccessibilityStateImpl is a deliberately leaked singleton, so we
  // don't need to record the notification token for later cleanup.
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserverForName:
          NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                gfx::Animation::UpdatePrefersReducedMotion();
                for (WebContentsImpl* wc :
                     WebContentsImpl::GetAllWebContents()) {
                  wc->GetRenderViewHost()->OnWebkitPreferencesChanged();
                }
              }];
}
}  // namespace

void BrowserAccessibilityStateImpl::PlatformInitialize() {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&SetupAccessibilityDisplayOptionsNotifier));
}

void BrowserAccessibilityStateImpl::
    UpdatePlatformSpecificHistogramsOnUIThread() {
  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];

  SEL sel = @selector(accessibilityDisplayShouldIncreaseContrast);
  if ([workspace respondsToSelector:sel]) {
    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.Mac.DifferentiateWithoutColor",
        workspace.accessibilityDisplayShouldDifferentiateWithoutColor);
    UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.IncreaseContrast",
                          workspace.accessibilityDisplayShouldIncreaseContrast);
    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.Mac.ReduceTransparency",
        workspace.accessibilityDisplayShouldReduceTransparency);

    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.Mac.FullKeyboardAccessEnabled",
        static_cast<NSApplication*>(NSApp).fullKeyboardAccessEnabled);
  }

  sel = @selector(accessibilityDisplayShouldReduceMotion);
  if ([workspace respondsToSelector:sel]) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ReduceMotion",
                          workspace.accessibilityDisplayShouldReduceMotion);
  }
}

void BrowserAccessibilityStateImpl::
    UpdatePlatformSpecificHistogramsOnOtherThread() {
  // Screen reader metric.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ScreenReader",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

void BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms() {
  ui::AXMode mode = GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ScreenReader.EveryReport",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

}  // namespace content
