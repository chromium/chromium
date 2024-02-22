// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"

#include <memory>
#include <utility>

#include "base/containers/extend.h"
#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/core_bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

// An implementation of CoreBookmarkModel that exposes a merged view of two
// underlying BookmarkModel instances.
class MergedBookmarkModel : public bookmarks::CoreBookmarkModel {
 public:
  MergedBookmarkModel(bookmarks::CoreBookmarkModel* model1,
                      bookmarks::CoreBookmarkModel* model2)
      : model1_(model1), model2_(model2) {
    CHECK(model1_);
    CHECK(model2_);
  }

  ~MergedBookmarkModel() override = default;

  bool IsBookmarked(const GURL& url) const override {
    return model1_->IsBookmarked(url) || model2_->IsBookmarked(url);
  }

 private:
  const raw_ptr<bookmarks::CoreBookmarkModel> model1_;
  const raw_ptr<bookmarks::CoreBookmarkModel> model2_;
};

std::unique_ptr<KeyedService> BuildBookmarkModel(web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<MergedBookmarkModel>(
      LocalOrSyncableBookmarkModelFactory::GetForBrowserState(browser_state),
      AccountBookmarkModelFactory::GetForBrowserState(browser_state));
}

}  // namespace

// static
bookmarks::CoreBookmarkModel* BookmarkModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::CoreBookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
bookmarks::CoreBookmarkModel* BookmarkModelFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<bookmarks::CoreBookmarkModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
BookmarkModelFactory* BookmarkModelFactory::GetInstance() {
  static base::NoDestructor<BookmarkModelFactory> instance;
  return instance.get();
}

// static
BookmarkModelFactory::TestingFactory BookmarkModelFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildBookmarkModel);
}

BookmarkModelFactory::BookmarkModelFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountBookmarkModelFactory::GetInstance());
  DependsOn(ios::LocalOrSyncableBookmarkModelFactory::GetInstance());
}

BookmarkModelFactory::~BookmarkModelFactory() {}

std::unique_ptr<KeyedService> BookmarkModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildBookmarkModel(context);
}

web::BrowserState* BookmarkModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool BookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
