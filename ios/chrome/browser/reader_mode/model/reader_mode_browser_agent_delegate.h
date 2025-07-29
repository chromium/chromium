// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_DELEGATE_H_

class ReaderModeBrowserAgent;

// Delegate for ReaderModeBrowserAgent.
@protocol ReaderModeBrowserAgentDelegate

// Shows the reader mode content.
- (void)showReaderModeContentFromBrowserAgent:
    (ReaderModeBrowserAgent*)browserAgent;

// Hides the reader mode content.
- (void)hideReaderModeContentFromBrowserAgent:
    (ReaderModeBrowserAgent*)browserAgent;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_DELEGATE_H_
