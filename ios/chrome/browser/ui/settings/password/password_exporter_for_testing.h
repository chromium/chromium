// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_EXPORTER_FOR_TESTING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_EXPORTER_FOR_TESTING_H_

#import "ios/chrome/browser/ui/settings/password/password_exporter.h"

@interface PasswordExporter (ForTesting)

- (void)setPasswordSerializerBridge:
    (id<PasswordSerializerBridge>)passwordSerialzerBridge;

- (void)setPasswordFileWriter:(id<FileWriterProtocol>)passwordFileWriter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_EXPORTER_FOR_TESTING_H_
