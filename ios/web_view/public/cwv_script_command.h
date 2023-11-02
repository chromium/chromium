// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SCRIPT_COMMAND_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SCRIPT_COMMAND_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVWebView;

// A script command passed to CWVScriptCommandHandler.
CWV_EXPORT
@interface CWVScriptCommand : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Content of the command. nil in case of an error converting the content
// object.
@property(nonatomic, readonly, nullable, copy) NSDictionary* content;

// URL of the document in the main web view frame.
@property(nonatomic, readonly, copy) NSURL* mainDocumentURL;

// YES if the user is currently interacting with the page.
@property(nonatomic, readonly, getter=isUserInteracting) BOOL userInteracting;

@end

// Provides a method for receiving commands from JavaScript running in a web
// page.
@protocol CWVScriptCommandHandler<NSObject>

- (BOOL)webView:(CWVWebView*)webView
    handleScriptCommand:(CWVScriptCommand*)command
          fromMainFrame:(BOOL)fromMainFrame;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SCRIPT_COMMAND_H_
