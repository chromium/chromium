// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/scoped_observation.h"
#import "base/time/time.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace {

// Self-deleting observer that waits for a page to finish loading.
class SelfDestructivePageLoadObserver : public web::WebStateObserver {
 public:
  SelfDestructivePageLoadObserver(web::WebState* web_state,
                                  base::OnceClosure callback)
      : callback_(std::move(callback)) {
    observation_.Observe(web_state);
  }

 private:
  ~SelfDestructivePageLoadObserver() override = default;

  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override {
    std::move(callback_).Run();
    observation_.Reset();
    delete this;
  }

  void WebStateDestroyed(web::WebState* web_state) override {
    observation_.Reset();
    delete this;
  }

  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
  base::OnceClosure callback_;
};

}  // namespace

PageContextWrapper* CreatePageContextWrapper(
    web::WebState* web_state,
    base::OnceCallback<void(PageContextWrapperCallbackResponse)>
        completion_callback) {
  PageContextWrapper* page_context_wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state
      completionCallback:std::move(completion_callback)];
  [page_context_wrapper setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper setShouldGetSnapshot:YES];
  [page_context_wrapper setShouldGetFullPagePDF:YES];
  return page_context_wrapper;
}

void PopulatePageContext(PageContextWrapper* wrapper,
                         web::WebState* web_state) {
  if (web_state->IsLoading()) {
    __weak PageContextWrapper* weak_wrapper = wrapper;
    new SelfDestructivePageLoadObserver(
        web_state, base::BindOnce(^{
          if (weak_wrapper) {
            [weak_wrapper populatePageContextFieldsAsync];
          }
        }));
  } else {
    [wrapper populatePageContextFieldsAsync];
  }
}

void PopulatePageContextWithTimeout(PageContextWrapper* wrapper,
                                    web::WebState* web_state,
                                    base::TimeDelta timeout) {
  if (web_state->IsLoading()) {
    __weak PageContextWrapper* weak_wrapper = wrapper;
    new SelfDestructivePageLoadObserver(
        web_state, base::BindOnce(^{
          if (weak_wrapper) {
            [weak_wrapper populatePageContextFieldsAsyncWithTimeout:timeout];
          }
        }));
  } else {
    [wrapper populatePageContextFieldsAsyncWithTimeout:timeout];
  }
}

// TODO(crbug.com/465016086): Add helper for serializing and storing page
// context locally.
