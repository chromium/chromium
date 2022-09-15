// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_PIN_ENTRY_VIEW_H_
#define REMOTING_IOS_APP_PIN_ENTRY_VIEW_H_

#import <UIKit/UIKit.h>

@protocol PinEntryDelegate<NSObject>

// Notifies the delegate that a pin has been provided and if we should pair.
@optional
- (void)didProvidePin:(NSString*)pin createPairing:(BOOL)createPairing;

@end

// This view is the container for a PIN entry box, a button to submit, and the
// option box to remember the pairing. All used for entering a PIN based
// passcode.
@interface PinEntryView : UIView

// Clears the pin entry view.
- (void)clearPinEntry;

// This delegate will handle interactions on the cells in the collection.
@property(weak, nonatomic) id<PinEntryDelegate> delegate;

// |supportsPairing| false will hide the remember pin checkbox.
@property(nonatomic) BOOL supportsPairing;

@end

#endif  // REMOTING_IOS_APP_PIN_ENTRY_VIEW_H_
