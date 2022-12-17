// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/fake_driver.h"

namespace media::internal {

FakeDriver::FakeDriver() = default;
FakeDriver::~FakeDriver() = default;

FakeConfig::IdType FakeDriver::CreateConfig(
    VAProfile profile,
    VAEntrypoint entrypoint,
    std::vector<VAConfigAttrib> attrib_list) {
  return config_.CreateObject(profile, entrypoint, std::move(attrib_list));
}

bool FakeDriver::ConfigExists(FakeConfig::IdType id) {
  return config_.ObjectExists(id);
}

const FakeConfig& FakeDriver::GetConfig(FakeConfig::IdType id) {
  return config_.GetObject(id);
}

void FakeDriver::DestroyConfig(FakeConfig::IdType id) {
  config_.DestroyObject(id);
}

FakeSurface::IdType FakeDriver::CreateSurface(
    unsigned int format,
    unsigned int width,
    unsigned int height,
    std::vector<VASurfaceAttrib> attrib_list) {
  return surface_.CreateObject(format, width, height, std::move(attrib_list));
}

bool FakeDriver::SurfaceExists(FakeSurface::IdType id) {
  return surface_.ObjectExists(id);
}

const FakeSurface& FakeDriver::GetSurface(FakeSurface::IdType id) {
  return surface_.GetObject(id);
}

void FakeDriver::DestroySurface(FakeSurface::IdType id) {
  surface_.DestroyObject(id);
}

FakeContext::IdType FakeDriver::CreateContext(
    VAConfigID config_id,
    int picture_width,
    int picture_height,
    int flag,
    std::vector<VASurfaceID> render_targets) {
  return context_.CreateObject(config_id, picture_width, picture_height, flag,
                               std::move(render_targets));
}

bool FakeDriver::ContextExists(FakeContext::IdType id) {
  return context_.ObjectExists(id);
}

const FakeContext& FakeDriver::GetContext(FakeContext::IdType id) {
  return context_.GetObject(id);
}

void FakeDriver::DestroyContext(FakeContext::IdType id) {
  context_.DestroyObject(id);
}

}  // namespace media::internal
