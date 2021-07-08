// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/web/public/web_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    ReadingListModel* model,
    web::WebState* web_state)
    : url_(URL),
      title_(title),
      estimated_read_time_(estimated_read_time),
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
  // TODO(crbug.com/1195978): Add message title text.
  return std::u16string();
}

bool IOSAddToReadingListInfobarDelegate::Accept() {
  model_->AddEntry(url_, base::UTF16ToUTF8(title_),
                   reading_list::ADDED_VIA_CURRENT_APP,
                   base::TimeDelta::FromMinutes(estimated_read_time_));
  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(web_state_);
  if (sourceID != ukm::kInvalidSourceId) {
    ukm::builders::IOS_PageAddedToReadingList(sourceID)
        .SetAddedFromMessages(true)
        .Record(ukm::UkmRecorder::Get());
  }
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:kLastReadingListEntryAddedFromMessages];
  return true;
}
