// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_details.h"

ScriptErrorDetails::ScriptErrorDetails(bool main) : is_main_frame(main) {}
ScriptErrorDetails::~ScriptErrorDetails() = default;
ScriptErrorDetails::ScriptErrorDetails(const ScriptErrorDetails& other) =
    default;

ScriptErrorDetails::ScriptErrorDetails(ScriptErrorDetails&& other) = default;
ScriptErrorDetails& ScriptErrorDetails::operator=(ScriptErrorDetails&& other) =
    default;
