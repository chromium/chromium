// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_CHROME_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_CHROME_TEST_UTIL_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"

@protocol ApplicationCommands;
@protocol CountryCodePickerCommands;
@protocol UnitConversionCommands;
@protocol DriveFilePickerCommands;

class Browser;
@class MainController;
@class NewTabPageController;
@class SceneController;
@class SceneState;
@class UIViewController;

namespace chrome_test_util {

// Returns the main controller.
MainController* GetMainController();

// Returns the foreground, active scene.
SceneState* GetForegroundActiveScene();

// Returns the foreground, active scene controller.
SceneController* GetForegroundActiveSceneController();

// Returns the number of regular Browsers for the default profile.
NSUInteger RegularBrowserCount();

// Returns the current, non-incognito Profile.
ProfileIOS* GetOriginalProfile();

// Returns the current incognito Profile
ProfileIOS* GetCurrentIncognitoProfile();

// Returns the browser for the main interface.
Browser* GetMainBrowser();

// Returns the current browser from the foreground active scene.
Browser* GetCurrentBrowser();

// Returns the active view controller.
// NOTE: It is preferred to not directly access the active view controller if
// possible.
UIViewController* GetActiveViewController();

// Returns the dispatcher for the active Browser.
id<ApplicationCommands,
   BrowserCommands,
   BrowserCoordinatorCommands,
   UnitConversionCommands,
   CountryCodePickerCommands,
   DriveFilePickerCommands>
HandlerForActiveBrowser();

// Removes all presented infobars.
void RemoveAllInfoBars();

// Dismisses all presented views and modal dialogs. `completion` is invoked when
// all the views are dismissed.
void ClearPresentedState(ProceduralBlock completion);

// Presents the signed in accounts view controller if conditions to be presented
// are met.
void PresentSignInAccountsViewControllerIfNecessary();

// Sets the value of a boolean local state pref.
// TODO(crbug.com/41275546): Clean up other tests that use this helper function.
void SetBooleanLocalStatePref(const char* pref_name, bool value);

// Sets the value of a boolean user pref in the given profile.
void SetBooleanUserPref(ProfileIOS* profile, const char* pref_name, bool value);

// Sets the value of an integer user pref in the given profile.
void SetIntegerUserPref(ProfileIOS* profile, const char* pref_name, int value);

// Checks whether metrics recording is enabled or not.
bool IsMetricsRecordingEnabled();

// Checks whether metrics reporting is enabled or not.
bool IsMetricsReportingEnabled();

// Checks whether crashpad recording is enabled or not.
bool IsCrashpadEnabled();

// Checks whether crashpad reporting is enabled or not.
bool IsCrashpadReportingEnabled();

// Simulates launching Chrome from another application.
void OpenChromeFromExternalApp(const GURL& url);

// Purges cached web view page, so the next time back navigation will not use
// cached page. Browsers don't have to use fresh version for back forward
// navigation for HTTP pages and may serve version from the cache even if
// Cache-Control response header says otherwise.
[[nodiscard]] bool PurgeCachedWebViewPages();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_CHROME_TEST_UTIL_H_
