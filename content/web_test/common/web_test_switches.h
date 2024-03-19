// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_COMMON_WEB_TEST_SWITCHES_H_
#define CONTENT_WEB_TEST_COMMON_WEB_TEST_SWITCHES_H_

namespace switches {

extern const char kAllowExternalPages[];
extern const char kCrashOnFailure[];
extern const char kDebugDevTools[];
extern const char kEnableAccelerated2DCanvas[];
extern const char kEnableFontAntialiasing[];
extern const char kAlwaysUseComplexText[];
extern const char kEnableLeakDetection[];
extern const char kInspectorProtocolLog[];
extern const char kEncodeBinary[];
extern const char kStableReleaseMode[];
extern const char kDisableHeadlessMode[];
extern const char kDisableAutoWPTOriginIsolation[];
extern const char kResetBrowsingInstanceBetweenTests[];

}  // namespace switches

#endif  // CONTENT_WEB_TEST_COMMON_WEB_TEST_SWITCHES_H_
