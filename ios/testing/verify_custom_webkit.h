// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_VERIFY_CUSTOM_WEBKIT_H_
#define IOS_TESTING_VERIFY_CUSTOM_WEBKIT_H_

// Returns whether custom WebKit frameworks were loaded if
// --run-with-custom-webkit was passed on the commandline.  Otherwise, always
// returns true.
bool IsCustomWebKitLoadedIfRequested();

#endif  // IOS_TESTING_VERIFY_CUSTOM_WEBKIT_H_
