// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/color_utils.h"

namespace content {

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
enum ModeFlagHistogramValue {
  UMA_AX_MODE_NATIVE_APIS = 0,
  UMA_AX_MODE_WEB_CONTENTS = 1,
  UMA_AX_MODE_INLINE_TEXT_BOXES = 2,
  UMA_AX_MODE_SCREEN_READER = 3,
  UMA_AX_MODE_HTML = 4,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  UMA_AX_MODE_MAX
};

// Record a histograms for an accessibility mode when it's enabled.
void RecordNewAccessibilityModeFlags(ModeFlagHistogramValue mode_flag) {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.ModeFlag", mode_flag,
                            UMA_AX_MODE_MAX);
}

// Update the accessibility histogram 45 seconds after initialization.
static const int ACCESSIBILITY_HISTOGRAM_DELAY_SECS = 45;

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return BrowserAccessibilityStateImpl::GetInstance();
}

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  return base::Singleton<
      BrowserAccessibilityStateImpl,
      base::LeakySingletonTraits<BrowserAccessibilityStateImpl>>::get();
}

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : BrowserAccessibilityState(), disable_hot_tracking_(false) {
  ResetAccessibilityModeValue();

  // We need to AddRef() the leaky singleton so that Bind doesn't
  // delete it prematurely.
  AddRef();

  // Hook ourselves up to observe ax mode changes.
  ui::AXPlatformNode::AddAXModeObserver(this);

  // Let each platform do its own initialization.
  PlatformInitialize();

  // Schedule calls to update histograms after a delay.
  //
  // The delay is necessary because assistive technology sometimes isn't
  // detected until after the user interacts in some way, so a reasonable delay
  // gives us better numbers.

  // Some things can be done on another thread safely.
  base::PostDelayedTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread, this),
      base::TimeDelta::FromSeconds(ACCESSIBILITY_HISTOGRAM_DELAY_SECS));

  // Other things must be done on the UI thread (e.g. to access PrefService).
  base::PostDelayedTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread,
                     this),
      base::TimeDelta::FromSeconds(ACCESSIBILITY_HISTOGRAM_DELAY_SECS));
}

BrowserAccessibilityStateImpl::~BrowserAccessibilityStateImpl() {
  // Remove ourselves from the AXMode global observer list.
  ui::AXPlatformNode::RemoveAXModeObserver(this);
}

void BrowserAccessibilityStateImpl::OnScreenReaderDetected() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }
  EnableAccessibility();
}

void BrowserAccessibilityStateImpl::EnableAccessibility() {
  AddAccessibilityModeFlags(ui::kAXModeComplete);
}

void BrowserAccessibilityStateImpl::DisableAccessibility() {
  ResetAccessibilityMode();
}

bool BrowserAccessibilityStateImpl::IsRendererAccessibilityEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRendererAccessibility);
}

void BrowserAccessibilityStateImpl::ResetAccessibilityModeValue() {
  accessibility_mode_ = ui::AXMode();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    accessibility_mode_ = ui::kAXModeComplete;
  }
}

void BrowserAccessibilityStateImpl::ResetAccessibilityMode() {
  ResetAccessibilityModeValue();

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->SetAccessibilityMode(accessibility_mode_);
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return accessibility_mode_ == ui::kAXModeComplete;
}

void BrowserAccessibilityStateImpl::AddUIThreadHistogramCallback(
    base::OnceClosure callback) {
  ui_thread_histogram_callbacks_.push_back(std::move(callback));
}

void BrowserAccessibilityStateImpl::AddOtherThreadHistogramCallback(
    base::OnceClosure callback) {
  other_thread_histogram_callbacks_.push_back(std::move(callback));
}

void BrowserAccessibilityStateImpl::UpdateHistogramsForTesting() {
  UpdateHistogramsOnUIThread();
  UpdateHistogramsOnOtherThread();
}

bool BrowserAccessibilityStateImpl::IsCaretBrowsingEnabled() const {
  // TODO(crbug.com/1018947): Refine this check once UX provided to toggle caret
  // browsing mode.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCaretBrowsing);
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread() {
  UpdatePlatformSpecificHistogramsOnUIThread();

  for (auto& callback : ui_thread_histogram_callbacks_)
    std::move(callback).Run();
  ui_thread_histogram_callbacks_.clear();

  UMA_HISTOGRAM_BOOLEAN("Accessibility.InvertedColors",
                        color_utils::IsInvertedColorScheme());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.ManuallyEnabled",
                        base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kForceRendererAccessibility));
}

void BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread() {
  UpdatePlatformSpecificHistogramsOnOtherThread();

  for (auto& callback : other_thread_histogram_callbacks_)
    std::move(callback).Run();
  other_thread_histogram_callbacks_.clear();
}

void BrowserAccessibilityStateImpl::OnAXModeAdded(ui::AXMode mode) {
  AddAccessibilityModeFlags(mode);
}

ui::AXMode BrowserAccessibilityStateImpl::GetAccessibilityMode() {
  return accessibility_mode_;
}

#if !defined(OS_ANDROID) && !defined(OS_WIN) && !defined(OS_MACOSX)
void BrowserAccessibilityStateImpl::PlatformInitialize() {}

void BrowserAccessibilityStateImpl::
    UpdatePlatformSpecificHistogramsOnUIThread() {}
void BrowserAccessibilityStateImpl::
    UpdatePlatformSpecificHistogramsOnOtherThread() {}
void BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms() {}
#endif

void BrowserAccessibilityStateImpl::AddAccessibilityModeFlags(ui::AXMode mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }

  ui::AXMode previous_mode = accessibility_mode_;
  accessibility_mode_ |= mode;
  if (accessibility_mode_ == previous_mode)
    return;

  // Retrieve only newly added modes for the purposes of logging.
  int new_mode_flags = mode.mode() & (~previous_mode.mode());
  if (new_mode_flags & ui::AXMode::kNativeAPIs)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_NATIVE_APIS);
  if (new_mode_flags & ui::AXMode::kWebContents)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_WEB_CONTENTS);
  if (new_mode_flags & ui::AXMode::kInlineTextBoxes)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_INLINE_TEXT_BOXES);
  if (new_mode_flags & ui::AXMode::kScreenReader)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_SCREEN_READER);
  if (new_mode_flags & ui::AXMode::kHTML)
    RecordNewAccessibilityModeFlags(UMA_AX_MODE_HTML);

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->AddAccessibilityMode(accessibility_mode_);
}

void BrowserAccessibilityStateImpl::RemoveAccessibilityModeFlags(
    ui::AXMode mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility) &&
      mode == ui::kAXModeComplete) {
    return;
  }

  int raw_flags =
      accessibility_mode_.mode() ^ (mode.mode() & accessibility_mode_.mode());
  accessibility_mode_ = raw_flags;

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->SetAccessibilityMode(accessibility_mode_);
}

}  // namespace content
