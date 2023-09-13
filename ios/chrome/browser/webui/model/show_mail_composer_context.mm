// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/model/show_mail_composer_context.h"

#import "base/check.h"
#import "base/files/file_path.h"

@implementation ShowMailComposerContext {
  base::FilePath _textFileToAttach;
}

@synthesize emailNotConfiguredAlertTitleId = _emailNotConfiguredAlertTitleId;
@synthesize emailNotConfiguredAlertMessageId =
    _emailNotConfiguredAlertMessageId;
@synthesize toRecipients = _toRecipients;
@synthesize subject = _subject;
@synthesize body = _body;

- (instancetype)initWithToRecipients:(NSArray<NSString*>*)toRecipients
                             subject:(NSString*)subject
                                body:(NSString*)body
      emailNotConfiguredAlertTitleId:(int)alertTitleId
    emailNotConfiguredAlertMessageId:(int)alertMessageId {
  DCHECK(alertTitleId);
  DCHECK(alertMessageId);
  self = [super init];
  if (self) {
    _toRecipients = [[NSArray alloc] initWithArray:toRecipients copyItems:YES];
    _subject = [subject copy];
    _body = [body copy];
    _emailNotConfiguredAlertTitleId = alertTitleId;
    _emailNotConfiguredAlertMessageId = alertMessageId;
  }
  return self;
}

- (const base::FilePath&)textFileToAttach {
  return _textFileToAttach;
}

- (void)setTextFileToAttach:(const base::FilePath&)textFileToAttach {
  _textFileToAttach = textFileToAttach;
}

@end
