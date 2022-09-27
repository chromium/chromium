// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_settings.h"

#include "third_party/blink/renderer/core/frame/settings.h"

namespace blink {

WorkerSettings::WorkerSettings(Settings* settings) {
  if (settings)
    CopyFlagValuesFromSettings(settings);
}

WorkerSettings::WorkerSettings(
    bool disable_reading_from_canvas,
    bool strict_mixed_content_checking,
    bool allow_running_of_insecure_content,
    bool strictly_block_blockable_mixed_content,
    const GenericFontFamilySettings& generic_font_family_settings)
    : disable_reading_from_canvas_(disable_reading_from_canvas),
      strict_mixed_content_checking_(strict_mixed_content_checking),
      allow_running_of_insecure_content_(allow_running_of_insecure_content),
      strictly_block_blockable_mixed_content_(
          strictly_block_blockable_mixed_content),
      generic_font_family_settings_(generic_font_family_settings) {}

std::unique_ptr<WorkerSettings> WorkerSettings::Copy(
    WorkerSettings* old_settings) {
  std::unique_ptr<WorkerSettings> new_settings =
      std::make_unique<WorkerSettings>(nullptr);
  new_settings->disable_reading_from_canvas_ =
      old_settings->disable_reading_from_canvas_;
  new_settings->strict_mixed_content_checking_ =
      old_settings->strict_mixed_content_checking_;
  new_settings->allow_running_of_insecure_content_ =
      old_settings->allow_running_of_insecure_content_;
  new_settings->strictly_block_blockable_mixed_content_ =
      old_settings->strictly_block_blockable_mixed_content_;
  new_settings->generic_font_family_settings_ =
      old_settings->generic_font_family_settings_;
  return new_settings;
}

void WorkerSettings::CopyFlagValuesFromSettings(Settings* settings) {
  disable_reading_from_canvas_ = settings->GetDisableReadingFromCanvas();
  strict_mixed_content_checking_ = settings->GetStrictMixedContentChecking();
  allow_running_of_insecure_content_ =
      settings->GetAllowRunningOfInsecureContent();
  strictly_block_blockable_mixed_content_ =
      settings->GetStrictlyBlockBlockableMixedContent();
  generic_font_family_settings_ = settings->GetGenericFontFamilySettings();
}

}  // namespace blink
