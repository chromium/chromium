// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_D3D11_TEXTURE_HELPER_H_
#define DEVICE_VR_WINDOWS_D3D11_TEXTURE_HELPER_H_

#include <D3D11_1.h>
#include <DXGI1_4.h>
#include <wrl.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/win/scoped_handle.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {
class ContextProvider;
}
namespace device {

class D3D11TextureHelper {
 public:
  D3D11TextureHelper();
  ~D3D11TextureHelper();

  bool EnsureInitialized();
  bool SetAdapterLUID(const LUID& luid);
  void SetUseBGRA(bool bgra) { bgra_ = bgra; }

  void CleanupNoSubmit();
  void SetSourceAndOverlayVisible(bool source_visible, bool overlay_visible);

  bool CompositeToBackBuffer(
      const scoped_refptr<viz::ContextProvider>& context_provider);
  void SetSourceTexture(base::win::ScopedHandle texture_handle,
                        const gpu::SyncToken& sync_token,
                        gfx::RectF left,
                        gfx::RectF right);
  bool SetOverlayTexture(base::win::ScopedHandle texture_handle,
                         const gpu::SyncToken& sync_token,
                         gfx::RectF left,
                         gfx::RectF right);

  bool UpdateBackbufferSizes();
  void OverrideViewports(gfx::RectF left, gfx::RectF right) {
    target_left_ = left;
    target_right_ = right;
    force_viewport_ = true;
  }
  gfx::Size BackBufferSize() { return target_size_; }
  void SetBackbuffer(Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer);
  Microsoft::WRL::ComPtr<ID3D11Device> GetDevice();

  void SetDefaultSize(gfx::Size size) { default_size_ = size; }

 private:
  struct LayerData {
    LayerData();
    ~LayerData();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> source_texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
    gpu::SyncToken sync_token_;
    gfx::RectF left_;   // 0 to 1 in each direction
    gfx::RectF right_;  // 0 to 1 in each direction
    bool submitted_this_frame_ = false;
  };

  bool EnsureOverlayBlendState();
  bool EnsureContentBlendState();
  bool EnsureRenderTargetView();
  bool EnsureShaders();
  bool EnsureInputLayout();
  bool EnsureVertexBuffer();
  bool UpdateVertexBuffer(LayerData& layer);
  bool EnsureSampler(LayerData& layer);
  Microsoft::WRL::ComPtr<IDXGIAdapter> GetAdapter();
  bool CompositeLayer(LayerData& layer);
  bool BindTarget();
  void CleanupLayerData(LayerData& layer);

  struct RenderState {
    RenderState();
    ~RenderState();

    Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context_;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> flip_pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometry_shader_;

    Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> target_texture_;

    Microsoft::WRL::ComPtr<ID3D11BlendState> content_blend_state_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> overlay_blend_state_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> current_blend_state_;

    LayerData source_;
    LayerData overlay_;
  };

  bool overlay_visible_ = true;
  bool source_visible_ = true;

  bool bgra_ = false;
  bool force_viewport_ = false;

  gfx::RectF target_left_;   // 0 to 1 in each direction
  gfx::RectF target_right_;  // 0 to 1 in each direction
  gfx::Size target_size_;

  gfx::Size default_size_;

  RenderState render_state_;
  LUID adapter_luid_ = {};
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_D3D11_TEXTURE_HELPER_H_
