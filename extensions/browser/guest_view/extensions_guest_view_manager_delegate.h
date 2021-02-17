// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_

#include "components/guest_view/browser/guest_view_manager_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// ExtensionsGuestViewManagerDelegate implements GuestViewManager functionality
// specific to Chromium builds that include the extensions module.
class ExtensionsGuestViewManagerDelegate
    : public guest_view::GuestViewManagerDelegate {
 public:
  explicit ExtensionsGuestViewManagerDelegate(content::BrowserContext* context);
  ~ExtensionsGuestViewManagerDelegate() override;

  // GuestViewManagerDelegate implementation.
  void OnGuestAdded(content::WebContents* guest_web_contents) const override;
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::DictionaryValue> args,
                     guest_view::GuestViewBase* guest,
                     int instance_id) override;
  bool IsGuestAvailableToContext(guest_view::GuestViewBase* guest) override;
  bool IsOwnedByExtension(guest_view::GuestViewBase* guest) override;
  void RegisterAdditionalGuestViewTypes() override;

 private:
  content::BrowserContext* const context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_
