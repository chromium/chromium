// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<BookmarkUndoService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<BookmarkUndoService*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
BookmarkUndoServiceFactory* BookmarkUndoServiceFactory::GetInstance() {
  static base::NoDestructor<BookmarkUndoServiceFactory> instance;
  return instance.get();
}

BookmarkUndoServiceFactory::BookmarkUndoServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkUndoService",
          BrowserStateDependencyManager::GetInstance()) {}

BookmarkUndoServiceFactory::~BookmarkUndoServiceFactory() {}

std::unique_ptr<KeyedService>
BookmarkUndoServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return base::WrapUnique(new BookmarkUndoService);
}

}  // namespace ios
