// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_helper_attacher.h"

#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

TabHelperAttacher::TabHelperAttacher(web::WebState* web_state,
                                     TabHelperFilter filter_flags)
    : web_state_(CHECK_DEREF(web_state)),
      profile_(CHECK_DEREF(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()))),
      is_off_the_record_(profile_->IsOffTheRecord()),
      for_prerender_(
          IsTabHelperFilterMaskSet(filter_flags, TabHelperFilter::kPrerender)),
      for_lens_overlay_(
          IsTabHelperFilterMaskSet(filter_flags,
                                   TabHelperFilter::kLensOverlay)),
      for_reader_mode_(IsTabHelperFilterMaskSet(filter_flags,
                                                TabHelperFilter::kReaderMode)) {
}

TabHelperAttacher::~TabHelperAttacher() = default;
