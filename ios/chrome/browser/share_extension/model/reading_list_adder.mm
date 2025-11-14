// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/reading_list_adder.h"

#import "base/strings/utf_string_conversions.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "url/gurl.h"

ReadingListAdder::ReadingListAdder(const GURL& url_to_add,
                                   std::string title_to_add)
    : title_to_add_(title_to_add), url_to_add_(url_to_add) {
  profile_manager_observation_.Observe(
      GetApplicationContext()->GetProfileManager());
}

ReadingListAdder::~ReadingListAdder() {}

void ReadingListAdder::AddUrlToReadingListModel(ReadingListModel* model) {
  model->AddOrReplaceEntry(url_to_add_, title_to_add_,
                           reading_list::ADDED_VIA_EXTENSION,
                           /*estimated_read_time=*/std::nullopt,
                           /*creation_time=*/std::nullopt);
  std::move(completion_).Run();
}

void ReadingListAdder::OnProfileLoaded(ScopedProfileKeepAliveIOS keep_alive,
                                       base::OnceClosure completion) {
  if (!keep_alive.profile()) {
    // Profile could not be loaded, abort the reading list item addition.
    std::move(completion).Run();
    return;
  }
  keep_alive_ = std::move(keep_alive);
  completion_ = std::move(completion);

  ReadingListModel* model =
      ReadingListModelFactory::GetForProfile(keep_alive_.profile());

  if (!model->loaded()) {
    reading_list_model_observation_.Observe(model);
    return;
  }

  ReadingListAdder::AddUrlToReadingListModel(model);
}

void ReadingListAdder::OnProfileManagerWillBeDestroyed(
    ProfileManagerIOS* manager) {
  keep_alive_ = ScopedProfileKeepAliveIOS{};
  reading_list_model_observation_.Reset();
  if (completion_) {
    std::move(completion_).Run();
  }
}

void ReadingListAdder::OnProfileLoaded(ProfileManagerIOS* manager,
                                       ProfileIOS* profile) {}
void ReadingListAdder::OnProfileManagerDestroyed(ProfileManagerIOS* manager) {
  profile_manager_observation_.Reset();
}
void ReadingListAdder::OnProfileCreated(ProfileManagerIOS* manager,
                                        ProfileIOS* profile) {}
void ReadingListAdder::OnProfileUnloaded(ProfileManagerIOS* manager,
                                         ProfileIOS* profile) {}
void ReadingListAdder::OnProfileMarkedForPermanentDeletion(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {}

void ReadingListAdder::ReadingListModelLoaded(const ReadingListModel* model) {
  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetForProfile(keep_alive_.profile());
  AddUrlToReadingListModel(reading_list_model);
}
