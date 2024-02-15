// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows/d3d11_texture_helper.h"

#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/constants.h"
#include "mojo/public/c/system/platform_handle.h"

namespace {
// These headers declare global variables representing the contained shaders, so
// they are defined in the anonymous namespace to ensure that they don't leak.
#include "device/vr/windows/flip_pixel_shader.h"
#include "device/vr/windows/geometry_shader.h"
#include "device/vr/windows/vertex_shader.h"

struct Vertex2D {
  float x;
  float y;
  float u;
  float v;

  // Which texture in a texture array to output this triangle to?  If we only
  // have a single texture bound as the render target, this is ignored.
  int target;
};

constexpr size_t kSizeOfVertex = sizeof(Vertex2D);

// 2 triangles per eye
constexpr size_t kNumVerticesPerLayer = 12;

// This enum is used in TRACE_EVENTs.  Try to keep enum values the same to make
// analysis easier across builds.
enum ErrorLocation {
  OverlayBlendState = 1,
  ContentBlendState = 2,
  SourceTimeout = 3,
  OverlayTimeout = 4,
  BindTarget = 5,
  EnsureRenderTargetView = 6,
  EnsureRenderTargetView2 = 7,
  EnsureVS = 8,
  EnsureGS = 9,
  EnsurePS = 10,
  InputLayout = 11,
  VertexBuffer = 12,
  Sampler = 13,
  ShaderResource = 14,
  OpenSource = 15,
  OpenOverlay = 16,
  CreateDevice = 17,
};

void TraceDXError(ErrorLocation location, HRESULT hr) {
  TRACE_EVENT_INSTANT2("xr", "TraceDXError", TRACE_EVENT_SCOPE_THREAD,
                       "ErrorLocation", location, "hr", hr);
}

}  // namespace

namespace device {

D3D11TextureHelper::RenderState::RenderState() {}
D3D11TextureHelper::RenderState::~RenderState() {}

D3D11TextureHelper::LayerData::LayerData() = default;
D3D11TextureHelper::LayerData::~LayerData() = default;

D3D11TextureHelper::D3D11TextureHelper() = default;

D3D11TextureHelper::~D3D11TextureHelper() = default;

void D3D11TextureHelper::SetSourceAndOverlayVisible(bool source_visible,
                                                    bool overlay_visible) {
  source_visible_ = source_visible;
  overlay_visible_ = overlay_visible;
  TRACE_EVENT_INSTANT2("xr", "TextureHelper SetSourceAndOverlayVisible",
                       TRACE_EVENT_SCOPE_THREAD, "source", source_visible,
                       "overlay", overlay_visible);

  if (!source_visible_) {
    render_state_.source_.keyed_mutex_ = nullptr;
    render_state_.source_.source_texture_ = nullptr;
    render_state_.source_.shader_resource_ = nullptr;
    render_state_.source_.sampler_ = nullptr;
  }
  if (!overlay_visible_) {
    render_state_.overlay_.keyed_mutex_ = nullptr;
    render_state_.overlay_.source_texture_ = nullptr;
    render_state_.overlay_.shader_resource_ = nullptr;
    render_state_.overlay_.sampler_ = nullptr;
  }
}

void D3D11TextureHelper::CleanupNoSubmit() {
  render_state_.source_.keyed_mutex_ = nullptr;
  render_state_.source_.source_texture_ = nullptr;
  render_state_.source_.shader_resource_ = nullptr;
  render_state_.source_.sampler_ = nullptr;

  render_state_.overlay_.keyed_mutex_ = nullptr;
  render_state_.overlay_.source_texture_ = nullptr;
  render_state_.overlay_.shader_resource_ = nullptr;
  render_state_.overlay_.sampler_ = nullptr;
}

void D3D11TextureHelper::CleanupLayerData(LayerData& layer) {
  if (!layer.submitted_this_frame_) {
    layer.keyed_mutex_ = nullptr;
    layer.sampler_ = nullptr;
    layer.shader_resource_ = nullptr;
    layer.source_texture_ = nullptr;
  }

  // Set up for the next frame so we know if we submitted again.
  layer.submitted_this_frame_ = false;
}

bool D3D11TextureHelper::EnsureOverlayBlendState() {
  if (!render_state_.overlay_blend_state_) {
    D3D11_BLEND_DESC blenddesc = {};
    blenddesc.RenderTarget[0].BlendEnable = true;
    blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blenddesc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    HRESULT hr = render_state_.d3d11_device_->CreateBlendState(
        &blenddesc, &(render_state_.overlay_blend_state_));
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::OverlayBlendState, hr);
      return false;
    }
  }

  if (render_state_.overlay_blend_state_ !=
      render_state_.current_blend_state_) {
    render_state_.d3d11_device_context_->OMSetBlendState(
        render_state_.overlay_blend_state_.Get(), 0, -1);
    render_state_.current_blend_state_ = render_state_.overlay_blend_state_;
  }
  return true;
}

