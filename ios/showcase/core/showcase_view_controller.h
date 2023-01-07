// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_CORE_SHOWCASE_VIEW_CONTROLLER_H_
#define IOS_SHOWCASE_CORE_SHOWCASE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace showcase {

// Required name for class that is intended to be tested.
extern NSString* const kClassForDisplayKey;

// Required name for class that will be instantiated.
extern NSString* const kClassForInstantiationKey;

// Optional description of the use case for the row.
extern NSString* const kUseCaseKey;

// Type for one row of model.
typedef NSDictionary<NSString*, NSString*> ModelRow;

}  // namespace showcase

// TableViewController that displays a searchable list of rows.
@interface ShowcaseViewController : UITableViewController
// Initializes the tableView with a list of rows.
- (instancetype)initWithRows:(NSArray<showcase::ModelRow*>*)rows
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
@end

#endif  // IOS_SHOWCASE_CORE_SHOWCASE_VIEW_CONTROLLER_H_
