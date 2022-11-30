// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

#include "build/build_config.h"

namespace blink {

RendererPreferences::RendererPreferences() = default;

RendererPreferences::RendererPreferences(const RendererPreferences& other) =
    default;

RendererPreferences::RendererPreferences(RendererPreferences&& other) = default;

RendererPreferences::~RendererPreferences() = default;

RendererPreferences& RendererPreferences::operator=(
    const RendererPreferences& other) = default;

RendererPreferences& RendererPreferences::operator=(
    RendererPreferences&& other) = default;

}  // namespace blink
