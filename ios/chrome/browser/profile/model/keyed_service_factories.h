// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_KEYED_SERVICE_FACTORIES_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_KEYED_SERVICE_FACTORIES_H_

// Instantiates all KeyedService factories ensuring they are registered with
// BrowserStateDependencyManager before the the ProfileIOS is created (as
// required by the KeyedService infrastructure).
void EnsureProfileKeyedServiceFactoriesBuilt();

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_KEYED_SERVICE_FACTORIES_H_
