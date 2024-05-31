// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_SAVED_PASSWORDS_PRESENTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_SAVED_PASSWORDS_PRESENTER_FACTORY_H_

class ChromeBrowserState;

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

namespace IOSChromeSavedPasswordsPresenterFactory {

// Constructs a SavedPasswordsPresenter for the provided browser state.
// Note that a new SavedPasswordsPresenter object is created every time
// GetForBrowserState is called, because SavedPasswordsPresenter's
// initialization can only be done once.
password_manager::SavedPasswordsPresenter* GetForBrowserState(
    ChromeBrowserState* browser_state);

}  // namespace IOSChromeSavedPasswordsPresenterFactory

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_SAVED_PASSWORDS_PRESENTER_FACTORY_H_
