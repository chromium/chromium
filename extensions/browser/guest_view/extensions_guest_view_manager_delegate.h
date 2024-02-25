// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_

#include "components/guest_view/browser/guest_view_manager_delegate.h"

namespace extensions {

// ExtensionsGuestViewManagerDelegate implements GuestViewManager functionality
// specific to Chromium builds that include the extensions module.
class ExtensionsGuestViewManagerDelegate
    : public guest_view::GuestViewManagerDelegate {
 public:
  static bool IsGuestAvailableToContextWithFeature(
      const guest_view::GuestViewBase* guest,
      const std::string& feature_name);

  ExtensionsGuestViewManagerDelegate();
  ~ExtensionsGuestViewManagerDelegate() override;

  // GuestViewManagerDelegate implementation.
  void OnGuestAdded(content::WebContents* guest_web_contents) const override;
  void DispatchEvent(const std::string& event_name,
                     base::Value::Dict args,
                     guest_view::GuestViewBase* guest,
                     int instance_id) override;
  bool IsGuestAvailableToContext(
      const guest_view::GuestViewBase* guest) const override;
  bool IsOwnedByExtension(const guest_view::GuestViewBase* guest) override;
  bool IsOwnedByControlledFrameEmbedder(
      const guest_view::GuestViewBase* guest) override;
  void RegisterAdditionalGuestViewTypes(
      guest_view::GuestViewManager* manager) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_
