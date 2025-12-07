// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_PRESENTER_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_PRESENTER_H_

#import "base/memory/weak_ptr.h"

class Browser;
enum class TipsNotificationType;

// A class that handles presenting the UI for a Tips Notification.
class TipsNotificationPresenter {
 public:
  // Presents the UI asynchronously for the given notification `type` using the
  // given `browser`, after closing other modals and opening an NTP.
  static void Present(base::WeakPtr<Browser> browser,
                      TipsNotificationType type);

 private:
  // Initializes an instance in order to present tips notification UI.
  explicit TipsNotificationPresenter(Browser* browser);

  // Presents the UI immediately for the given notification `type` using the
  // given `browser.
  static void PresentInternal(base::WeakPtr<Browser> browser,
                              TipsNotificationType type);

  // Presents the UI immediately for the given notification `type`.
  void Present(TipsNotificationType type);

  // Shows the Default Browser promo.
  void ShowDefaultBrowserPromo();

  // Shows the What's New view.
  void ShowWhatsNew();

  // Starts the sign-in flow.
  void ShowSignin();

  // Shows the Set Up List "See More" view.
  void ShowSetUpListContinuation();

  // Shows the Docking promo.
  void ShowDocking();

  // Shows the Omnibox Position promo.
  void ShowOmniboxPosition();

  // Shows the Lens promo.
  void ShowLensPromo();

  // Shows the Enhanced Safe Browsing promo.
  void ShowEnhancedSafeBrowsingPromo();

  // Shows the CPE promo.
  void ShowCPEPromo();

  // Shows the Lens Overlay promo.
  void ShowLensOverlayPromo();

  // Returns true if there are any identities on the device.
  bool HasIdentitiesOnDevice();

  // Starts the trusted Vault key retrieval flow.
  void StartTrustedVaultKeyRetrievalFlow();

  // Contains a non-weak pointer to the browser.
  raw_ptr<Browser> browser_;
};

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_PRESENTER_H_
