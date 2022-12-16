// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/fake_config.h"

namespace media::internal {

FakeConfig::FakeConfig(FakeConfig::IdType id,
                       VAProfile profile,
                       VAEntrypoint entrypoint,
                       std::vector<VAConfigAttrib> attrib_list)
    : id_(id),
      profile_(profile),
      entrypoint_(entrypoint),
      attrib_list_(std::move(attrib_list)) {}
FakeConfig::~FakeConfig() = default;

FakeConfig::IdType FakeConfig::GetID() const {
  return id_;
}

VAProfile FakeConfig::GetProfile() const {
  return profile_;
}

VAEntrypoint FakeConfig::GetEntrypoint() const {
  return entrypoint_;
}

const std::vector<VAConfigAttrib>& FakeConfig::GetConfigAttribs() const {
  return attrib_list_;
}

}  // namespace media::internal