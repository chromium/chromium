// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_distiller_page_factory.h"

#import "ios/chrome/browser/reading_list/model/favicon_web_state_dispatcher_impl.h"
#import "ios/chrome/browser/reading_list/model/reading_list_distiller_page.h"

namespace reading_list {

ReadingListDistillerPageFactory::ReadingListDistillerPageFactory(
    ProfileIOS* profile)
    : profile_(profile) {
  web_state_dispatcher_ =
      std::make_unique<reading_list::FaviconWebStateDispatcherImpl>(profile_);
}

ReadingListDistillerPageFactory::~ReadingListDistillerPageFactory() {}

std::unique_ptr<ReadingListDistillerPage>
ReadingListDistillerPageFactory::CreateReadingListDistillerPage(
    const GURL& url,
    ReadingListDistillerPageDelegate* delegate) const {
  return std::make_unique<ReadingListDistillerPage>(
      url, profile_, web_state_dispatcher_.get(), delegate);
}

void ReadingListDistillerPageFactory::ReleaseAllRetainedWebState() {
  web_state_dispatcher_->ReleaseAll();
}

}  // namespace reading_list
