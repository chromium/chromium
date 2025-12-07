// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_VCN_ENROLLMENT_MANAGER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_VCN_ENROLLMENT_MANAGER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCard;

// Manages enrolling a credit card to have a Virtual Card Number (VCN).
// To make a decision, there are 2 options:
// 1. Call |enrollWithCompletionHandler| to accept the enrollment offer.
// 2. Call |decline| to decline the enrollment offer.

CWV_EXPORT
@interface CWVVCNEnrollmentManager : NSObject

// The card that can be enrolled.
@property(nonatomic, readonly) CWVCreditCard* creditCard;

// If not empty, contains legal messaging that must be displayed to the user.
// Contains |NSLinkAttributeName| to indicate links wherever applicable.
@property(nonatomic, readonly) NSArray<NSAttributedString*>* legalMessages;

- (instancetype)init NS_UNAVAILABLE;

// Enrolls |creditCard| to have a Virtual Card Number.
// |completionHandler| will be called upon completion. |enrolled| represents the
// status of the enrollment process.
// This method should only be called once, and is mutually exclusive with
// |decline|.
- (void)enrollWithCompletionHandler:(void (^)(BOOL enrolled))completionHandler;

// Rejects enrolling the |creditCard|.
// This method should only be called once, and is mutually exclusive with
// |enrollWithCompletionHandler|.
- (void)decline;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_VCN_ENROLLMENT_MANAGER_H_
