// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DISTILLER_PAGE_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DISTILLER_PAGE_FACTORY_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#include "url/gurl.h"

namespace web {
class BrowserState;
}

namespace reading_list {

class FaviconWebStateDispatcher;
class ReadingListDistillerPage;
class ReadingListDistillerPageDelegate;

// ReadingListDistillerPageFactory is an iOS-specific implementation of the
// DistillerPageFactory interface allowing the creation of DistillerPage
// instances.
// These instances are configured to distille the articles of the Reading List.
class ReadingListDistillerPageFactory {
 public:
  explicit ReadingListDistillerPageFactory(web::BrowserState* browser_state);

  ReadingListDistillerPageFactory(const ReadingListDistillerPageFactory&) =
      delete;
  ReadingListDistillerPageFactory& operator=(
      const ReadingListDistillerPageFactory&) = delete;

  virtual ~ReadingListDistillerPageFactory();

  // Creates a ReadingListDistillerPage to distill `url`.
  // Information about page will be reported to `delegate`.
  std::unique_ptr<ReadingListDistillerPage> CreateReadingListDistillerPage(
      const GURL& url,
      ReadingListDistillerPageDelegate* delegate) const;

  // Releases all WebState owned by `web_state_dispatcher_`.
  void ReleaseAllRetainedWebState();

 private:
  raw_ptr<web::BrowserState> browser_state_;
  std::unique_ptr<FaviconWebStateDispatcher> web_state_dispatcher_;
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DISTILLER_PAGE_FACTORY_H_
