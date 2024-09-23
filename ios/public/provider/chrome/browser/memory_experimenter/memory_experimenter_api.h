// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MEMORY_EXPERIMENTER_MEMORY_EXPERIMENTER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MEMORY_EXPERIMENTER_MEMORY_EXPERIMENTER_API_H_

#import <memory>

namespace ios::provider {

// Begin memory experimentation.
void BeginMemoryExperimentation();

// Stop memory experimentation.
void StopMemoryExperimentation();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MEMORY_EXPERIMENTER_MEMORY_EXPERIMENTER_API_H_
