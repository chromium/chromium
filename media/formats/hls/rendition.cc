// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition.h"

#include <optional>

#include "base/types/pass_key.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/quirks.h"
#include "media/formats/hls/tags.h"

namespace media::hls {

Rendition::Rendition(base::PassKey<RenditionGroup>, CtorArgs args)
    : Rendition(std::move(args)) {}

Rendition::Rendition(CtorArgs args)
    : uri_(std::move(args.uri)),
      name_(std::move(args.name)),
      language_(std::move(args.language)),
      stable_rendition_id_(std::move(args.stable_rendition_id)),
      channels_(std::move(args.channels)),
      autoselect_(std::move(args.autoselect)) {}

Rendition::Rendition(Rendition&&) = default;

Rendition::~Rendition() = default;

// static
Rendition Rendition::CreateRenditionForTesting(CtorArgs args) {
  return Rendition{std::move(args)};
}

}  // namespace media::hls
