// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_KEYED_SERVICE_FACTORIES_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_KEYED_SERVICE_FACTORIES_H_

// Instantiates all KeyedService factories which is especially important for
// services that register preferences or that should be created at browser
// state creation time (as opposed to lazily on first access).
void EnsureBrowserStateKeyedServiceFactoriesBuilt();

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_BROWSER_STATE_KEYED_SERVICE_FACTORIES_H_
