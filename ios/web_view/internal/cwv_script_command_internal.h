// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_SCRIPT_COMMAND_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_SCRIPT_COMMAND_INTERNAL_H_

#import "ios/web_view/public/cwv_script_command.h"

NS_ASSUME_NONNULL_BEGIN

@interface CWVScriptCommand ()

/**
 * Designated initializer.
 *
 * @param content Content of the command. nil in case of an error converting the
 *     content object.
 * @param mainDocumentURL URL of the document in the main web view frame.
 * @param userInteracting YES if the user is currently interacting with the
 *     page.
 */
- (instancetype)initWithContent:(nullable NSDictionary*)content
                mainDocumentURL:(NSURL*)mainDocumentURL
                userInteracting:(BOOL)userInteracting NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_SCRIPT_COMMAND_INTERNAL_H_