bool D3D11TextureHelper::EnsureContentBlendState() {
  if (!render_state_.content_blend_state_) {
    D3D11_BLEND_DESC blenddesc = {};
    blenddesc.RenderTarget[0].BlendEnable = false;
    blenddesc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    HRESULT hr = render_state_.d3d11_device_->CreateBlendState(
        &blenddesc, &(render_state_.content_blend_state_));
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::ContentBlendState, hr);
      return false;
    }
  }

  if (render_state_.content_blend_state_ !=
      render_state_.current_blend_state_) {
    render_state_.d3d11_device_context_->OMSetBlendState(
        render_state_.content_blend_state_.Get(), 0, -1);
    render_state_.current_blend_state_ = render_state_.content_blend_state_;
  }
  return true;
}

bool D3D11TextureHelper::CompositeToBackBuffer(
    const scoped_refptr<viz::ContextProvider>& context_provider) {
  if (!EnsureInitialized())
    return false;

  // Clear stale data:
  CleanupLayerData(render_state_.source_);
  CleanupLayerData(render_state_.overlay_);

  // We should always have a target texture that WebXR
  // is rendering into.
  if (!render_state_.target_texture_)
    return false;

  // Source texture is optional depending on whether we're using
  // shared images for the destination.
  if (!render_state_.source_.source_texture_ &&
      !render_state_.overlay_.source_texture_)
    return true;

  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  if (!gl) {
    return false;
  }

  HRESULT hr = S_OK;
  if (render_state_.source_.keyed_mutex_) {
    if (render_state_.source_.sync_token_.HasData()) {
      // Ensure work has been issused to write to source texture by blocking
      // until GPU process has passed the sync token. This must happen before
      // AcquireSync(0) below otherwise the GPU process will be unable to
      // acquire the mutex and work will happen out of order.
      gl->WaitSyncTokenCHROMIUM(
          render_state_.source_.sync_token_.GetConstData());
      gl->Finish();
      render_state_.source_.sync_token_.Clear();
    }

    hr = render_state_.source_.keyed_mutex_->AcquireSync(
        gpu::kDXGIKeyedMutexAcquireKey, INFINITE);
    if (FAILED(hr) || hr == WAIT_TIMEOUT || hr == WAIT_ABANDONED) {
      // We failed to acquire the lock.  We'll drop this frame, but subsequent
      // frames won't be affected.
      TraceDXError(ErrorLocation::SourceTimeout, hr);
      return false;
    }
  }

  if (render_state_.overlay_.keyed_mutex_) {
    if (render_state_.overlay_.sync_token_.HasData()) {
      // Ensure work has been issused to write to overlay texture by blocking
      // until GPU process has passed the sync token. This must happen before
      // AcquireSync(0) below otherwise the GPU process will be unable to
      // acquire the mutex and work will happen out of order.
      gl->WaitSyncTokenCHROMIUM(
          render_state_.overlay_.sync_token_.GetConstData());
      gl->Finish();
      render_state_.overlay_.sync_token_.Clear();
    }

    hr = render_state_.overlay_.keyed_mutex_->AcquireSync(
        gpu::kDXGIKeyedMutexAcquireKey, INFINITE);
    if (FAILED(hr) || hr == WAIT_TIMEOUT || hr == WAIT_ABANDONED) {
      // We failed to acquire the lock.  We'll drop this frame, but subsequent
      // frames won't be affected.
      TraceDXError(ErrorLocation::OverlayTimeout, hr);
      if (render_state_.source_.keyed_mutex_) {
        render_state_.source_.keyed_mutex_->ReleaseSync(0);
      }
      return false;
    }
  }

  if (!BindTarget()) {
    TraceDXError(ErrorLocation::BindTarget, hr);
    return false;
  }

  if (render_state_.overlay_.source_texture_ &&
      (!render_state_.source_.source_texture_ || !source_visible_)) {
    // If we have an overlay, but no WebXR texture under it, clear the target
    // first, since overlay may assume transparency.
    float color_rgba[4] = {0, 0, 0, 1};
    render_state_.d3d11_device_context_->ClearRenderTargetView(
        render_state_.render_target_view_.Get(), color_rgba);
  }

  bool success = true;
  if (render_state_.source_.source_texture_)
    success = success && EnsureContentBlendState() &&
              CompositeLayer(render_state_.source_);
  if (render_state_.overlay_.source_texture_)
    success = success && EnsureOverlayBlendState() &&
              CompositeLayer(render_state_.overlay_);

  if (render_state_.source_.keyed_mutex_)
    render_state_.source_.keyed_mutex_->ReleaseSync(0);
  if (render_state_.overlay_.keyed_mutex_)
    render_state_.overlay_.keyed_mutex_->ReleaseSync(0);

  return success;
}

