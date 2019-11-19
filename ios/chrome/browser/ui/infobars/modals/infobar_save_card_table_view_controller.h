// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol InfobarSaveCardModalDelegate;
@class SaveCardMessageWithLinks;

// InfobarSaveCardTableViewController represents the content for the Save Card
// InfobarModal.
@interface InfobarSaveCardTableViewController : ChromeTableViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveCardModalDelegate>)modalDelegate NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// Cardholder name to be displayed.
@property(nonatomic, copy) NSString* cardholderName;

// Card Issuer icon image to be displayed.
@property(nonatomic, strong) UIImage* cardIssuerIcon;

// Card Number to be displayed.
@property(nonatomic, copy) NSString* cardNumber;

// Card Expiration Month to be displayed
@property(nonatomic, copy) NSString* expirationMonth;

// Card Expiration Year to be displayed.
@property(nonatomic, copy) NSString* expirationYear;

// Card related Legal Messages to be displayed.
@property(nonatomic, copy)
    NSMutableArray<SaveCardMessageWithLinks*>* legalMessages;

// YES if the Card being displayed has been saved.
@property(nonatomic, assign) BOOL currentCardSaved;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_TABLE_VIEW_CONTROLLER_H_
