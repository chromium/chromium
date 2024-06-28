// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/shell/app/shell_main_delegate_mac.h"

#include <unistd.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/shell/app/paths_mac.h"
#include "content/shell/browser/shell_application_mac.h"
#include "content/shell/common/shell_switches.h"

namespace content {

void EnsureCorrectResolutionSettings() {
  // Exit early if this isn't a browser process.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessType)) {
    return;
  }

  NSString* const kHighResolutionCapable = @"NSHighResolutionCapable";
  NSURL* info_plist = base::apple::FilePathToNSURL(GetInfoPlistPath());
  NSMutableDictionary* info_dict =
      [[NSMutableDictionary alloc] initWithContentsOfURL:info_plist error:nil];

  bool running_web_tests = switches::IsRunWebTestsSwitchPresent();
  NSNumber* high_resolution_capable_from_info_dict =
      info_dict[kHighResolutionCapable];
  bool not_high_resolution_capable =
      high_resolution_capable_from_info_dict &&
      !high_resolution_capable_from_info_dict.boolValue;
  if (running_web_tests == not_high_resolution_capable) {
    return;
  }

  // We need to update our Info.plist before we can continue.
  info_dict[kHighResolutionCapable] = @(!running_web_tests);
  CHECK([info_dict writeToURL:info_plist error:nil]);

  const base::CommandLine::StringVector& original_argv =
      base::CommandLine::ForCurrentProcess()->argv();
  char** argv = new char*[original_argv.size() + 1];
  for (unsigned i = 0; i < original_argv.size(); ++i) {
    argv[i] = const_cast<char*>(original_argv.at(i).c_str());
  }
  argv[original_argv.size()] = nullptr;

  CHECK(execvp(argv[0], argv));
}

void RegisterShellCrApp() {
  // Force the NSApplication subclass to be used.
  [ShellCrApplication sharedApplication];

  // If there was an invocation to NSApp prior to this method, then the NSApp
  // will not be a ShellCrApplication, but will instead be an NSApplication.
  // This is undesirable and we must enforce that this doesn't happen.
  CHECK([NSApp isKindOfClass:[ShellCrApplication class]]);
}

}  // namespace content
