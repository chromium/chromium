// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dxgi1_2.h>

#include "media/base/win/mf_helpers.h"
#include "media/renderers/win/media_foundation_texture_pool.h"

namespace {

using Microsoft::WRL::ComPtr;

// The Texture Count was determined empirically initially having a count of 30
// and running many different video presentations in frame server mode and
// recording the number of textures in use and the count never exceeded 3.
// Therefore for a max of 3 in flight with the 3 being written requires that
// we allocate 4 textures.
constexpr int kTexturePoolCount = 4;

}  // namespace

namespace media {

MediaFoundationFrameInfo::MediaFoundationFrameInfo() = default;
MediaFoundationFrameInfo::~MediaFoundationFrameInfo() = default;
MediaFoundationFrameInfo::MediaFoundationFrameInfo(
    MediaFoundationFrameInfo&& other) = default;

MediaFoundationTexturePool::TextureInfo::TextureInfo()
    : texture_in_use_(false) {}
MediaFoundationTexturePool::TextureInfo::~TextureInfo() = default;
MediaFoundationTexturePool::TextureInfo::TextureInfo(const TextureInfo& other) =
    default;
MediaFoundationTexturePool::TextureInfo&
MediaFoundationTexturePool::TextureInfo::operator=(
    const MediaFoundationTexturePool::TextureInfo& other) = default;

MediaFoundationTexturePool::MediaFoundationTexturePool() = default;
MediaFoundationTexturePool::~MediaFoundationTexturePool() = default;

// TODO(crbug.com/40810044): The pool should release the textures when the media
// engine is idling to save resources.
HRESULT MediaFoundationTexturePool::Initialize(
    ID3D11Device* device,
    FramePoolInitializedCallback frame_pool_cb,
    const gfx::Size& frame_size) {
  D3D11_TEXTURE2D_DESC desc{
      static_cast<UINT>(frame_size.width()),
      static_cast<UINT>(frame_size.height()),
      1,
      1,
      // TODO(crbug.com/40808700): Need to handle higher bit-depths like HDR.
      DXGI_FORMAT_B8G8R8A8_UNORM,
      {1, 0},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
      0,
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
          D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

  std::vector<MediaFoundationFrameInfo> frame_infos;
  bool callback_is_valid = !frame_pool_cb.is_null();
  if (callback_is_valid) {
    frame_infos.reserve(kTexturePoolCount);
  }

  // We can be reinitialized so remove all the previous textures from our
  // pool.
  texture_pool_.clear();

  for (int i = 0; i < kTexturePoolCount; ++i) {
    auto texture_info_element = std::make_unique<TextureInfo>();
    auto texture_token = base::UnguessableToken::Create();

    ComPtr<ID3D11Texture2D> d3d11_video_frame;
    RETURN_IF_FAILED(
        device->CreateTexture2D(&desc, nullptr, &d3d11_video_frame));
    SetDebugName(d3d11_video_frame.Get(), "Media_MFFrameServerMode_Pool");

    ComPtr<IDXGIResource1> d3d11_video_frame_resource;
    RETURN_IF_FAILED(d3d11_video_frame.As(&d3d11_video_frame_resource));

    HANDLE shared_texture_handle;
    RETURN_IF_FAILED(d3d11_video_frame_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &shared_texture_handle));

    base::win::ScopedHandle scoped_shared_texture_handle;
    scoped_shared_texture_handle.Set(shared_texture_handle);
    shared_texture_handle = nullptr;
    texture_pool_[texture_token].texture_ = std::move(d3d11_video_frame);
    texture_pool_[texture_token].texture_in_use_ = false;

    if (callback_is_valid) {
      MediaFoundationFrameInfo frame_info;
      frame_info.dxgi_handle = std::move(scoped_shared_texture_handle);
      frame_info.token = texture_token;
      frame_infos.emplace_back(std::move(frame_info));
    }
  }

  if (callback_is_valid) {
    frame_pool_cb.Run(std::move(frame_infos), frame_size);
  }

  return S_OK;
}

ComPtr<ID3D11Texture2D> MediaFoundationTexturePool::AcquireTexture(
    base::UnguessableToken* texture_token) {
  for (auto& texture_item : texture_pool_) {
    if (!texture_item.second.texture_in_use_) {
      *texture_token = texture_item.first;
      texture_item.second.texture_in_use_ = true;
      return texture_item.second.texture_;
    }
  }

  return nullptr;
}

void MediaFoundationTexturePool::ReleaseTexture(
    const base::UnguessableToken& texture_token) {
  auto it = texture_pool_.find(texture_token);
  if (it != texture_pool_.end()) {
    it->second.texture_in_use_ = false;
  }
}

}  // namespace media