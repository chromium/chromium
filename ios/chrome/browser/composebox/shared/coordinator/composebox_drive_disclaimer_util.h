// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_DRIVE_DISCLAIMER_UTIL_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_DRIVE_DISCLAIMER_UTIL_H_

#import <memory>

class Browser;

namespace drive_picker {
class DriveDisclaimerController;
}  // namespace drive_picker

namespace composebox {

// Creates a Drive disclaimer controller for the given browser.
std::unique_ptr<drive_picker::DriveDisclaimerController>
CreateDriveDisclaimerController(Browser* browser);

}  // namespace composebox

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_DRIVE_DISCLAIMER_UTIL_H_