bool D3D11TextureHelper::EnsureRenderTargetView() {
  if (!render_state_.render_target_view_) {
    D3D11_TEXTURE2D_DESC desc;
    render_state_.target_texture_->GetDesc(&desc);

    if (desc.ArraySize > 1) {
      HRESULT hr = render_state_.d3d11_device_->CreateRenderTargetView(
          render_state_.target_texture_.Get(), nullptr,
          &render_state_.render_target_view_);
      if (FAILED(hr)) {
        TraceDXError(ErrorLocation::EnsureRenderTargetView, hr);
      }
      return SUCCEEDED(hr);
    }

    D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
    // If the resource is unknown or typeless, use R8G8B8A8_UNORM, which is
    // required for Oculus.  Otherwise, use the resource's native type.
    render_target_view_desc.Format =
        (desc.Format == DXGI_FORMAT_UNKNOWN ||
         desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)
            ? DXGI_FORMAT_R8G8B8A8_UNORM
            : DXGI_FORMAT_UNKNOWN;
    render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    render_target_view_desc.Texture2D.MipSlice = 0;
    HRESULT hr = render_state_.d3d11_device_->CreateRenderTargetView(
        render_state_.target_texture_.Get(), &render_target_view_desc,
        &render_state_.render_target_view_);
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::EnsureRenderTargetView2, hr);
      return false;
    }
  }
  return true;
}

bool D3D11TextureHelper::EnsureShaders() {
  if (!render_state_.vertex_shader_) {
    HRESULT hr = render_state_.d3d11_device_->CreateVertexShader(
        g_vertex, _countof(g_vertex), nullptr, &render_state_.vertex_shader_);
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::EnsureVS, hr);
      return false;
    }
  }

  if (!render_state_.geometry_shader_) {
    HRESULT hr = render_state_.d3d11_device_->CreateGeometryShader(
        g_geometry, _countof(g_geometry), nullptr,
        &render_state_.geometry_shader_);
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::EnsureGS, hr);
      return false;
    }
  }

  if (!render_state_.flip_pixel_shader_) {
    HRESULT hr = render_state_.d3d11_device_->CreatePixelShader(
        g_flip_pixel, _countof(g_flip_pixel), nullptr,
        &render_state_.flip_pixel_shader_);
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::EnsurePS, hr);
      return false;
    }
  }

  return true;
}

bool D3D11TextureHelper::EnsureInputLayout() {
  if (!render_state_.input_layout_) {
    D3D11_INPUT_ELEMENT_DESC vertex_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    HRESULT hr = render_state_.d3d11_device_->CreateInputLayout(
        vertex_desc, std::size(vertex_desc), g_vertex, _countof(g_vertex),
        &render_state_.input_layout_);
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::InputLayout, hr);
      return false;
    }
  }
  return true;
}

