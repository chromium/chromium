// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_OPTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_OPTIONS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_mutator.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"

class WebStateList;

namespace dom_distiller {
class DistilledPagePrefs;
}

@protocol ReaderModeOptionsConsumer;

// Mediator for the reader mode options.
@interface ReaderModeOptionsMediator : NSObject <ReaderModeOptionsMutator>

@property(nonatomic, weak) id<ReaderModeOptionsConsumer> consumer;

@property(nonatomic, weak) id<ReaderModeCommands> readerModeHandler;

// Initializer.
- (instancetype)initWithDistilledPagePrefs:
                    (dom_distiller::DistilledPagePrefs*)distilledPagePrefs
                              webStateList:(WebStateList*)webStateList;

// Disconnects from the model layer.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_OPTIONS_MEDIATOR_H_
