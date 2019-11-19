// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/search_engines/search_terms_data.h"

namespace ios {

// Implementation of SearchTermsData that is only usable on UI thread.
class UIThreadSearchTermsData : public SearchTermsData {
 public:
  UIThreadSearchTermsData();
  ~UIThreadSearchTermsData() override;

  // SearchTermsData implementation.
  std::string GoogleBaseURLValue() const override;
  std::string GetApplicationLocale() const override;
  base::string16 GetRlzParameterValue(bool from_app_list) const override;
  std::string GetSearchClient() const override;
  std::string GetSuggestClient() const override;
  std::string GetSuggestRequestIdentifier() const override;
  std::string GoogleImageSearchSource() const override;

 private:
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UIThreadSearchTermsData);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_UI_THREAD_SEARCH_TERMS_DATA_H_