bool D3D11TextureHelper::EnsureVertexBuffer() {
  if (!render_state_.vertex_buffer_) {
    // Pairs of x/y coordinates, and UVs for 2 triangles in a quad.
    CD3D11_BUFFER_DESC vertex_buffer_desc(kSizeOfVertex * kNumVerticesPerLayer,
                                          D3D11_BIND_VERTEX_BUFFER);
    HRESULT hr = render_state_.d3d11_device_->CreateBuffer(
        &vertex_buffer_desc, nullptr, &render_state_.vertex_buffer_);
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::VertexBuffer, hr);
      return false;
    }
  }
  return true;
}

bool D3D11TextureHelper::EnsureSampler(LayerData& layer) {
  if (!layer.sampler_) {
    CD3D11_DEFAULT default_values;
    CD3D11_SAMPLER_DESC sampler_desc = CD3D11_SAMPLER_DESC(default_values);
    D3D11_SAMPLER_DESC sd = sampler_desc;
    HRESULT hr =
        render_state_.d3d11_device_->CreateSamplerState(&sd, &(layer.sampler_));
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::Sampler, hr);
      return false;
    }
  }
  return true;
}

bool D3D11TextureHelper::BindTarget() {
  if (!EnsureRenderTargetView())
    return false;

  ID3D11RenderTargetView* render_target_views[] = {
      render_state_.render_target_view_.Get()};
  render_state_.d3d11_device_context_->OMSetRenderTargets(
      ARRAYSIZE(render_target_views), render_target_views, nullptr);
  return true;
}

void PushVertRect(std::vector<Vertex2D>& data,
                  const gfx::RectF& rect,
                  const gfx::RectF& uv,
                  int target) {
  Vertex2D vert;
  vert.target = target;

  vert.x = rect.x() * 2 - 1;
  vert.y = rect.y() * 2 - 1;
  vert.u = uv.x();
  vert.v = uv.y();
  data.push_back(vert);

  vert.x = rect.x() * 2 - 1;
  vert.y = (rect.y() + rect.height()) * 2 - 1;
  vert.u = uv.x();
  vert.v = uv.y() + uv.height();
  data.push_back(vert);

  vert.x = (rect.x() + rect.width()) * 2 - 1;
  vert.y = (rect.y() + rect.height()) * 2 - 1;
  vert.u = uv.x() + uv.width();
  vert.v = uv.y() + uv.height();
  data.push_back(vert);

  vert.x = rect.x() * 2 - 1;
  vert.y = rect.y() * 2 - 1;
  vert.u = uv.x();
  vert.v = uv.y();
  data.push_back(vert);

  vert.x = (rect.x() + rect.width()) * 2 - 1;
  vert.y = (rect.y() + rect.height()) * 2 - 1;
  vert.u = uv.x() + uv.width();
  vert.v = uv.y() + uv.height();
  data.push_back(vert);

  vert.x = (rect.x() + rect.width()) * 2 - 1;
  vert.y = rect.y() * 2 - 1;
  vert.u = uv.x() + uv.width();
  vert.v = uv.y();
  data.push_back(vert);
}

bool D3D11TextureHelper::UpdateVertexBuffer(LayerData& layer) {
  std::vector<Vertex2D> vertex_data;
  PushVertRect(vertex_data, target_left_, layer.left_, 0);
  PushVertRect(vertex_data, target_right_, layer.right_, 1);
  render_state_.d3d11_device_context_->UpdateSubresource(
      render_state_.vertex_buffer_.Get(), 0, nullptr, vertex_data.data(),
      sizeof(Vertex2D), vertex_data.size());
  return true;
}

