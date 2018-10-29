// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cocoa/system_hotkey_helper_mac.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/cocoa/system_hotkey_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

NSString* kSystemHotkeyPlistExtension =
    @"/Preferences/com.apple.symbolichotkeys.plist";

// Amount of time to delay loading the hotkeys in seconds.
const int kLoadHotkeysDelaySeconds = 10;

}  // namespace

namespace content {

// static
SystemHotkeyHelperMac* SystemHotkeyHelperMac::GetInstance() {
  return base::Singleton<SystemHotkeyHelperMac>::get();
}

void SystemHotkeyHelperMac::DeferredLoadSystemHotkeys() {
  base::PostDelayedTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::Bind(&SystemHotkeyHelperMac::LoadSystemHotkeys,
                 base::Unretained(this)),
      base::TimeDelta::FromSeconds(kLoadHotkeysDelaySeconds));
}

SystemHotkeyHelperMac::SystemHotkeyHelperMac() : map_(new SystemHotkeyMap) {
}

SystemHotkeyHelperMac::~SystemHotkeyHelperMac() {
}

void SystemHotkeyHelperMac::LoadSystemHotkeys() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  std::string library_path(base::mac::GetUserLibraryPath().value());
  NSString* expanded_file_path =
      [NSString stringWithFormat:@"%s%@",
                                 library_path.c_str(),
                                 kSystemHotkeyPlistExtension];

  // Loads the file into memory.
  NSData* data = [NSData dataWithContentsOfFile:expanded_file_path];
  // Intentionally create the object with +1 retain count, as FileDidLoad
  // will destroy the object.
  NSDictionary* dictionary = [SystemHotkeyMap::DictionaryFromData(data) retain];

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::Bind(&SystemHotkeyHelperMac::FileDidLoad,
                                      base::Unretained(this), dictionary));
}

void SystemHotkeyHelperMac::FileDidLoad(NSDictionary* dictionary) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool success = map()->ParseDictionary(dictionary);
  UMA_HISTOGRAM_BOOLEAN("OSX.SystemHotkeyMap.LoadSuccess", success);
  [dictionary release];
}

}  // namespace content
