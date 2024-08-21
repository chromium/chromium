// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/prefs/pref_service.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"

// static
IOSAddToReadingListInfobarDelegate*
IOSAddToReadingListInfobarDelegate::FromInfobarDelegate(
    infobars::InfoBarDelegate* delegate) {
  return delegate->GetIdentifier() == ADD_TO_READING_LIST_IOS
             ? static_cast<IOSAddToReadingListInfobarDelegate*>(delegate)
             : nullptr;
}

IOSAddToReadingListInfobarDelegate::~IOSAddToReadingListInfobarDelegate() {}

IOSAddToReadingListInfobarDelegate::IOSAddToReadingListInfobarDelegate(
    const GURL& URL,
    const std::u16string& title,
    int estimated_read_time,
    double score,
    double long_score,
    ReadingListModel* model,
    web::WebState* web_state)
    : url_(URL),
      title_(title),
      estimated_read_time_(estimated_read_time),
      distilibility_score_(score),
      length_score_(long_score),
      model_(model),
      web_state_(web_state) {
  DCHECK(model_);
  DCHECK(web_state_);
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSAddToReadingListInfobarDelegate::GetIdentifier() const {
  return ADD_TO_READING_LIST_IOS;
}

std::u16string IOSAddToReadingListInfobarDelegate::GetMessageText() const {
  // TODO(crbug.com/40176250): Add message title text.
  return std::u16string();
}

void IOSAddToReadingListInfobarDelegate::InfoBarDismissed() {
  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(web_state_);
  if (sourceID != ukm::kInvalidSourceId) {
    // Round to the nearest tenth, and additionally round to a .5 level of
    // granularity if <0.5 or > 1.5. Get accuracy to the tenth digit in UKM by
    // multiplying by 10.
    int score_minimization = (int)(round(distilibility_score_ * 10));
    int long_score_minimization = (int)(round(length_score_ * 10));
    if (score_minimization > 15 || score_minimization < 5) {
      score_minimization = ((score_minimization + 2.5) / 5) * 5;
    }
    if (long_score_minimization > 15 || long_score_minimization < 5) {
      long_score_minimization = ((long_score_minimization + 2.5) / 5) * 5;
    }
    ukm::builders::IOS_PageReadability(sourceID)
        .SetDidAccept(false)
        .SetDistilibilityScore(score_minimization)
        .SetDistilibilityLongScore(long_score_minimization)
        .Record(ukm::UkmRecorder::Get());
  }
}

bool IOSAddToReadingListInfobarDelegate::Accept() {
  model_->AddOrReplaceEntry(url_, base::UTF16ToUTF8(title_.get()),
                            reading_list::ADDED_VIA_CURRENT_APP,
                            base::Minutes(estimated_read_time_));
  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(web_state_);
  if (sourceID != ukm::kInvalidSourceId) {
    ukm::builders::IOS_PageAddedToReadingList(sourceID)
        .SetAddedFromMessages(true)
        .Record(ukm::UkmRecorder::Get());
    // Round to the nearest tenth, and additionally round to a .5 level of
    // granularity if <0.5 or > 1.5. Get accuracy to the tenth digit in UKM by
    // multiplying by 10.
    int score_minimization = (int)(round(distilibility_score_ * 10));
    int long_score_minimization = (int)(round(length_score_ * 10));
    if (score_minimization > 15 || score_minimization < 5) {
      score_minimization = ((score_minimization + 2.5) / 5) * 5;
    }
    if (long_score_minimization > 15 || long_score_minimization < 5) {
      long_score_minimization = ((long_score_minimization + 2.5) / 5) * 5;
    }
    ukm::builders::IOS_PageReadability(sourceID)
        .SetDidAccept(true)
        .SetDistilibilityScore(score_minimization)
        .SetDistilibilityLongScore(long_score_minimization)
        .Record(ukm::UkmRecorder::Get());
  }
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:kLastReadingListEntryAddedFromMessages];
  return true;
}

void IOSAddToReadingListInfobarDelegate::NeverShow() {}
