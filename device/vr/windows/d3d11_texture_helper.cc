// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows/d3d11_texture_helper.h"
#include "base/stl_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/c/system/platform_handle.h"

namespace {
#include "device/vr/windows/flip_pixel_shader.h"
#include "device/vr/windows/geometry_shader.h"
#include "device/vr/windows/vertex_shader.h"

constexpr int kAcquireWaitMS = 2000;

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
}

namespace {

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
  EnsureCB = 10,
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

D3D11TextureHelper::D3D11TextureHelper() {}

D3D11TextureHelper::~D3D11TextureHelper() {}

void D3D11TextureHelper::Reset() {
  render_state_ = {};
}

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
        &blenddesc,
        render_state_.overlay_blend_state_.ReleaseAndGetAddressOf());
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
        &blenddesc,
        render_state_.content_blend_state_.ReleaseAndGetAddressOf());
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

bool D3D11TextureHelper::CompositeToBackBuffer(mojom::XRFrameDataPtr &frame_data_) {
  if (!EnsureInitialized())
    return false;

  // Clear stale data:
  CleanupLayerData(render_state_.source_);
  CleanupLayerData(render_state_.overlay_);

  if (!render_state_.source_.source_texture_ &&
      !render_state_.overlay_.source_texture_)
    return false;
  if (!render_state_.target_texture_)
    return false;

  HRESULT hr = S_OK;
  if (render_state_.source_.keyed_mutex_) {
    hr = render_state_.source_.keyed_mutex_->AcquireSync(1, kAcquireWaitMS);
    if (FAILED(hr) || hr == WAIT_TIMEOUT || hr == WAIT_ABANDONED) {
      // We failed to acquire the lock.  We'll drop this frame, but subsequent
      // frames won't be affected.
      TraceDXError(ErrorLocation::SourceTimeout, hr);
      return false;
    }
  }

  if (render_state_.overlay_.keyed_mutex_) {
    hr = render_state_.overlay_.keyed_mutex_->AcquireSync(1, kAcquireWaitMS);
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
              CompositeLayer(render_state_.source_, frame_data_);
  if (render_state_.overlay_.source_texture_)
    success = success && EnsureOverlayBlendState() &&
              CompositeLayer(render_state_.overlay_, frame_data_);

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


  if (!render_state_.cbuffer_) {
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = 32 * sizeof(float);
    // cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    // cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    // cbDesc.MiscFlags = 0;
    // cbDesc.StructureByteStride = 0;

    // x = -1+far/near
    // y = 1
    // z = x/far
    // w = 1/far

    // Create the buffer.
    HRESULT hr = render_state_.d3d11_device_->CreateBuffer(
      &cbDesc,
      NULL,
      &render_state_.cbuffer_
    );
    if (FAILED(hr)) {
      TraceDXError(ErrorLocation::EnsureCB, hr);
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
        vertex_desc, base::size(vertex_desc), g_vertex, _countof(g_vertex),
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
    HRESULT hr = render_state_.d3d11_device_->CreateSamplerState(
        &sd, layer.sampler_.GetAddressOf());
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

  render_state_.d3d11_device_context_->OMSetRenderTargets(
      1, render_state_.render_target_view_.GetAddressOf(), nullptr);
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

constexpr float m_eyeFOV = 115.0f;
constexpr float M_PI = 3.14159265358979323846;
inline void composeMatrix(float *matrix, const float *position, const float *quaternion, const float *scale) {
  float *te = matrix;

  float x = quaternion[0], y = quaternion[1], z = quaternion[2], w = quaternion[3];
  float x2 = x + x, y2 = y + y, z2 = z + z;
  float xx = x * x2, xy = x * y2, xz = x * z2;
  float yy = y * y2, yz = y * z2, zz = z * z2;
  float wx = w * x2, wy = w * y2, wz = w * z2;

  float sx = scale[0], sy = scale[1], sz = scale[2];

  te[ 0 ] = ( 1 - ( yy + zz ) ) * sx;
  te[ 1 ] = ( xy + wz ) * sx;
  te[ 2 ] = ( xz - wy ) * sx;
  te[ 3 ] = 0;

  te[ 4 ] = ( xy - wz ) * sy;
  te[ 5 ] = ( 1 - ( xx + zz ) ) * sy;
  te[ 6 ] = ( yz + wx ) * sy;
  te[ 7 ] = 0;

  te[ 8 ] = ( xz + wy ) * sz;
  te[ 9 ] = ( yz - wx ) * sz;
  te[ 10 ] = ( 1 - ( xx + yy ) ) * sz;
  te[ 11 ] = 0;

  te[ 12 ] = position[0];
  te[ 13 ] = position[1];
  te[ 14 ] = position[2];
  te[ 15 ] = 1;
}
inline void multiplyMatrices(float *outMatrix, const float *aMatrix, const float *bMatrix) {
  const float *ae = aMatrix;
  const float *be = bMatrix;
  float *te = outMatrix;

  const float a11 = ae[ 0 ], a12 = ae[ 4 ], a13 = ae[ 8 ], a14 = ae[ 12 ];
  const float a21 = ae[ 1 ], a22 = ae[ 5 ], a23 = ae[ 9 ], a24 = ae[ 13 ];
  const float a31 = ae[ 2 ], a32 = ae[ 6 ], a33 = ae[ 10 ], a34 = ae[ 14 ];
  const float a41 = ae[ 3 ], a42 = ae[ 7 ], a43 = ae[ 11 ], a44 = ae[ 15 ];

  const float b11 = be[ 0 ], b12 = be[ 4 ], b13 = be[ 8 ], b14 = be[ 12 ];
  const float b21 = be[ 1 ], b22 = be[ 5 ], b23 = be[ 9 ], b24 = be[ 13 ];
  const float b31 = be[ 2 ], b32 = be[ 6 ], b33 = be[ 10 ], b34 = be[ 14 ];
  const float b41 = be[ 3 ], b42 = be[ 7 ], b43 = be[ 11 ], b44 = be[ 15 ];

  te[ 0 ] = a11 * b11 + a12 * b21 + a13 * b31 + a14 * b41;
  te[ 4 ] = a11 * b12 + a12 * b22 + a13 * b32 + a14 * b42;
  te[ 8 ] = a11 * b13 + a12 * b23 + a13 * b33 + a14 * b43;
  te[ 12 ] = a11 * b14 + a12 * b24 + a13 * b34 + a14 * b44;

  te[ 1 ] = a21 * b11 + a22 * b21 + a23 * b31 + a24 * b41;
  te[ 5 ] = a21 * b12 + a22 * b22 + a23 * b32 + a24 * b42;
  te[ 9 ] = a21 * b13 + a22 * b23 + a23 * b33 + a24 * b43;
  te[ 13 ] = a21 * b14 + a22 * b24 + a23 * b34 + a24 * b44;

  te[ 2 ] = a31 * b11 + a32 * b21 + a33 * b31 + a34 * b41;
  te[ 6 ] = a31 * b12 + a32 * b22 + a33 * b32 + a34 * b42;
  te[ 10 ] = a31 * b13 + a32 * b23 + a33 * b33 + a34 * b43;
  te[ 14 ] = a31 * b14 + a32 * b24 + a33 * b34 + a34 * b44;

  te[ 3 ] = a41 * b11 + a42 * b21 + a43 * b31 + a44 * b41;
  te[ 7 ] = a41 * b12 + a42 * b22 + a43 * b32 + a44 * b42;
  te[ 11 ] = a41 * b13 + a42 * b23 + a43 * b33 + a44 * b43;
  te[ 15 ] = a41 * b14 + a42 * b24 + a43 * b34 + a44 * b44;
}
inline void makeScaleMatrix(float *outMatrix, const float *scale) {
  float *te = outMatrix;
  
  te[ 0 ] = scale[0];
  te[ 1 ] = 0;
  te[ 2 ] = 0;
  te[ 3 ] = 0;

  te[ 4 ] = 0;
  te[ 5 ] = scale[1];
  te[ 6 ] = 0;
  te[ 7 ] = 0;

  te[ 8 ] = 0;
  te[ 9 ] = 0;
  te[ 10 ] = scale[2];
  te[ 11 ] = 0;

  te[ 12 ] = 0;
  te[ 13 ] = 0;
  te[ 14 ] = 0;
  te[ 15 ] = 1;
}

bool D3D11TextureHelper::CompositeLayer(LayerData& layer, mojom::XRFrameDataPtr &frame_data_) {
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
  render_state_.d3d11_device_context_->IASetVertexBuffers(
      0, 1, render_state_.vertex_buffer_.GetAddressOf(), &stride, &offset);
  render_state_.d3d11_device_context_->PSSetSamplers(
      0, 1, layer.sampler_.GetAddressOf());

  D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
  shader_resource_view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  shader_resource_view_desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
  shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
  shader_resource_view_desc.Texture2D.MipLevels = 1;
  HRESULT hr = render_state_.d3d11_device_->CreateShaderResourceView(
      layer.source_texture_.Get(), &shader_resource_view_desc,
      layer.shader_resource_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    TraceDXError(ErrorLocation::ShaderResource, hr);
    return false;
  }
  render_state_.d3d11_device_context_->PSSetShaderResources(
      0, 1, layer.shader_resource_.GetAddressOf());

  D3D11_TEXTURE2D_DESC desc;
  render_state_.target_texture_->GetDesc(&desc);
  D3D11_VIEWPORT viewport = {0, 0, desc.Width, desc.Height, 0, 1};
  render_state_.d3d11_device_context_->RSSetViewports(1, &viewport);
  render_state_.d3d11_device_context_->IASetPrimitiveTopology(
      D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // float4x4 lookRotation;
  // float halfFOVInRadians;
  auto &pose = frame_data_->pose;
  auto &posePosition = pose->position;
  auto &poseOrientation = pose->orientation;
  float position[3] = {0, 0, 0};
  float quaternion[4] = {poseOrientation->x(), poseOrientation->y(), poseOrientation->z(), poseOrientation->w()};
  TRACE_EVENT1("gpu", "OpenVR Composite px", "x", posePosition->x());
  TRACE_EVENT1("gpu", "OpenVR Composite py", "y", posePosition->y());
  TRACE_EVENT1("gpu", "OpenVR Composite pz", "z", posePosition->z());
  TRACE_EVENT1("gpu", "OpenVR Composite qx", "x", poseOrientation->x());
  TRACE_EVENT1("gpu", "OpenVR Composite qy", "y", poseOrientation->y());
  TRACE_EVENT1("gpu", "OpenVR Composite qz", "z", poseOrientation->z());
  TRACE_EVENT1("gpu", "OpenVR Composite qw", "w", poseOrientation->w());
  float scale[3] = {1, 1, 1};
  float lookRotation[16];
  composeMatrix(lookRotation, position, quaternion, scale);

  /* float scale2[3] = {1, 1, -1};
  float scaleMatrix[16];
  makeScaleMatrix(scaleMatrix, scale2);
  
  multiplyMatrices(lookRotation, scaleMatrix, lookRotation); */
  
  float localVsData[32] = {};
  memcpy(localVsData, lookRotation, sizeof(lookRotation));
  const float halfFOVInRadians = (m_eyeFOV / 2.0f) * (M_PI / 180.0f);
  localVsData[16] = halfFOVInRadians;
  TRACE_EVENT1("gpu", "OpenVR Composite cbuffer", "0", localVsData[0]);
  TRACE_EVENT1("gpu", "OpenVR Composite cbuffer", "1", localVsData[1]);
  TRACE_EVENT1("gpu", "OpenVR Composite cbuffer", "2", localVsData[2]);
  TRACE_EVENT1("gpu", "OpenVR Composite cbuffer", "3", localVsData[3]);
  TRACE_EVENT1("gpu", "OpenVR Composite cbuffer", "4", localVsData[4]);
  
  render_state_.d3d11_device_context_->UpdateSubresource(render_state_.cbuffer_.Get(), 0, 0, localVsData, 0, 0);
  render_state_.d3d11_device_context_->PSSetConstantBuffers(0, 1, render_state_.cbuffer_.GetAddressOf());
  
  /* {
    ID3D11InfoQueue *infoQueue;
    render_state_.d3d11_device_->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&infoQueue);
    UINT64 numStoredMessages = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 i = 0; i < numStoredMessages; i++) {
      size_t messageSize = 0;
      HRESULT hr = infoQueue->GetMessage(
        i,
        nullptr,
        &messageSize
      );
      if (SUCCEEDED(hr)) {
        D3D11_MESSAGE *message = (D3D11_MESSAGE *)malloc(messageSize);

        hr = infoQueue->GetMessage(
          i,
          message,
          &messageSize
        );
        if (SUCCEEDED(hr)) {
          // if (message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING) {
            std::string s = "info: ";
            s += std::to_string(message->Severity);
            s += " ";
            s += std::string(message->pDescription, message->DescriptionByteLength);
          // }
          TRACE_EVENT1("gpu", "OpenVR log 2", "log", s);
        } else {
          TRACE_EVENT1("gpu", "failed to get info queue message size", "hr", hr);
        }

        free(message);
      } else {
        TRACE_EVENT1("gpu", "failed to get info queue message size", "hr", hr);
      }
    }
    infoQueue->ClearStoredMessages();
    infoQueue->Release();
  } */

  /* vr::TrackedDevicePose_t rRenderPoses[vr::k_unMaxTrackedDeviceCount];
  if (vr::VRCompositor()->CanRenderScene() == false)
          return;

  uint64_t newFrameIndex = 0;
  float lastVSync = 0;

  while (newFrameIndex == m_lastFrameIndex)
  {
          auto vsyncTimesAvailable = vr::VRSystem()->GetTimeSinceLastVsync(
                  &lastVSync, &newFrameIndex);

          if (vsyncTimesAvailable == false)
                  return;
  }

  if (m_lastFrameIndex + 1 < newFrameIndex)
          m_framesSkipped++;

  m_lastFrameIndex = newFrameIndex;
  float secondsSinceLastVsync = 0;
  uint64_t newLastFrame = 0;
  vr::VRSystem()->GetTimeSinceLastVsync(&secondsSinceLastVsync, &newLastFrame);

  vr::ETrackedPropertyError error;
  float displayFrequency = vr::VRSystem()->GetFloatTrackedDeviceProperty(
          vr::k_unTrackedDeviceIndex_Hmd,
          vr::ETrackedDeviceProperty::Prop_DisplayFrequency_Float,
          &error);

  float frameDuration = 1.0f / displayFrequency;
  float vsyncToPhotons = vr::VRSystem()->GetFloatTrackedDeviceProperty(
          vr::k_unTrackedDeviceIndex_Hmd,
          vr::ETrackedDeviceProperty::Prop_SecondsFromVsyncToPhotons_Float,
          &error);

  float predictedSecondsFromNow = frameDuration - secondsSinceLastVsync + vsyncToPhotons;
  vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
          c_eTrackingOrigin,
          predictedSecondsFromNow,
          &rRenderPoses[0],
          vr::k_unMaxTrackedDeviceCount);

  glm::mat4 universeFromHmd = glmMatFromVrMat(
          rRenderPoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);

  m_hmdFromUniverse = glm::inverse(universeFromHmd);
  m_universeFromOriginTransforms["/user/head"] = universeFromHmd;

  m_universeFromOriginTransforms["/space/stage"] = glm::mat4(1.f);
  m_vargglesLookRotation = glm::scale(m_hmdFromUniverse, glm::vec3(1, 1, -1)); */
  
  
  
  
        
        
        

  // TODO(billorr): Optimize to avoid the geometry shader when not needed.
  render_state_.d3d11_device_context_->GSSetShader(
      render_state_.geometry_shader_.Get(), nullptr, 0);
  render_state_.d3d11_device_context_->Draw(kNumVerticesPerLayer, 0);

  return true;
}

bool D3D11TextureHelper::SetSourceTexture(
    base::win::ScopedHandle texture_handle,
    gfx::RectF left,
    gfx::RectF right) {
  TRACE_EVENT0("xr", "SetSourceTexture");
  render_state_.source_.source_texture_ = nullptr;
  render_state_.source_.keyed_mutex_ = nullptr;
  render_state_.source_.left_ = left;
  render_state_.source_.right_ = right;
  render_state_.source_.submitted_this_frame_ = true;

  if (!EnsureInitialized())
    return false;
  HRESULT hr = render_state_.d3d11_device_->OpenSharedResource1(
      texture_handle.Get(),
      IID_PPV_ARGS(
          render_state_.source_.keyed_mutex_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    TraceDXError(ErrorLocation::OpenSource, hr);
    return false;
  }
  hr = render_state_.source_.keyed_mutex_.CopyTo(
      render_state_.source_.source_texture_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    render_state_.source_.keyed_mutex_ = nullptr;
    return false;
  }

  return true;
}

bool D3D11TextureHelper::SetOverlayTexture(
    base::win::ScopedHandle texture_handle,
    gfx::RectF left,
    gfx::RectF right) {
  render_state_.overlay_.source_texture_ = nullptr;
  render_state_.overlay_.keyed_mutex_ = nullptr;
  render_state_.overlay_.left_ = left;
  render_state_.overlay_.right_ = right;
  render_state_.overlay_.submitted_this_frame_ = true;

  if (!EnsureInitialized())
    return false;
  HRESULT hr = render_state_.d3d11_device_->OpenSharedResource1(
      texture_handle.Get(),
      IID_PPV_ARGS(
          render_state_.overlay_.keyed_mutex_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    TraceDXError(ErrorLocation::OpenOverlay, hr);
    return false;
  }
  hr = render_state_.overlay_.keyed_mutex_.CopyTo(
      render_state_.overlay_.source_texture_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    render_state_.overlay_.keyed_mutex_ = nullptr;
    return false;
  }

  return true;
}

bool D3D11TextureHelper::UpdateBackbufferSizes() {
  if (!EnsureInitialized())
    return false;

  if (!render_state_.source_.source_texture_ &&
      !render_state_.overlay_.source_texture_)
    return false;

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

void D3D11TextureHelper::AllocateBackBuffer() {
  if (!EnsureInitialized())
    return;

  // If we don't have anything to composite, just return.
  if (!render_state_.source_.source_texture_ &&
      !render_state_.overlay_.source_texture_)
    return;

  LayerData* layer = render_state_.overlay_.source_texture_
                         ? &render_state_.overlay_
                         : &render_state_.source_;

  D3D11_TEXTURE2D_DESC desc_desired;
  layer->source_texture_->GetDesc(&desc_desired);
  desc_desired.MiscFlags = 0;
  desc_desired.Width = target_size_.width();
  desc_desired.Height = target_size_.height();

  if (render_state_.target_texture_) {
    D3D11_TEXTURE2D_DESC desc_target;
    render_state_.target_texture_->GetDesc(&desc_target);
    // If the target should change size, format, or other properties reallocate
    // a new texture and new render target view.
    if (desc_desired.Width != desc_target.Width ||
        desc_desired.Height != desc_target.Height ||
        desc_desired.MipLevels != desc_target.MipLevels ||
        desc_desired.ArraySize != desc_target.ArraySize ||
        desc_desired.Format != desc_target.Format ||
        desc_desired.SampleDesc.Count != desc_target.SampleDesc.Count ||
        desc_desired.SampleDesc.Quality != desc_target.SampleDesc.Quality ||
        desc_desired.Usage != desc_target.Usage ||
        desc_desired.BindFlags != desc_target.BindFlags ||
        desc_desired.CPUAccessFlags != desc_target.CPUAccessFlags ||
        desc_desired.MiscFlags != desc_target.MiscFlags) {
      render_state_.target_texture_ = nullptr;
      render_state_.render_target_view_ = nullptr;
    }
  }

  if (!render_state_.target_texture_) {
    // Ignoring error - target_texture_ will be null on failure.
    render_state_.d3d11_device_->CreateTexture2D(
        &desc_desired, nullptr,
        render_state_.target_texture_.ReleaseAndGetAddressOf());
  }
}

const Microsoft::WRL::ComPtr<ID3D11Texture2D>&
D3D11TextureHelper::GetBackbuffer() {
  return render_state_.target_texture_;
}

void D3D11TextureHelper::DiscardView() {
  if (render_state_.render_target_view_ &&
      render_state_.d3d11_device_context_) {
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
    if (SUCCEEDED(render_state_.d3d11_device_context_.As(&context1))) {
      context1->DiscardView(render_state_.render_target_view_.Get());
    }
  }
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
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));
  if (FAILED(hr))
    return nullptr;
  if (adapter_index_ >= 0) {
    dxgi_factory->EnumAdapters(adapter_index_, adapter.GetAddressOf());
  } else {
    // We don't have a valid adapter index, lets see if we have a valid LUID.
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory4;
    hr = dxgi_factory.As(&dxgi_factory4);
    if (FAILED(hr))
      return nullptr;
    dxgi_factory4->EnumAdapterByLuid(adapter_luid_,
                                     IID_PPV_ARGS(adapter.GetAddressOf()));
  }
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
  // flags |= D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_1;

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter = GetAdapter();
  if (!adapter) {
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  HRESULT hr = D3D11CreateDevice(
      adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, feature_levels,
      base::size(feature_levels), D3D11_SDK_VERSION,
      d3d11_device.GetAddressOf(), &feature_level_out,
      render_state_.d3d11_device_context_.GetAddressOf());
  if (SUCCEEDED(hr)) {
    hr = d3d11_device.As(&render_state_.d3d11_device_);
    if (FAILED(hr)) {
      render_state_.d3d11_device_context_ = nullptr;
    }
  }
  TraceDXError(ErrorLocation::CreateDevice, hr);
  return SUCCEEDED(hr);
}

bool D3D11TextureHelper::SetAdapterIndex(int32_t index) {
  adapter_index_ = index;
  return (index >= 0);
}

bool D3D11TextureHelper::SetAdapterLUID(const LUID& luid) {
  adapter_luid_ = luid;
  adapter_index_ = -1;
  return true;
}

}  // namespace device
