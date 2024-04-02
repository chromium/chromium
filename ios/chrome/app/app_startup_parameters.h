// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APP_STARTUP_PARAMETERS_H_
#define IOS_CHROME_APP_APP_STARTUP_PARAMETERS_H_

#import <Foundation/Foundation.h>

#include <map>
#include <string>
#include <vector>

// Input format for the `TabOpening` protocol.
enum class ApplicationModeForTabOpening {
  NORMAL,
  INCOGNITO,
  CURRENT,
  UNDETERMINED
};

enum TabOpeningPostOpeningAction {
  // No action should be done
  NO_ACTION = 0,
  START_VOICE_SEARCH,
  START_QR_CODE_SCANNER,
  START_LENS_FROM_HOME_SCREEN_WIDGET,
  START_LENS_FROM_APP_ICON_LONG_PRESS,
  START_LENS_FROM_SPOTLIGHT,
  FOCUS_OMNIBOX,
  SHOW_DEFAULT_BROWSER_SETTINGS,
  SEARCH_PASSWORDS,
  OPEN_READING_LIST,
  OPEN_BOOKMARKS,
  OPEN_RECENT_TABS,
  OPEN_TAB_GRID,
  SET_CHROME_DEFAULT_BROWSER,
  VIEW_HISTORY,
  OPEN_PAYMENT_METHODS,
  RUN_SAFETY_CHECK,
  MANAGE_PASSWORDS,
  MANAGE_SETTINGS,
  OPEN_LATEST_TAB,
  START_LENS_FROM_INTENTS,
  OPEN_CLEAR_BROWSING_DATA_DIALOG,
  TAB_OPENING_POST_OPENING_ACTION_COUNT,
  ADD_BOOKMARKS,
  ADD_READING_LIST_ITEMS,
  EXTERNAL_ACTION_SHOW_BROWSER_SETTINGS,
};

class GURL;

// This class stores all the parameters relevant to the app startup in case
// of launch from another app.
@interface AppStartupParameters : NSObject

// The URL that should be opened. This may not always be the same URL as the one
// that was received. The reason for this is in the case of Universal Link
// navigation where we may want to open up a fallback URL e.g., the New Tab Page
// instead of the actual universal link. If this URL is empty, a new tab page
// will be created upon app open iff there is no active tab.
@property(nonatomic, readonly, assign) const GURL& externalURL;

// Original URL that should be opened. May or may not be the same as
// `externalURL`.
@property(nonatomic, readonly, assign) const GURL& completeURL;

// The list of URLs to open. First URL in the vector is the same
// as `externalURL`.
@property(nonatomic, readonly, assign) const std::vector<GURL>& URLs;

// The URL query string parameters in the case that the app was launched as a
// result of Universal Link navigation. The map associates query string
// parameters with their corresponding value.
@property(nonatomic, assign) std::map<std::string, std::string>
    externalURLParams;

// The list of inputted URLs to process. These URLs aren't automatically opened.
// Used in the context of Siri shortcuts that allow URL inputs that are not
// meant to be opened in new tabs automatically.
@property(nonatomic, readwrite, strong) NSArray<NSURL*>* inputURLs;

// The mode in which the tab must be opened. Defaults to UNDETERMINED.
@property(nonatomic, assign) ApplicationModeForTabOpening applicationMode;
// Action to be taken after loading the URL.
@property(nonatomic, readwrite, assign)
    TabOpeningPostOpeningAction postOpeningAction;
// Boolean to track if a Payment Request response is requested at startup.
@property(nonatomic, readwrite, assign) BOOL completePaymentRequest;
// When this flag is set, attempt to open `externalURL` in an existing tab.
@property(nonatomic, readwrite, assign) BOOL openExistingTab;
// Text query that should be executed on startup.
@property(nonatomic, readwrite, copy) NSString* textQuery;
// Data for UIImage for image query that should be executed on startup.
@property(nonatomic, readwrite, strong) NSData* imageSearchData;
// Boolean to track if the app is open in an user unexpected mode.
// When a certain enterprise policy has been set, it's possible that one browser
// mode is disabled. When the user intends to open an unavailable mode of
// Chrome, the browser won't proceed in that disabled mode, and it will signal
// to the user that a different mode is opened.
@property(nonatomic, readwrite, getter=isUnexpectedMode) BOOL unexpectedMode;
// Boolean to track whether the app was opened via a custom scheme from another
// first-party app.
@property(nonatomic, readwrite, assign) BOOL openedViaFirstPartyScheme;
// Boolean to track whether the app was opened via widget.
@property(nonatomic, readwrite, assign) BOOL openedViaWidgetScheme;
// Boolean to track whether the app was opened via URL.
@property(nonatomic, readwrite, assign) BOOL openedWithURL;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL
                    applicationMode:(ApplicationModeForTabOpening)mode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithURLs:(const std::vector<GURL>&)URLs
             applicationMode:(ApplicationModeForTabOpening)mode;

@end

#endif  // IOS_CHROME_APP_APP_STARTUP_PARAMETERS_H_
