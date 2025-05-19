// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_DELEGATE_H_

class ReaderModeTabHelper;

// Delegate for ReaderModeTabHelper.
class ReaderModeTabHelperDelegate {
 public:
  // Called when Reader mode content became available in this tab.
  virtual void ReaderModeContentDidBecomeAvailable(
      ReaderModeTabHelper* tab_helper) = 0;
  // Called when Reader mode content will become unavailable in this tab.
  virtual void ReaderModeContentWillBecomeUnavailable(
      ReaderModeTabHelper* tab_helper) = 0;

 protected:
  virtual ~ReaderModeTabHelperDelegate() = default;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_DELEGATE_H_
