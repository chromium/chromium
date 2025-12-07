// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/bookmark_adder.h"

#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "url/gurl.h"

BookmarkAdder::BookmarkAdder(const GURL& url_to_add, std::string title_to_add)
    : title_to_add_(title_to_add), url_to_add_(url_to_add) {
  profile_manager_observation_.Observe(
      GetApplicationContext()->GetProfileManager());
}

BookmarkAdder::~BookmarkAdder() {}

void BookmarkAdder::AddUrlToModel(bookmarks::BookmarkModel* model) {
  const bookmarks::BookmarkNode* defaultFolder = model->account_mobile_node();
  if (defaultFolder) {
    model->AddNewURL(model->account_mobile_node(), 0,
                     base::UTF8ToUTF16(title_to_add_), url_to_add_);
  } else {
    model->AddNewURL(model->mobile_node(), 0, base::UTF8ToUTF16(title_to_add_),
                     url_to_add_);
  }

  std::move(completion_).Run();
}

void BookmarkAdder::OnProfileLoaded(ScopedProfileKeepAliveIOS keep_alive,
                                    base::OnceClosure completion) {
  if (!keep_alive.profile()) {
    // Profile could not be loaded, abort the bookmark addition.
    std::move(completion).Run();
    return;
  }
  keep_alive_ = std::move(keep_alive);
  completion_ = std::move(completion);

  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForProfile(keep_alive_.profile());

  if (!model->loaded()) {
    bookmark_model_observation_.Observe(model);
    return;
  }

  BookmarkAdder::AddUrlToModel(model);
}

void BookmarkAdder::OnProfileManagerWillBeDestroyed(
    ProfileManagerIOS* manager) {
  keep_alive_ = ScopedProfileKeepAliveIOS{};
  bookmark_model_observation_.Reset();
  if (completion_) {
    std::move(completion_).Run();
  }
}

void BookmarkAdder::OnProfileLoaded(ProfileManagerIOS* manager,
                                    ProfileIOS* profile) {}
void BookmarkAdder::OnProfileManagerDestroyed(ProfileManagerIOS* manager) {
  profile_manager_observation_.Reset();
}
void BookmarkAdder::OnProfileCreated(ProfileManagerIOS* manager,
                                     ProfileIOS* profile) {}
void BookmarkAdder::OnProfileUnloaded(ProfileManagerIOS* manager,
                                      ProfileIOS* profile) {}
void BookmarkAdder::OnProfileMarkedForPermanentDeletion(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {}

void BookmarkAdder::BookmarkModelLoaded(bool id_reassigned) {
  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForProfile(keep_alive_.profile());
  AddUrlToModel(model);
}

void BookmarkAdder::BookmarkModelChanged() {}
