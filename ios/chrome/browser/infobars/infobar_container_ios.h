// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_IOS_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_IOS_H_

#include "components/infobars/core/infobar_container.h"

#include "base/macros.h"

@protocol InfobarContainerConsumer;

// IOS infobar container specialization, managing infobars visibility and
// presentation via the InfobarContainerConsumer protocol.|legacyConsumer| is
// used to support the legacy InfobarPresentation concurrently with the new one
// that uses |consumer|.
class InfoBarContainerIOS : public infobars::InfoBarContainer {
 public:
  InfoBarContainerIOS(id<InfobarContainerConsumer> consumer,
                      id<InfobarContainerConsumer> legacyConsumer);
  ~InfoBarContainerIOS() override;

  // Changes the InfoBarManager for which this container is showing infobars.
  // This will hide all current infobars, remove them from the container, add
  // the infobars from |infobar_manager|, and show them all. If
  // |infobar_manager| is nullptr, it will hide all current Infobars but won't
  // be able to present new ones.
  void ChangeInfoBarManager(infobars::InfoBarManager* infobar_manager);

 protected:
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificInfoBarStateChanged(bool is_animating) override;

 private:
  infobars::InfoBarManager* info_bar_manager_ = nullptr;
  __weak id<InfobarContainerConsumer> consumer_;
  __weak id<InfobarContainerConsumer> legacyConsumer_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerIOS);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTAINER_IOS_H_
