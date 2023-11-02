// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tts_controller_delegate.h"

namespace content {

TtsControllerDelegate::PreferredVoiceId::PreferredVoiceId(
    const std::string& name,
    const std::string& id)
    : name(name), id(id) {}

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
