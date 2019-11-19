// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/test_url_loading_service.h"

#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestUrlLoadingService::TestUrlLoadingService(UrlLoadingNotifier* notifier)
    : UrlLoadingService(notifier) {}

void TestUrlLoadingService::LoadUrlInCurrentTab(const UrlLoadParams& params) {
  last_params = params;
  load_current_tab_call_count++;
}

void TestUrlLoadingService::LoadUrlInNewTab(const UrlLoadParams& params) {
  last_params = params;
  load_new_tab_call_count++;
}

void TestUrlLoadingService::SwitchToTab(const UrlLoadParams& params) {
  last_params = params;
  switch_tab_call_count++;
}
