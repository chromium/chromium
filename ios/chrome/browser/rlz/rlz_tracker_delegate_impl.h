// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_RLZ_RLZ_TRACKER_DELEGATE_IMPL_H_
#define IOS_CHROME_BROWSER_RLZ_RLZ_TRACKER_DELEGATE_IMPL_H_

#import <memory>

#import "base/callback_list.h"
#import "base/functional/callback.h"
#import "components/rlz/rlz_tracker_delegate.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

struct OmniboxLog;

// RLZTrackerDelegateImpl implements RLZTrackerDelegate abstract interface
// and provides access to Chrome on iOS features.
class RLZTrackerDelegateImpl : public rlz::RLZTrackerDelegate {
 public:
  RLZTrackerDelegateImpl();

  RLZTrackerDelegateImpl(const RLZTrackerDelegateImpl&) = delete;
  RLZTrackerDelegateImpl& operator=(const RLZTrackerDelegateImpl&) = delete;

  ~RLZTrackerDelegateImpl() override;

  static bool IsGoogleDefaultSearch(ProfileIOS* profile);
  static bool IsGoogleHomepage(ProfileIOS* profile);
  static bool IsGoogleInStartpages(ProfileIOS* profile);

 private:
  // RLZTrackerDelegate implementation.
  void Cleanup() override;
  bool IsOnUIThread() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool GetBrand(std::string* brand) override;
  bool IsBrandOrganic(const std::string& brand) override;
  bool GetReactivationBrand(std::string* brand) override;
  bool ShouldEnableZeroDelayForTesting() override;
  bool GetLanguage(std::u16string* language) override;
  bool GetReferral(std::u16string* referral) override;
  bool ClearReferral() override;
  void SetOmniboxSearchCallback(base::OnceClosure callback) override;
  void SetHomepageSearchCallback(base::OnceClosure callback) override;
  void RunHomepageSearchCallback() override;
  bool ShouldUpdateExistingAccessPointRlz() override;

  // Called when user open an URL from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  base::OnceClosure on_omnibox_search_callback_;
  base::CallbackListSubscription on_omnibox_url_opened_subscription_;
};

#endif  // IOS_CHROME_BROWSER_RLZ_RLZ_TRACKER_DELEGATE_IMPL_H_
