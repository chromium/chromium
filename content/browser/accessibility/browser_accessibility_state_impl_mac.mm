// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "ui/gfx/animation/animation.h"

namespace content {

namespace {
void SetUpAccessibilityNotifications() {
  // We need to call into gfx::Animation and WebContentsImpl on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Listen to accessibility display options changing, so that we can update
  // the renderer for the prefers reduced motion settings.
  //
  // BrowserAccessibilityStateImpl is a deliberately leaked singleton, so we
  // don't need to record the notification token for later cleanup.
  [NSWorkspace.sharedWorkspace.notificationCenter
      addObserverForName:
          NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                gfx::Animation::UpdatePrefersReducedMotion();
                for (WebContentsImpl* wc :
                     WebContentsImpl::GetAllWebContents()) {
                  wc->OnWebPreferencesChanged();
                }
              }];

  if (base::mac::MacOSVersion() >= 14'00'00 &&
      base::FeatureList::IsEnabled(
          features::kSonomaAccessibilityActivationRefinements)) {
    // Set up KVO monitoring of VoiceOver state changes. KVO best practices
    // recommend setting the context to the "address of a uniquely named
    // static variable within the class". This allows observers to disambiguate
    // notifications (where a class and its superclass, say, are observing the
    // same property). We'll use the global accessibility object.
    [[NSWorkspace sharedWorkspace]
        addObserver:NSApp
         forKeyPath:@"voiceOverEnabled"
            options:(NSKeyValueObservingOptionInitial |
                     NSKeyValueObservingOptionNew)
            context:BrowserAccessibilityStateImpl::GetInstance()];
  }
}
}  // namespace

class BrowserAccessibilityStateImplMac : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplMac() = default;
  ~BrowserAccessibilityStateImplMac() override {}

 protected:
  void InitBackgroundTasks() override;
  void UpdateHistogramsOnOtherThread() override;
  void UpdateUniqueUserHistograms() override;
};

void BrowserAccessibilityStateImplMac::InitBackgroundTasks() {
  BrowserAccessibilityStateImpl::InitBackgroundTasks();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SetUpAccessibilityNotifications));
}

void BrowserAccessibilityStateImplMac::UpdateHistogramsOnOtherThread() {
  BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread();

  // Screen reader metric.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ScreenReader",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

void BrowserAccessibilityStateImplMac::UpdateUniqueUserHistograms() {
  BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms();

  ui::AXMode mode = GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ScreenReader.EveryReport",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplMac>();
}

}  // namespace content
