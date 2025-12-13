// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/ios_share_url_interception_context.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"

namespace data_sharing {

IOSShareURLInterceptionContext::IOSShareURLInterceptionContext(Browser* browser)
    : weak_browser(browser ? browser->AsWeakPtr() : nullptr) {}

IOSShareURLInterceptionContext::~IOSShareURLInterceptionContext() {}

}  // namespace data_sharing
