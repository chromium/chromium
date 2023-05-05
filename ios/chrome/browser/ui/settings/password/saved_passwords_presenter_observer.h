// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_SAVED_PASSWORDS_PRESENTER_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_SAVED_PASSWORDS_PRESENTER_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

@protocol SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange;

@end

// Simple observer bridge that forwards all events to its delegate observer.
class SavedPasswordsPresenterObserverBridge
    : public password_manager::SavedPasswordsPresenter::Observer {
 public:
  SavedPasswordsPresenterObserverBridge(
      id<SavedPasswordsPresenterObserver> delegate,
      password_manager::SavedPasswordsPresenter* presenter);
  ~SavedPasswordsPresenterObserverBridge() override;

  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

 private:
  __weak id<SavedPasswordsPresenterObserver> delegate_ = nil;
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      saved_passwords_presenter_observer_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_SAVED_PASSWORDS_PRESENTER_OBSERVER_H_
