// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_URL_LOADER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_URL_LOADER_H_

class GURL;

// Protocol for loading URLs in the AIM prototype.
@protocol AIMPrototypeURLLoader

- (void)loadURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_URL_LOADER_H_
