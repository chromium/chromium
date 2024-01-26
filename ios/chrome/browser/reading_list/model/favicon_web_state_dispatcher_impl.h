// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_FAVICON_WEB_STATE_DISPATCHER_IMPL_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_FAVICON_WEB_STATE_DISPATCHER_IMPL_H_

#include <memory>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/reading_list/ios/favicon_web_state_dispatcher.h"

namespace web {
class BrowserState;
}

namespace reading_list {

// Implementation of the FaviconWebStateDispatcher.
class FaviconWebStateDispatcherImpl : public FaviconWebStateDispatcher {
 public:
  // Constructor for keeping the WebStates alive for `keep_alive_second`
  // seconds. If `keep_alive_second` < 0 then the default value is used.
  FaviconWebStateDispatcherImpl(web::BrowserState* browser_state,
                                int64_t keep_alive_second);
  ~FaviconWebStateDispatcherImpl() override;

  // FaviconWebStateDispatcher implementation.
  std::unique_ptr<web::WebState> RequestWebState() override;
  void ReturnWebState(std::unique_ptr<web::WebState> web_state) override;
  void ReleaseAll() override;

 private:
  raw_ptr<web::BrowserState> browser_state_;
  // Map of the WebStates currently alive.
  std::vector<std::unique_ptr<web::WebState>> web_states_;
  // Time during which the WebState will be kept alive after being returned.
  int64_t keep_alive_second_;
  base::WeakPtrFactory<FaviconWebStateDispatcherImpl> weak_ptr_factory_;
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_FAVICON_WEB_STATE_DISPATCHER_IMPL_H_
