// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_WEB_STATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_WEB_STATE_DELEGATE_H_

#import <Foundation/Foundation.h>

class ReaderModeBrowserAgent;

namespace web {
class WebState;
}  // namespace web

// Delegate for ReaderModeBrowserAgent for web state lifecycle.
@protocol ReaderModeBrowserAgentWebStateDelegate

- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)browserAgent
       didCreateReaderWebState:(web::WebState*)webState;

- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)browserAgent
     willDestroyReaderWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_WEB_STATE_DELEGATE_H_
