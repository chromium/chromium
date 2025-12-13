// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_BOOKMARK_ADDER_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_BOOKMARK_ADDER_H_

#import "base/functional/callback.h"
#import "base/scoped_observation.h"
#import "components/bookmarks/browser/base_bookmark_model_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "url/gurl.h"

namespace bookmarks {
class BookmarkModel;
}

class ProfileManagerIOS;
class ScopedProfileKeepAliveIOS;

// A class to load a given profile and bookmark a URL for that given profile. It
// acts as an observer for both the profile (loaded/destroyed etc) and
// bookmarks.
class BookmarkAdder : public ProfileManagerObserverIOS,
                      public bookmarks::BaseBookmarkModelObserver {
 public:
  BookmarkAdder(const GURL& url_to_add, std::string title_to_add);

  BookmarkAdder(const BookmarkAdder&) = delete;
  BookmarkAdder& operator=(const BookmarkAdder&) = delete;

  ~BookmarkAdder() override;

  // ProfileManagerObserverIOS:
  void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileCreated(ProfileManagerIOS* manager,
                        ProfileIOS* profile) override;
  void OnProfileLoaded(ProfileManagerIOS* manager,
                       ProfileIOS* profile) override;
  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) override;
  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) override;

  // BookmarkModelObserver
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelChanged() override;

  void OnProfileLoaded(ScopedProfileKeepAliveIOS keep_alive,
                       base::OnceClosure completion);

 private:
  const std::string title_to_add_;
  const GURL url_to_add_;
  base::OnceClosure completion_;
  ScopedProfileKeepAliveIOS keep_alive_;
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      profile_manager_observation_{this};
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observation_{this};

  void AddUrlToModel(bookmarks::BookmarkModel* model);
};

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_BOOKMARK_ADDER_H_
