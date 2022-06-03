// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/undo/bookmark_undo_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace ios {

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<BookmarkUndoService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<BookmarkUndoService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
BookmarkUndoServiceFactory* BookmarkUndoServiceFactory::GetInstance() {
  static base::NoDestructor<BookmarkUndoServiceFactory> instance;
  return instance.get();
}

BookmarkUndoServiceFactory::BookmarkUndoServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BookmarkUndoService",
          BrowserStateDependencyManager::GetInstance()) {
}

BookmarkUndoServiceFactory::~BookmarkUndoServiceFactory() {
}

std::unique_ptr<KeyedService>
BookmarkUndoServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return base::WrapUnique(new BookmarkUndoService);
}

}  // namespace ios
