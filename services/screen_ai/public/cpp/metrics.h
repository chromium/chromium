// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_PUBLIC_CPP_METRICS_H_
#define SERVICES_SCREEN_AI_PUBLIC_CPP_METRICS_H_

#include <string>

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace screen_ai {

void RecordMostDetectedLanguageInOcrData(
    const std::string& metric_name,
    const ui::AXTreeUpdate& tree_update_with_ocr_data);

}  // namespace screen_ai

#endif  // SERVICES_SCREEN_AI_PUBLIC_CPP_METRICS_H_
