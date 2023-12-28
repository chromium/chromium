// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/fake_driver.h"

namespace media::internal {

FakeDriver::FakeDriver(int drm_fd) : scoped_bo_mapping_factory_(drm_fd) {}

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
  return surface_.CreateObject(format, width, height, std::move(attrib_list),
                               scoped_bo_mapping_factory_);
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
  return context_.CreateObject(GetConfig(config_id), picture_width,
                               picture_height, flag, std::move(render_targets));
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

FakeBuffer::IdType FakeDriver::CreateBuffer(VAContextID context,
                                            VABufferType type,
                                            unsigned int size_per_element,
                                            unsigned int num_elements,
                                            const void* data) {
  return buffers_.CreateObject(context, type, size_per_element, num_elements,
                               data);
}

bool FakeDriver::BufferExists(FakeBuffer::IdType id) {
  return buffers_.ObjectExists(id);
}

const FakeBuffer& FakeDriver::GetBuffer(FakeBuffer::IdType id) {
  return buffers_.GetObject(id);
}

void FakeDriver::DestroyBuffer(FakeBuffer::IdType id) {
  buffers_.DestroyObject(id);
}

void FakeDriver::CreateImage(const VAImageFormat& format,
                             int width,
                             int height,
                             VAImage* va_image) {
  images_.CreateObject(format, width, height, /*fake_driver=*/*this, va_image);
}

bool FakeDriver::ImageExists(FakeImage::IdType id) {
  return images_.ObjectExists(id);
}

const FakeImage& FakeDriver::GetImage(FakeImage::IdType id) {
  return images_.GetObject(id);
}

void FakeDriver::DestroyImage(FakeImage::IdType id) {
  images_.DestroyObject(id);
}

}  // namespace media::internal
