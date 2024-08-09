// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/ui_thread_search_terms_data.h"

#import <string>

#import "base/check.h"
#import "base/strings/escape.h"
#import "base/strings/strcat.h"
#import "components/google/core/common/google_util.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/google/model/google_brand.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/web/public/thread/web_thread.h"
#import "rlz/buildflags/buildflags.h"
#import "url/gurl.h"

#if BUILDFLAG(ENABLE_RLZ)
#import "components/rlz/rlz_tracker.h"  // nogncheck
#endif

namespace ios {

UIThreadSearchTermsData::UIThreadSearchTermsData() {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
}

UIThreadSearchTermsData::~UIThreadSearchTermsData() {}

std::string UIThreadSearchTermsData::GoogleBaseURLValue() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (google_base_url.is_valid())
    return google_base_url.spec();

  return SearchTermsData::GoogleBaseURLValue();
}

std::string UIThreadSearchTermsData::GetApplicationLocale() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetApplicationContext()->GetApplicationLocale();
}

std::u16string UIThreadSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  DCHECK(!from_app_list);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::u16string rlz_string;
#if BUILDFLAG(ENABLE_RLZ)
  // For organic brandcode do not use rlz at all.
  if (!ios::google_brand::IsOrganic(ios::provider::GetBrandCode())) {
    // This call will may return false until the value has been cached. This
    // normally would mean that a few omnibox searches might not send the RLZ
    // data but this is not really a problem (as the value will eventually be
    // set and cached).
    rlz::RLZTracker::GetAccessPointRlz(rlz::RLZTracker::ChromeOmnibox(),
                                       &rlz_string);
  }
#endif
  return rlz_string;
}

std::string UIThreadSearchTermsData::GetSearchClient() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::string();
}

std::string UIThreadSearchTermsData::GoogleImageSearchSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string channel_name = GetChannelString();
  return base::StrCat({version_info::GetProductName(), " ",
                       version_info::GetVersionNumber(),
                       version_info::IsOfficialBuild() ? " (Official) " : " ",
                       version_info::GetOSType(),
                       channel_name.empty() ? "" : " ", channel_name});
}

}  // namespace ios
