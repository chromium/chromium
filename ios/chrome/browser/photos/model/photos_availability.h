// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_AVAILABILITY_H_

class ChromeBrowserState;

// Returns whether the Save to Photos entry point can be presented for a given
// browser state.
bool IsSaveToPhotosAvailable(ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_AVAILABILITY_H_
