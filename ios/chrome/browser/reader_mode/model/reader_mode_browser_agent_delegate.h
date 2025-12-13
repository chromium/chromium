// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_DELEGATE_H_

#import <Foundation/Foundation.h>

class ReaderModeBrowserAgent;

// Delegate for ReaderModeBrowserAgent.
@protocol ReaderModeBrowserAgentDelegate

// Shows the reader mode content.
- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)browserAgent
           showContentAnimated:(BOOL)animated;

// Hides the reader mode content.
- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)browserAgent
           hideContentAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_DELEGATE_H_
