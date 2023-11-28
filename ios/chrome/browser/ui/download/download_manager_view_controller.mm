// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller_delegate.h"
#import "ios/chrome/browser/ui/download/features.h"

// TODO(crbug.com/1495347): Implement the DownloadManagerViewController.
@interface DownloadManagerViewController () {
  NSString* _fileName;
  int64_t _countOfBytesReceived;
  int64_t _countOfBytesExpectedToReceive;
  float _progress;
  DownloadManagerState _state;
}

@end

@implementation DownloadManagerViewController

#pragma mark - DownloadManagerConsumer

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.overrideUserInterfaceStyle =
      incognito && base::FeatureList::IsEnabled(kIOSIncognitoDownloadsWarning)
          ? UIUserInterfaceStyleDark
          : UIUserInterfaceStyleUnspecified;
}

- (void)setFileName:(NSString*)fileName {
  if (![_fileName isEqualToString:fileName]) {
    _fileName = [fileName copy];
  }
}

- (void)setCountOfBytesReceived:(int64_t)value {
  if (_countOfBytesReceived != value) {
    _countOfBytesReceived = value;
  }
}

- (void)setCountOfBytesExpectedToReceive:(int64_t)value {
  if (_countOfBytesExpectedToReceive != value) {
    _countOfBytesExpectedToReceive = value;
  }
}

- (void)setProgress:(float)value {
  if (_progress != value) {
    _progress = value;
  }
}

- (void)setState:(DownloadManagerState)state {
  if (_state != state) {
    _state = state;
  }
}

#pragma mark - DownloadManagerViewControllerProtocol

- (UIView*)openInSourceView {
  return nil;
}

@end
