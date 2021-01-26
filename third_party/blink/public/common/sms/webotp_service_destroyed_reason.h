// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SMS_WEBOTP_SERVICE_DESTROYED_REASON_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SMS_WEBOTP_SERVICE_DESTROYED_REASON_H_

namespace blink {

// This enum describes the reason for destruction of the WebOTPService.
enum class WebOTPServiceDestroyedReason {
  // Don't change the meaning of these values because they are being recorded
  // in a metric.
  kNavigateNewPage = 0,
  kNavigateExistingPage = 1,
  // As of M90, kNavigateSamePage is no longer reported, and such "load the same
  // URL" cases are reported in the kNavigateExistingPage group instead.
  kNavigateSamePage_DEPRECATED = 2,
  kMaxValue = kNavigateSamePage_DEPRECATED
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SMS_WEBOTP_SERVICE_DESTROYED_REASON_H_
