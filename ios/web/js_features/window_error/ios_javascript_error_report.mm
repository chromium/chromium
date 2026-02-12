// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/ios_javascript_error_report.h"

IOSJavaScriptErrorReport::IOSJavaScriptErrorReport() = default;
IOSJavaScriptErrorReport::IOSJavaScriptErrorReport(
    const IOSJavaScriptErrorReport& rhs) = default;
IOSJavaScriptErrorReport::IOSJavaScriptErrorReport(
    IOSJavaScriptErrorReport&& rhs) noexcept = default;
IOSJavaScriptErrorReport::~IOSJavaScriptErrorReport() = default;
IOSJavaScriptErrorReport& IOSJavaScriptErrorReport::operator=(
    const IOSJavaScriptErrorReport& rhs) = default;
IOSJavaScriptErrorReport& IOSJavaScriptErrorReport::operator=(
    IOSJavaScriptErrorReport&& rhs) noexcept = default;
