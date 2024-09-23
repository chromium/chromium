// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_TEXTURE_POOL_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_TEXTURE_POOL_H_

#include <d3d11.h>
#include <wrl/client.h>
#include <map>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct MEDIA_EXPORT MediaFoundationFrameInfo {
  MediaFoundationFrameInfo();
  ~MediaFoundationFrameInfo();
  MediaFoundationFrameInfo(MediaFoundationFrameInfo&& other);
  base::win::ScopedHandle dxgi_handle;
  base::UnguessableToken token;
};

using FramePoolInitializedCallback = base::RepeatingCallback<void(
    std::vector<MediaFoundationFrameInfo> frame_textures,
    const gfx::Size& texture_size)>;

// This object will create a pool D3D11Texture2Ds that the video frames in the
// Media Foundation Media Engine will draw to. By having this pool we don't
// need to create a D3D11Texture2D and associated Shared Image mailbox for
// each video frame. To coordinate the Shared Images in the
// MediaFoundationRendererClient each texture has a |texture_token| to signal
// which texture is ready to be displayed and which texture is ready for
// reuse.
class MEDIA_EXPORT MediaFoundationTexturePool {
 public:
  MediaFoundationTexturePool();
  ~MediaFoundationTexturePool();
  MediaFoundationTexturePool(const MediaFoundationTexturePool& other) = delete;
  MediaFoundationTexturePool& operator=(
      const MediaFoundationTexturePool& other) = delete;

  // Initializes the texture pool with a specific size. Once the textures are
  // created the callback will be called passing the information about the
  // textures. The method can be called multiple times, which will release the
  // previously allocated textures and create a new set of textures on the
  // device with the frame size.
  // Any event that changes the frame size will be calling this method to
  // change the texture size. The callback will eventually call into the Media
  // Foundation Renderer which will create the Shared Images with the DX shared
  // handle. Examples of callers are CreateMediaEngine and
  // OnVideoNaturalSizeChange in the MediaFoundationRenderer
  HRESULT Initialize(ID3D11Device* device,
                     FramePoolInitializedCallback frame_pool_cb,
                     const gfx::Size& frame_size);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> AcquireTexture(
      base::UnguessableToken* texture_token);
  void ReleaseTexture(const base::UnguessableToken& texture_token);

 private:
  struct TextureInfo {
    TextureInfo();
    ~TextureInfo();
    TextureInfo(const TextureInfo& other);
    TextureInfo& operator=(const TextureInfo& other);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    bool texture_in_use_;
  };

  base::flat_map<base::UnguessableToken, TextureInfo> texture_pool_;
};
}  // namespace media
#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_TEXTURE_POOL_H_