bool D3D11TextureHelper::CompositeLayer(LayerData& layer) {
  if (!EnsureShaders() || !EnsureInputLayout() || !EnsureVertexBuffer() ||
      !EnsureSampler(layer) || !UpdateVertexBuffer(layer))
    return false;

  render_state_.d3d11_device_context_->VSSetShader(
      render_state_.vertex_shader_.Get(), nullptr, 0);
  render_state_.d3d11_device_context_->PSSetShader(
      render_state_.flip_pixel_shader_.Get(), nullptr, 0);
  render_state_.d3d11_device_context_->IASetInputLayout(
      render_state_.input_layout_.Get());

  UINT stride = kSizeOfVertex;
  UINT offset = 0;
  ID3D11Buffer* vertex_buffers[] = {render_state_.vertex_buffer_.Get()};
  ID3D11SamplerState* samplers[] = {layer.sampler_.Get()};
  render_state_.d3d11_device_context_->IASetVertexBuffers(
      0, ARRAYSIZE(vertex_buffers), vertex_buffers, &stride, &offset);
  render_state_.d3d11_device_context_->PSSetSamplers(0, ARRAYSIZE(samplers),
                                                     samplers);

  D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
  shader_resource_view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  shader_resource_view_desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
  shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
  shader_resource_view_desc.Texture2D.MipLevels = 1;
  HRESULT hr = render_state_.d3d11_device_->CreateShaderResourceView(
      layer.source_texture_.Get(), &shader_resource_view_desc,
      &layer.shader_resource_);
  if (FAILED(hr)) {
    TraceDXError(ErrorLocation::ShaderResource, hr);
    return false;
  }

  ID3D11ShaderResourceView* shader_resources[] = {layer.shader_resource_.Get()};
  render_state_.d3d11_device_context_->PSSetShaderResources(
      0, ARRAYSIZE(shader_resources), shader_resources);

  D3D11_TEXTURE2D_DESC desc;
  render_state_.target_texture_->GetDesc(&desc);
  D3D11_VIEWPORT viewport = {
      0, 0, static_cast<float>(desc.Width), static_cast<float>(desc.Height),
      0, 1};
  render_state_.d3d11_device_context_->RSSetViewports(1, &viewport);
  render_state_.d3d11_device_context_->IASetPrimitiveTopology(
      D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // TODO(billorr): Optimize to avoid the geometry shader when not needed.
  render_state_.d3d11_device_context_->GSSetShader(
      render_state_.geometry_shader_.Get(), nullptr, 0);
  render_state_.d3d11_device_context_->Draw(kNumVerticesPerLayer, 0);
  return true;
}

void D3D11TextureHelper::SetSourceTexture(
    base::win::ScopedHandle texture_handle,
    const gpu::SyncToken& sync_token,
    gfx::RectF left,
    gfx::RectF right) {
  TRACE_EVENT0("xr", "SetSourceTexture");
  render_state_.source_.source_texture_ = nullptr;
  render_state_.source_.keyed_mutex_ = nullptr;
  render_state_.source_.sync_token_.Clear();
  render_state_.source_.left_ = left;
  render_state_.source_.right_ = right;
  render_state_.source_.submitted_this_frame_ = true;

  if (!texture_handle.IsValid()) {
    return;
  }

  if (!EnsureInitialized())
    return;

  HRESULT hr = render_state_.d3d11_device_->OpenSharedResource1(
      texture_handle.Get(),
      IID_PPV_ARGS(&(render_state_.source_.keyed_mutex_)));
  if (FAILED(hr)) {
    TraceDXError(ErrorLocation::OpenSource, hr);
    return;
  }
  hr = render_state_.source_.keyed_mutex_.As(
      &(render_state_.source_.source_texture_));
  if (FAILED(hr)) {
    render_state_.source_.keyed_mutex_ = nullptr;
    return;
  }
  render_state_.source_.sync_token_ = sync_token;
}

bool D3D11TextureHelper::SetOverlayTexture(
    base::win::ScopedHandle texture_handle,
    const gpu::SyncToken& sync_token,
    gfx::RectF left,
    gfx::RectF right) {
  render_state_.overlay_.source_texture_ = nullptr;
  render_state_.overlay_.keyed_mutex_ = nullptr;
  render_state_.overlay_.sync_token_.Clear();
  render_state_.overlay_.left_ = left;
  render_state_.overlay_.right_ = right;
  render_state_.overlay_.submitted_this_frame_ = true;

  if (!EnsureInitialized())
    return false;
  HRESULT hr = render_state_.d3d11_device_->OpenSharedResource1(
      texture_handle.Get(),
      IID_PPV_ARGS(&(render_state_.overlay_.keyed_mutex_)));
  if (FAILED(hr)) {
    TraceDXError(ErrorLocation::OpenOverlay, hr);
    return false;
  }
  hr = render_state_.overlay_.keyed_mutex_.As(
      &(render_state_.overlay_.source_texture_));
  if (FAILED(hr)) {
    render_state_.overlay_.keyed_mutex_ = nullptr;
    return false;
  }
  render_state_.overlay_.sync_token_ = sync_token;

  return true;
}

bool D3D11TextureHelper::UpdateBackbufferSizes() {
  if (!EnsureInitialized())
    return false;

  // Source texture is optional depending on whether we're using
  // shared images for the destination.
  if (!render_state_.source_.source_texture_ &&
      !render_state_.overlay_.source_texture_)
    return true;

  if (force_viewport_) {
    target_size_ = default_size_;
    return true;
  }

  if (render_state_.source_.source_texture_ &&
      render_state_.overlay_.source_texture_) {
    target_left_ = gfx::RectF(0 /*x*/, 0 /*y*/, 0.5f /*width*/, 1 /*height*/);
    target_right_ =
        gfx::RectF(0.5f /*x*/, 0 /*y*/, 0.5f /*width*/, 1 /*height*/);
    target_size_ = default_size_;
    return true;
  }

  LayerData* layer = render_state_.overlay_.source_texture_
                         ? &render_state_.overlay_
                         : &render_state_.source_;
  D3D11_TEXTURE2D_DESC desc_desired;
  layer->source_texture_->GetDesc(&desc_desired);
  target_left_ = layer->left_;
  target_right_ = layer->right_;
  target_size_ = gfx::Size(desc_desired.Width, desc_desired.Height);
  return true;
}

void D3D11TextureHelper::SetBackbuffer(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer) {
  if (render_state_.target_texture_ != back_buffer) {
    render_state_.render_target_view_ = nullptr;
  }
  render_state_.target_texture_ = back_buffer;
}

Microsoft::WRL::ComPtr<IDXGIAdapter> D3D11TextureHelper::GetAdapter() {
  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
  if (FAILED(hr))
    return nullptr;
  // We don't have a valid adapter index, lets see if we have a valid LUID.
  Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory4;
  hr = dxgi_factory.As(&dxgi_factory4);
  if (FAILED(hr))
    return nullptr;
  dxgi_factory4->EnumAdapterByLuid(adapter_luid_, IID_PPV_ARGS(&adapter));
  return adapter;
}

Microsoft::WRL::ComPtr<ID3D11Device> D3D11TextureHelper::GetDevice() {
  EnsureInitialized();
  return render_state_.d3d11_device_;
}

bool D3D11TextureHelper::EnsureInitialized() {
  if (render_state_.d3d11_device_ &&
      SUCCEEDED(render_state_.d3d11_device_->GetDeviceRemovedReason()))
    return true;  // Already initialized.

  // If we were previously initialized, but lost the device, throw away old
  // state.  This will be initialized lazily as needed.
  render_state_ = {};

  D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};
  UINT flags = bgra_ ? D3D11_CREATE_DEVICE_BGRA_SUPPORT : 0;
  D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_1;

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter = GetAdapter();
  if (!adapter) {
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  HRESULT hr = D3D11CreateDevice(
      adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, feature_levels,
      std::size(feature_levels), D3D11_SDK_VERSION, &d3d11_device,
      &feature_level_out, &(render_state_.d3d11_device_context_));
  if (SUCCEEDED(hr)) {
    hr = d3d11_device.As(&render_state_.d3d11_device_);
    if (FAILED(hr)) {
      render_state_.d3d11_device_context_ = nullptr;
    }
  }
  TraceDXError(ErrorLocation::CreateDevice, hr);
  return SUCCEEDED(hr);
}

bool D3D11TextureHelper::SetAdapterLUID(const LUID& luid) {
  adapter_luid_ = luid;
  return true;
}

}  // namespace device
