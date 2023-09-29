// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_OBSERVER_HELPER_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_OBSERVER_HELPER_H_

class Browser;
class SessionRestorationObserver;

// There are two separate classes that can notify of session restoration
// events depending on UseSessionSerializationOptimizations(). This file
// provides helper function abstracting the registration/unregistration
// of SessionRestorationObservers.
//
// TODO(crbug.com/1383087): remove once the feature is fully launched.

// Registers `observer` as a SessionRestorationObserver for `browser`.
void AddSessionRestorationObserver(Browser* browser,
                                   SessionRestorationObserver* observer);

// Unregisters `observer` from SessionRestorationObservers of `browser`.
void RemoveSessionRestorationObserver(Browser* browser,
                                      SessionRestorationObserver* observer);

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_OBSERVER_HELPER_H_
