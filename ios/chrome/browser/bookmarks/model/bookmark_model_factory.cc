// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/core_bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#include "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

// Merges `list1` and `list2` to produce a third list where there is at most one
// entry per URL. In case deduplication took place, the resulting title is
// arbitrary.
std::vector<bookmarks::UrlAndTitle> MergeTitlesByUniqueUrl(
    std::vector<bookmarks::UrlAndTitle> list1,
    std::vector<bookmarks::UrlAndTitle> list2) {
  // This implementation is not the most efficient one, but performance is not
  // particularly concerning given the current callers.
  std::map<GURL, std::u16string> titles_per_url;
  for (const bookmarks::UrlAndTitle& url_and_title : list1) {
    titles_per_url.emplace(url_and_title.url, url_and_title.title);
  }
  for (const bookmarks::UrlAndTitle& url_and_title : list2) {
    titles_per_url.emplace(url_and_title.url, url_and_title.title);
  }

  std::vector<bookmarks::UrlAndTitle> merged_list;
  merged_list.reserve(titles_per_url.size());
  for (const auto& url_and_title : titles_per_url) {
    merged_list.emplace_back(url_and_title.first, url_and_title.second);
  }
  return merged_list;
}

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

  bool loaded() const override {
    return model1_->loaded() && model2_->loaded();
  }

  bool IsBookmarked(const GURL& url) const override {
    return model1_->IsBookmarked(url) || model2_->IsBookmarked(url);
  }

  size_t GetNodeCountByURL(const GURL& url) const override {
    return model1_->GetNodeCountByURL(url) || model2_->GetNodeCountByURL(url);
  }

  std::vector<std::u16string_view> GetNodeTitlesByURL(
      const GURL& url) const override {
    std::vector<std::u16string_view> titles = model1_->GetNodeTitlesByURL(url);
    base::Extend(titles, model2_->GetNodeTitlesByURL(url));
    return titles;
  }

  [[nodiscard]] std::vector<bookmarks::UrlAndTitle> GetUniqueUrls()
      const override {
    return MergeTitlesByUniqueUrl(model1_->GetUniqueUrls(),
                                  model2_->GetUniqueUrls());
  }

  std::vector<bookmarks::TitledUrlMatch> GetBookmarksMatching(
      const std::u16string& query,
      size_t max_count_hint,
      query_parser::MatchingAlgorithm matching_algorithm) const override {
    std::vector<bookmarks::TitledUrlMatch> matches =
        model1_->GetBookmarksMatching(query, max_count_hint,
                                      matching_algorithm);
    base::Extend(matches, model2_->GetBookmarksMatching(query, max_count_hint,
                                                        matching_algorithm));
    return matches;
  }

  void RemoveAllUserBookmarks() override {
    model1_->RemoveAllUserBookmarks();
    model2_->RemoveAllUserBookmarks();
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
