// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_UTILS_APP_UTILS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_UTILS_APP_UTILS_API_H_

#import <Foundation/Foundation.h>

class Browser;

namespace base {
class CommandLine;
}

namespace ios {
namespace provider {

// Initializes global provider state. Must be called as soon as possible
// in the application startup code. It is safe to call it multiple times
// in unit tests.
void Initialize();

// Appends additional command-line flags. Called before web startup.
void AppendSwitchesFromExperimentalSettings(
    NSUserDefaults* experimental_settings,
    base::CommandLine* command_line);

// Attaches any provider-specific browser agents to the given `browser`.
void AttachBrowserAgents(Browser* browser);

// Sets whether metrics reporting is enabled for provider-specific services.
void SetMetricsReportingEnabled(bool enabled);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_UTILS_APP_UTILS_API_H_
