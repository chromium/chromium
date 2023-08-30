// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SYNC_ERRORS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SYNC_ERRORS_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Possible error codes that can result from |CWVSyncControllerDataSource|'s
// access token fetches.
typedef NS_ENUM(NSInteger, CWVSyncError) {
  // The credentials supplied to GAIA were either invalid, or the locally
  // cached credentials have expired.
  CWVSyncErrorInvalidGAIACredentials = -100,
  // The GAIA user is not authorized to use the service.
  CWVSyncErrorUserNotSignedUp = -200,
  // Could not connect to server to verify credentials. This could be in
  // response to either failure to connect to GAIA or failure to connect to
  // the service needing GAIA tokens during authentication.
  CWVSyncErrorConnectionFailed = -300,
  // The service is not available; try again later.
  CWVSyncErrorServiceUnavailable = -400,
  // The requestor of the authentication step cancelled the request
  // prior to completion.
  CWVSyncErrorRequestCanceled = -500,
  // Indicates the service responded to a request, but we cannot
  // interpret the response.
  CWVSyncErrorUnexpectedServiceResponse = -600,
};

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SYNC_ERRORS_H_
