// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DEPENDENCY_BRIDGE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DEPENDENCY_BRIDGE_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

class Browser;

// A bridge class to install dependencies on the reader mode WebState.
class ReaderModeDependencyBridge {
 public:
  ReaderModeDependencyBridge(Browser* browser);
  ~ReaderModeDependencyBridge();

  // ReaderModeTabHelper::Observer implementation:
  void ReaderModeWebStateDidLoadContent(web::WebState* web_state);
  void ReaderModeWebStateWillBecomeUnavailable(web::WebState* web_state);
  void ReaderModeTabHelperDestroyed(web::WebState* web_state);

 private:
  raw_ptr<Browser> browser_;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DEPENDENCY_BRIDGE_H_
