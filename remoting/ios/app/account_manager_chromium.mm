// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/app/account_manager_chromium.h"

#import "remoting/ios/app/remoting_menu_view_controller.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"

namespace {

void ShowMenu() {
  RemotingMenuViewController* menu_view_controller =
      [[RemotingMenuViewController alloc] init];
  [remoting::TopPresentingVC() presentViewController:menu_view_controller
                                            animated:YES
                                          completion:nil];
}

}  // namespace

@interface SimpleAccountParticleDiscViewController : UIViewController
@end

@implementation SimpleAccountParticleDiscViewController

- (void)loadView {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setImage:RemotingTheme.settingsIcon forState:UIControlStateNormal];
  [button addTarget:self
                action:@selector(showMenu)
      forControlEvents:UIControlEventTouchUpInside];
  self.view = button;
}

- (void)showMenu {
  ShowMenu();
}

@end

namespace remoting {
namespace ios {

AccountManagerChromium::AccountManagerChromium() = default;

AccountManagerChromium::~AccountManagerChromium() = default;

UIViewController*
AccountManagerChromium::CreateAccountParticleDiscViewController() {
  return [[SimpleAccountParticleDiscViewController alloc] initWithNibName:nil
                                                                   bundle:nil];
}

void AccountManagerChromium::PresentSignInMenu() {
  ShowMenu();
}

}  // namespace ios
}  // namespace remoting
