// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_UTILS_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_UTILS_H_

#include <memory>

class ConfirmInfoBarDelegate;

namespace infobars {
class InfoBar;
}

// Returns a confirm infobar that owns `delegate`.
std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate);

// Returns a confirm infobar with high priority presentation that owns
// `delegate`.
// TODO (crbug.com/961343):Reassess this method once there's more clarity on how
// to handle queueing and if priorities are actually needed.
std::unique_ptr<infobars::InfoBar> CreateHighPriorityConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate);

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_UTILS_H_
