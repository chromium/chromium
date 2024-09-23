// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_CONSUMER_H_

@class TableViewIdentityItem;

// Consumer for the IdentityChooser.
@protocol IdentityChooserConsumer

// Sets the `items` displayed by this consumer.
- (void)setIdentityItems:(NSArray<TableViewIdentityItem*>*)items;

// Notifies the consumer that the `changedItem` has changed.
- (void)itemHasChanged:(TableViewIdentityItem*)changedItem;

// Returns an TableViewIdentityItem based on a gaia ID.
- (TableViewIdentityItem*)tableViewIdentityItemWithGaiaID:(NSString*)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_CONSUMER_H_
