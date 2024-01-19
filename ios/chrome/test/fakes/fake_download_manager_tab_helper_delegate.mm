// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_download_manager_tab_helper_delegate.h"

using DecidePolicyForDownloadHandler = void (^)(NewDownloadPolicy);

@implementation FakeDownloadManagerTabHelperDelegate {
  std::unique_ptr<web::DownloadTask::State> _state;
  web::DownloadTask* _decidingPolicyForDownload;
  DecidePolicyForDownloadHandler _decidePolicyForDownloadHandler;
}

- (web::DownloadTask::State*)state {
  return _state.get();
}

- (web::DownloadTask*)decidingPolicyForDownload {
  return _decidingPolicyForDownload;
}

- (BOOL)decidePolicy:(NewDownloadPolicy)policy {
  if (!_decidePolicyForDownloadHandler)
    return NO;

  _decidePolicyForDownloadHandler(policy);
  _decidingPolicyForDownload = nil;
  _decidePolicyForDownloadHandler = nil;
  return YES;
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(web::DownloadTask*)download
               webStateIsVisible:(BOOL)webStateIsVisible {
  if (webStateIsVisible) {
    _state = std::make_unique<web::DownloadTask::State>(download->GetState());
  }
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
         decidePolicyForDownload:(web::DownloadTask*)download
               completionHandler:(void (^)(NewDownloadPolicy))handler {
  _decidingPolicyForDownload = download;
  _decidePolicyForDownloadHandler = handler;
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
                 didHideDownload:(web::DownloadTask*)download {
  _state = nullptr;
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
                 didShowDownload:(web::DownloadTask*)download {
  _state = std::make_unique<web::DownloadTask::State>(download->GetState());
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
            wantsToStartDownload:(web::DownloadTask*)download {
}

@end
