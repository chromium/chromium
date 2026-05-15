// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"

#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"
#import "ios/chrome/browser/level_up/ui/level_up_task.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@implementation LevelUpMediator

- (void)setConsumer:(id<LevelUpConsumer>)consumer {
  _consumer = consumer;

  int level = 1;

  LevelUpTask* task1 = [[LevelUpTask alloc]
        initWithTaskID:@"default_browser"
                 title:@"Set Chrome as Default Browser"
       taskDescription:@"Use Chrome as your primary default browser"
        iconSymbolName:kDefaultBrowserSymbol
             completed:YES
      navigationAction:^{
      }];
  LevelUpTask* task2 =
      [[LevelUpTask alloc] initWithTaskID:@"bookmarks_sync"
                                    title:@"Sync your bookmarks"
                          taskDescription:@"Access your bookmarks on any device"
                           iconSymbolName:kBookmarksSymbol
                                completed:YES
                         navigationAction:^{
                         }];
  LevelUpTask* task3 = [[LevelUpTask alloc]
        initWithTaskID:@"password_manager"
                 title:@"Enable Password Manager"
       taskDescription:@"Check and autofill your passwords securely"
        iconSymbolName:kKeySymbol
             completed:YES
      navigationAction:^{
      }];
  LevelUpTask* task4 = [[LevelUpTask alloc]
        initWithTaskID:@"widgets"
                 title:@"Add widgets to Home Screen"
       taskDescription:@"Access search and bookmarks from the Home Screen"
        iconSymbolName:kPlusInSquareSymbol
             completed:NO
      navigationAction:^{
      }];

  [self.consumer setLevel:level tasksForLevel:@[ task1, task2, task3, task4 ]];
}

@end
