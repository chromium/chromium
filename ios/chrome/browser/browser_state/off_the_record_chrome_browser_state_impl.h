// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_OFF_THE_RECORD_CHROME_BROWSER_STATE_IMPL_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_OFF_THE_RECORD_CHROME_BROWSER_STATE_IMPL_H_

#include "base/macros.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/off_the_record_chrome_browser_state_io_data.h"

namespace sync_preferences {
class PrefServiceSyncable;
}

// The implementation of ChromeBrowserState that is used for incognito browsing.
// Each OffTheRecordChromeBrowserStateImpl instance is associated with and owned
// by a non-incognito ChromeBrowserState instance.
class OffTheRecordChromeBrowserStateImpl : public ios::ChromeBrowserState {
 public:
  ~OffTheRecordChromeBrowserStateImpl() override;

  // ChromeBrowserState:
  ios::ChromeBrowserState* GetOriginalChromeBrowserState() override;
  bool HasOffTheRecordChromeBrowserState() const override;
  ios::ChromeBrowserState* GetOffTheRecordChromeBrowserState() override;
  void DestroyOffTheRecordChromeBrowserState() override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  PrefService* GetPrefs() override;
  PrefService* GetOffTheRecordPrefs() override;
  ChromeBrowserStateIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   const base::Closure& completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;

  // BrowserState:
  bool IsOffTheRecord() const override;
  base::FilePath GetStatePath() const override;

 private:
  friend class ChromeBrowserStateImpl;

  // |original_chrome_browser_state_| is the non-incognito
  // ChromeBrowserState instance that owns this instance.
  OffTheRecordChromeBrowserStateImpl(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      ios::ChromeBrowserState* original_chrome_browser_state,
      const base::FilePath& otr_path);

  base::FilePath otr_state_path_;
  ios::ChromeBrowserState* original_chrome_browser_state_;  // weak

  // Weak pointer owned by |original_chrome_browser_state_|.
  sync_preferences::PrefServiceSyncable* prefs_;

  std::unique_ptr<OffTheRecordChromeBrowserStateIOData::Handle> io_data_;
  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordChromeBrowserStateImpl);
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_OFF_THE_RECORD_CHROME_BROWSER_STATE_IMPL_H_
