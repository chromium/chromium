// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/debug/crash_logging.h"
#import "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/animation/animation.h"

namespace content {

class BrowserAccessibilityStateImplMac : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplMac();
  ~BrowserAccessibilityStateImplMac() override {}

 protected:
  void RefreshAssistiveTech() override;
};

BrowserAccessibilityStateImplMac::BrowserAccessibilityStateImplMac() {
  // Set up accessibility notifications.

  // Skip this in tests that don't set up a task runner on the main thread.
  if (!base::SingleThreadTaskRunner::HasCurrentDefault()) {
    return;
  }

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
                NotifyWebContentsPreferencesChanged();
              }];

  // Set up KVO monitoring of VoiceOver state changes. KVO best practices
  // recommend setting the context to the "address of a uniquely named
  // static variable within the class". This allows observers to disambiguate
  // notifications (where a class and its superclass, say, are observing the
  // same property). We'll use the global accessibility object.
  [[NSWorkspace sharedWorkspace] addObserver:NSApp
                                  forKeyPath:@"voiceOverEnabled"
                                     options:(NSKeyValueObservingOptionInitial |
                                              NSKeyValueObservingOptionNew)
                                     context:this];
}

void BrowserAccessibilityStateImplMac::RefreshAssistiveTech() {
  bool is_active = GetAccessibilityMode().has_mode(ui::AXMode::kScreenReader);

  static auto* ax_voiceover_crash_key = base::debug::AllocateCrashKeyString(
      "ax_voiceover", base::debug::CrashKeySize::Size32);
  if (is_active) {
    base::debug::SetCrashKeyString(ax_voiceover_crash_key, "true");
  } else {
    base::debug::ClearCrashKeyString(ax_voiceover_crash_key);
  }

  UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.VoiceOver", is_active);

  OnAssistiveTechFound(is_active ? ui::AssistiveTech::kVoiceOver
                                 : ui::AssistiveTech::kNone);
}

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplMac>();
}

}  // namespace content
