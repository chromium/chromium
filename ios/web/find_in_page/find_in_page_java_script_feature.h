// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

class WebFrame;

namespace find_in_page {
// Value returned when a `Search` or `Pump` call times out.
extern const int kFindInPagePending;
}  // namespace find_in_page

// A feature which searches for, highlights and allows navigating through text
// search results.
class FindInPageJavaScriptFeature : public JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static FindInPageJavaScriptFeature* GetInstance();

  // Searches for string `query` in `frame`. `callback` returns the number of
  // search results found or `kFindInPagePending` if a call to `Pump` is
  // necessary before match count is available.
  bool Search(WebFrame* frame,
              const std::string& query,
              base::OnceCallback<void(std::optional<int>)> callback);

  // Continues an ongoing search started with `Search` which hasn't yet
  // completed. `callback` returns the number of search results found or
  // `kFindInPagePending` if more calls to `Pump` are necessary before match
  // count is available.
  void Pump(WebFrame* frame,
            base::OnceCallback<void(std::optional<int>)> callback);

  // Selects the given match at `index` in `frame`. `callback` is called with
  // the dictionary value results from the selection.
  void SelectMatch(WebFrame* frame,
                   int index,
                   base::OnceCallback<void(const base::Value*)> callback);

  // Stops any current search and clears highlighted/selected state from the
  // page.
  void Stop(WebFrame* frame);

 private:
  friend class base::NoDestructor<FindInPageJavaScriptFeature>;

  // Processes the JavaScript `result` to extract the match count and send it
  // to `callback`.
  void ProcessSearchResult(
      base::OnceCallback<void(const std::optional<int>)> callback,
      const base::Value* result);

  FindInPageJavaScriptFeature();
  ~FindInPageJavaScriptFeature() override;

  FindInPageJavaScriptFeature(const FindInPageJavaScriptFeature&) = delete;
  FindInPageJavaScriptFeature& operator=(const FindInPageJavaScriptFeature&) =
      delete;
};

}  // namespace web

#endif  // IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_JAVA_SCRIPT_FEATURE_H_
