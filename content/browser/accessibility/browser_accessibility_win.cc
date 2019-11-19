// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "ui/base/win/atl_module.h"

namespace content {

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityWin();
}

BrowserAccessibilityWin::BrowserAccessibilityWin() {
  ui::win::CreateATLModuleIfNeeded();
  HRESULT hr = CComObject<BrowserAccessibilityComWin>::CreateInstance(
      &browser_accessibility_com_);
  DCHECK(SUCCEEDED(hr));

  browser_accessibility_com_->AddRef();

  // Set the delegate to us
  browser_accessibility_com_->Init(this);
}

BrowserAccessibilityWin::~BrowserAccessibilityWin() {
  if (browser_accessibility_com_) {
    browser_accessibility_com_->Destroy();
    browser_accessibility_com_ = nullptr;
  }
}

void BrowserAccessibilityWin::UpdatePlatformAttributes() {
  GetCOM()->UpdateStep1ComputeWinAttributes();
  GetCOM()->UpdateStep2ComputeHypertext();
  GetCOM()->UpdateStep3FireEvents(false);
}

ui::AXPlatformNode* BrowserAccessibilityWin::GetAXPlatformNode() const {
  if (!instance_active())
    return nullptr;

  return GetCOM();
}

bool BrowserAccessibilityWin::IsNative() const {
  return true;
}

void BrowserAccessibilityWin::OnLocationChanged() {
  GetCOM()->FireNativeEvent(EVENT_OBJECT_LOCATIONCHANGE);
}

base::string16 BrowserAccessibilityWin::GetText() const {
  return GetHypertext();
}

base::string16 BrowserAccessibilityWin::GetHypertext() const {
  return GetCOM()->AXPlatformNodeWin::GetHypertext();
}

gfx::NativeViewAccessible BrowserAccessibilityWin::GetNativeViewAccessible() {
  return GetCOM();
}

BrowserAccessibilityComWin* BrowserAccessibilityWin::GetCOM() const {
  DCHECK(browser_accessibility_com_);
  return browser_accessibility_com_;
}

BrowserAccessibilityWin* ToBrowserAccessibilityWin(BrowserAccessibility* obj) {
  DCHECK(!obj || obj->IsNative());
  return static_cast<BrowserAccessibilityWin*>(obj);
}

const BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    const BrowserAccessibility* obj) {
  DCHECK(!obj || obj->IsNative());
  return static_cast<const BrowserAccessibilityWin*>(obj);
}

ui::TextAttributeList BrowserAccessibilityWin::ComputeTextAttributes() const {
  return GetCOM()->AXPlatformNodeWin::ComputeTextAttributes();
}

}  // namespace content
