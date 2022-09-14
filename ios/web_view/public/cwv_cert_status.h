// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CERT_STATUS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CERT_STATUS_H_

#import <Foundation/Foundation.h>

// Bit mask for the status of a SSL certificate.
//
// Implementation comment: This enum mirrors error statuses (not including
// non-error statuses) in //net/cert/cert_status_flags_list.h.
typedef NS_OPTIONS(NSInteger, CWVCertStatus) {
  CWVCertStatusCommonNameInvalid = 1 << 0,
  CWVCertStatusDateInvalid = 1 << 1,
  CWVCertStatusAuthorityInvalid = 1 << 2,
  CWVCertStatusNoRevocationMechanism = 1 << 4,
  CWVCertStatusUnableToCheckRevocation = 1 << 5,
  CWVCertStatusRevoked = 1 << 6,
  CWVCertStatusInvalid = 1 << 7,
  CWVCertStatusWeakSignatureAlgorithm = 1 << 8,
  CWVCertStatusNonUniqueName = 1 << 10,
  CWVCertStatusWeakKey = 1 << 11,
  CWVCertStatusPinnedKeyMissing = 1 << 13,
  CWVCertStatusNameConstraintViolation = 1 << 14,
  CWVCertStatusValidityTooLong = 1 << 15,
  CWVCertStatusCertificateTransparencyRequired = 1 << 24,
  CWVCertStatusSymantecLegacy = 1 << 25,
  CWVCertStatusKnownInterceptionBlocked = 1 << 26,
};

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CERT_STATUS_H_
