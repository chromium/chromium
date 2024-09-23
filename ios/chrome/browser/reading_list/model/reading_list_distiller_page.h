// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DISTILLER_PAGE_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DISTILLER_PAGE_H_

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/ios/distiller_page_ios.h"
#include "url/gurl.h"

namespace web {
class BrowserState;
}

namespace reading_list {

class FaviconWebStateDispatcher;

// A delegate to get information on the page that is actually loaded during
// distillation.
class ReadingListDistillerPageDelegate {
 public:
  ReadingListDistillerPageDelegate(const ReadingListDistillerPageDelegate&) =
      delete;
  ReadingListDistillerPageDelegate& operator=(
      const ReadingListDistillerPageDelegate&) = delete;

  virtual ~ReadingListDistillerPageDelegate();

  // A callback called if the URL passed to the distilled led to a redirection.
  virtual void DistilledPageRedirectedToURL(const GURL& original_url,
                                            const GURL& final_url) = 0;
  // Gives to the delegate the mime type of the loaded page.
  // If the type is not "text/html", the distillation will fail.
  virtual void DistilledPageHasMimeType(const GURL& original_url,
                                        const std::string& mime_type) = 0;

 protected:
  ReadingListDistillerPageDelegate();
};

// An DistillerPageIOS that will retain WebState to allow favicon download and
// and add a 2 seconds delay between loading and distillation.
class ReadingListDistillerPage : public dom_distiller::DistillerPageIOS {
 public:
  // Creates a ReadingListDistillerPage to distill `url`. WebStates to download
  // the pages will be provided by web_state_dispatcher.
  // `browser_state`, `web_state_dispatcher` and `delegate` must not be null.
  explicit ReadingListDistillerPage(
      const GURL& url,
      web::BrowserState* browser_state,
      FaviconWebStateDispatcher* web_state_dispatcher,
      ReadingListDistillerPageDelegate* delegate);

  ReadingListDistillerPage(const ReadingListDistillerPage&) = delete;
  ReadingListDistillerPage& operator=(const ReadingListDistillerPage&) = delete;

  ~ReadingListDistillerPage() override;

 protected:
  void DistillPageImpl(const GURL& url, const std::string& script) override;
  void OnDistillationDone(const GURL& page_url,
                          const base::Value* value) override;
  void OnLoadURLDone(
      web::PageLoadCompletionStatus load_completion_status) override;

 private:
  // Returns whether there is the loading has no error and if the distillation
  // can continue.
  bool IsLoadingSuccess(web::PageLoadCompletionStatus load_completion_status);
  // Work around the fact that articles opened from Google Search page and
  // Google News are presented in an iframe. This method detects if the current
  // page is a Google AMP and navigate to the iframe in that case.
  // Returns whether the current page is a Google AMP page.
  // IsGoogleCachedAMPPage will determine if the current page is a Google AMP
  // page.
  bool IsGoogleCachedAMPPage();
  // HandleGoogleCachedAMPPage will navigate to the iframe containing the actual
  // article page.
  void HandleGoogleCachedAMPPage();
  // Handles the JavaScript response. If the URL of the iframe is returned,
  // triggers a navigation to it. Stop distillation of the page there as the new
  // load will trigger a new distillation.
  void OnHandleGoogleCachedAMPPageResult(const base::Value* value,
                                         NSError* error);

  // Work around the fact that articles from wikipedia has the major part of the
  // article hidden.
  // IsWikipediaPage determines if the current page is a wikipedia article.
  bool IsWikipediaPage();
  // HandleWikipediaPage sets the style of collapsable parts of article to
  // visible.
  void HandleWikipediaPage();
  // OnHandleWikipediaPageResult is called asynchronously with the
  // result of the javascript evaluation started in
  // HandleWikipediaPage.
  void OnHandleWikipediaPageResult(const base::Value* value);

  // Continue the distillation on the page that is currently loaded in
  // `CurrentWebState()`.
  void ContinuePageDistillation();
  // Starts the fetching of `page_url`'s favicon.
  void FetchFavicon(const GURL& page_url);

  // Continues distillation by calling superclass `OnLoadURLDone`.
  void DelayedOnLoadURLDone(int delayed_task_id);
  GURL original_url_;
  bool distilling_main_page_;

  raw_ptr<FaviconWebStateDispatcher> web_state_dispatcher_;
  raw_ptr<ReadingListDistillerPageDelegate> delegate_;
  int delayed_task_id_;
  base::WeakPtrFactory<ReadingListDistillerPage> weak_ptr_factory_;
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_DISTILLER_PAGE_H_
