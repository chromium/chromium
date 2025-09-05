// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"

#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<BookmarkUndoService>(
      profile, /*create=*/true);
}

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<BookmarkUndoService>(
      profile, /*create=*/false);
}

// static
BookmarkUndoServiceFactory* BookmarkUndoServiceFactory::GetInstance() {
  static base::NoDestructor<BookmarkUndoServiceFactory> instance;
  return instance.get();
}

BookmarkUndoServiceFactory::BookmarkUndoServiceFactory()
    : ProfileKeyedServiceFactoryIOS("BookmarkUndoService") {}

BookmarkUndoServiceFactory::~BookmarkUndoServiceFactory() = default;

std::unique_ptr<KeyedService>
BookmarkUndoServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<BookmarkUndoService>();
}

}  // namespace ios
