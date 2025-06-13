// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tts_controller_delegate.h"

#include <string>
#include <utility>

namespace content {

TtsControllerDelegate::PreferredVoiceId::PreferredVoiceId(std::string name,
                                                          std::string id)
    : name(std::move(name)), id(std::move(id)) {}

TtsControllerDelegate::PreferredVoiceId::PreferredVoiceId() = default;

TtsControllerDelegate::PreferredVoiceId::~PreferredVoiceId() = default;

TtsControllerDelegate::PreferredVoiceIds::PreferredVoiceIds() = default;

TtsControllerDelegate::PreferredVoiceIds::PreferredVoiceIds(
    const PreferredVoiceIds&) = default;

TtsControllerDelegate::PreferredVoiceIds&
TtsControllerDelegate::PreferredVoiceIds::operator=(const PreferredVoiceIds&) =
    default;

TtsControllerDelegate::PreferredVoiceIds::~PreferredVoiceIds() = default;

}  // namespace content
