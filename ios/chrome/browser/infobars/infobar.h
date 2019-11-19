// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_H_

#import <UIKit/UIKit.h>
#include <memory>

#include "base/macros.h"
#include "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_controller_delegate.h"

@protocol InfobarUIDelegate;
namespace infobars {
class InfoBarDelegate;
}

// The iOS version of infobars::InfoBar.
class InfoBarIOS : public infobars::InfoBar, public InfoBarControllerDelegate {
 public:
  InfoBarIOS(id<InfobarUIDelegate> controller,
             std::unique_ptr<infobars::InfoBarDelegate> delegate);
  ~InfoBarIOS() override;

  // Returns the InfobarUIDelegate associated to this Infobar.
  id<InfobarUIDelegate> InfobarUIDelegate();

  // Remove the infobar view from infobar container view.
  void RemoveView();

 private:
  // InfoBarControllerDelegate overrides:
  bool IsOwned() override;
  void RemoveInfoBar() override;

  id<InfobarUIDelegate> controller_;
  DISALLOW_COPY_AND_ASSIGN(InfoBarIOS);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_H_
