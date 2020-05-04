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

bool D3D11TextureHelper::CompositeToBackBuffer(mojom::XRFrameDataPtr &frame_data_, mojom::VRDisplayInfoPtr &display_info) {
  if (!EnsureInitialized())
    return false;

  // Clear stale data:
  CleanupLayerData(render_state_.source_);
  // CleanupLayerData(render_state_.overlay_);

  if (!render_state_.source_.source_texture_ /* &&
      !render_state_.overlay_.source_texture_ */)
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

  /* if (render_state_.overlay_.keyed_mutex_) {
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
  } */

  if (!BindTarget()) {
    TraceDXError(ErrorLocation::BindTarget, hr);
    return false;
  }

  /* if (render_state_.overlay_.source_texture_ &&
      (!render_state_.source_.source_texture_ || !source_visible_)) {
    // If we have an overlay, but no WebXR texture under it, clear the target
    // first, since overlay may assume transparency.
    float color_rgba[4] = {0, 0, 0, 1};
    render_state_.d3d11_device_context_->ClearRenderTargetView(
        render_state_.render_target_view_.Get(), color_rgba);
  } */

  bool success = true;
  if (render_state_.source_.source_texture_)
    success = success && EnsureContentBlendState() &&
              CompositeLayer(render_state_.source_, frame_data_, display_info);
  /* if (render_state_.overlay_.source_texture_)
    success = success && EnsureOverlayBlendState() &&
              CompositeLayer(render_state_.overlay_, frame_data_, display_info); */

  if (render_state_.source_.keyed_mutex_)
    render_state_.source_.keyed_mutex_->ReleaseSync(0);
  /* if (render_state_.overlay_.keyed_mutex_)
    render_state_.overlay_.keyed_mutex_->ReleaseSync(0); */

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

// constexpr float m_eyeFOV = 115.0f;
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
void getMatrixInverse(float *outMatrix, const float *inMatrix) {
  // based on http://www.euclideanspace.com/maths/algebra/matrix/functions/inverse/fourD/index.htm
  float *te = outMatrix;
  const float *me = inMatrix;

  const float n11 = me[ 0 ], n21 = me[ 1 ], n31 = me[ 2 ], n41 = me[ 3 ],
    n12 = me[ 4 ], n22 = me[ 5 ], n32 = me[ 6 ], n42 = me[ 7 ],
    n13 = me[ 8 ], n23 = me[ 9 ], n33 = me[ 10 ], n43 = me[ 11 ],
    n14 = me[ 12 ], n24 = me[ 13 ], n34 = me[ 14 ], n44 = me[ 15 ],

    t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44,
    t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44,
    t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44,
    t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

  const float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;

  if (det == 0.0f) {
    TRACE_EVENT0("gpu", "Can't invert matrix, determinant is 0");
  }

  const float detInv = 1.0f / det;

  te[ 0 ] = t11 * detInv;
  te[ 1 ] = ( n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44 ) * detInv;
  te[ 2 ] = ( n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44 ) * detInv;
  te[ 3 ] = ( n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43 ) * detInv;

  te[ 4 ] = t12 * detInv;
  te[ 5 ] = ( n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44 ) * detInv;
  te[ 6 ] = ( n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44 ) * detInv;
  te[ 7 ] = ( n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43 ) * detInv;

  te[ 8 ] = t13 * detInv;
  te[ 9 ] = ( n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44 ) * detInv;
  te[ 10 ] = ( n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44 ) * detInv;
  te[ 11 ] = ( n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43 ) * detInv;

  te[ 12 ] = t14 * detInv;
  te[ 13 ] = ( n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34 ) * detInv;
  te[ 14 ] = ( n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34 ) * detInv;
  te[ 15 ] = ( n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33 ) * detInv;
}
void makeEulerFromRotationMatrix(float *euler, const float *matrix, const std::string &order) {
  const float *te = matrix;
  float m11 = te[ 0 ], m12 = te[ 4 ], m13 = te[ 8 ];
  float m21 = te[ 1 ], m22 = te[ 5 ], m23 = te[ 9 ];
  float m31 = te[ 2 ], m32 = te[ 6 ], m33 = te[ 10 ];

  if ( order == "XYZ" ) {

    euler[1] = std::asin( std::min<float>(std::max<float>( m13, - 1), 1 ) );

    if ( std::abs( m13 ) < 0.9999999 ) {

      euler[0] = std::atan2( - m23, m33 );
      euler[2] = std::atan2( - m12, m11 );

    } else {

      euler[0] = std::atan2( m32, m22 );
      euler[2] = 0;

    }

  } else if ( order == "YXZ" ) {

    euler[0] = std::asin( - std::min<float>(std::max<float>( m23, - 1), 1 ) );

    if ( std::abs( m23 ) < 0.9999999 ) {

      euler[1] = std::atan2( m13, m33 );
      euler[2] = std::atan2( m21, m22 );

    } else {

      euler[1] = std::atan2( - m31, m11 );
      euler[2] = 0;

    }

  } else if ( order == "ZXY" ) {

    euler[0] = std::asin( std::min<float>(std::max<float>( m32, - 1), 1 ) );

    if ( std::abs( m32 ) < 0.9999999 ) {

      euler[1] = std::atan2( - m31, m33 );
      euler[2] = std::atan2( - m12, m22 );

    } else {

      euler[1] = 0;
      euler[2] = std::atan2( m21, m11 );

    }

  } else if ( order == "ZYX" ) {

    euler[1] = std::asin( - std::min<float>(std::max<float>( m31, - 1), 1 ) );

    if ( std::abs( m31 ) < 0.9999999 ) {

      euler[0] = std::atan2( m32, m33 );
      euler[2] = std::atan2( m21, m11 );

    } else {

      euler[0] = 0;
      euler[2] = std::atan2( - m12, m22 );

    }

  } else if ( order == "YZX" ) {

    euler[2] = std::asin( std::min<float>(std::max<float>( m21, - 1), 1 ) );

    if ( std::abs( m21 ) < 0.9999999 ) {

      euler[0] = std::atan2( - m23, m22 );
      euler[1] = std::atan2( - m31, m11 );

    } else {

      euler[0] = 0;
      euler[1] = std::atan2( m13, m33 );

    }

  } else if ( order == "XZY" ) {

    euler[2] = std::asin( - std::min<float>(std::max<float>( m12, - 1), 1 ) );

    if ( std::abs( m12 ) < 0.9999999 ) {

      euler[0] = std::atan2( m32, m22 );
      euler[1] = std::atan2( m13, m11 );

    } else {

      euler[0] = std::atan2( - m23, m33 );
      euler[1] = 0;

    }

  }
}
void makeQuaternionFromEuler(float *quaternion, const float *euler, const std::string &order) {
  float x = euler[0], y = euler[1], z = euler[2];

  // http://www.mathworks.com/matlabcentral/fileexchange/
  // 	20696-function-to-convert-between-dcm-euler-angles-quaternions-and-euler-vectors/
  //	content/SpinCalc.m

  float c1 = std::cos( x / 2 );
  float c2 = std::cos( y / 2 );
  float c3 = std::cos( z / 2 );

  float s1 = std::sin( x / 2 );
  float s2 = std::sin( y / 2 );
  float s3 = std::sin( z / 2 );

  if ( order == "XYZ" ) {

    quaternion[0] = s1 * c2 * c3 + c1 * s2 * s3;
    quaternion[1] = c1 * s2 * c3 - s1 * c2 * s3;
    quaternion[2] = c1 * c2 * s3 + s1 * s2 * c3;
    quaternion[3] = c1 * c2 * c3 - s1 * s2 * s3;

  } else if ( order == "YXZ" ) {

    quaternion[0] = s1 * c2 * c3 + c1 * s2 * s3;
    quaternion[1] = c1 * s2 * c3 - s1 * c2 * s3;
    quaternion[2] = c1 * c2 * s3 - s1 * s2 * c3;
    quaternion[3] = c1 * c2 * c3 + s1 * s2 * s3;

  } else if ( order == "ZXY" ) {

    quaternion[0] = s1 * c2 * c3 - c1 * s2 * s3;
    quaternion[1] = c1 * s2 * c3 + s1 * c2 * s3;
    quaternion[2] = c1 * c2 * s3 + s1 * s2 * c3;
    quaternion[3] = c1 * c2 * c3 - s1 * s2 * s3;

  } else if ( order == "ZYX" ) {

    quaternion[0] = s1 * c2 * c3 - c1 * s2 * s3;
    quaternion[1] = c1 * s2 * c3 + s1 * c2 * s3;
    quaternion[2] = c1 * c2 * s3 - s1 * s2 * c3;
    quaternion[3] = c1 * c2 * c3 + s1 * s2 * s3;

  } else if ( order == "YZX" ) {

    quaternion[0] = s1 * c2 * c3 + c1 * s2 * s3;
    quaternion[1] = c1 * s2 * c3 + s1 * c2 * s3;
    quaternion[2] = c1 * c2 * s3 - s1 * s2 * c3;
    quaternion[3] = c1 * c2 * c3 - s1 * s2 * s3;

  } else if ( order == "XZY" ) {

    quaternion[0] = s1 * c2 * c3 - c1 * s2 * s3;
    quaternion[1] = c1 * s2 * c3 - s1 * c2 * s3;
    quaternion[2] = c1 * c2 * s3 + s1 * s2 * c3;
    quaternion[3] = c1 * c2 * c3 + s1 * s2 * s3;

  }
}
void makeMatrixFromEuler(float *matrix, const float *euler, const std::string &order) {
  float *te = matrix;

  float x = euler[0], y = euler[1], z = euler[2];
  float a = std::cos( x ), b = std::sin( x );
  float c = std::cos( y ), d = std::sin( y );
  float e = std::cos( z ), f = std::sin( z );

  if ( order == "XYZ" ) {

    float ae = a * e, af = a * f, be = b * e, bf = b * f;

    te[ 0 ] = c * e;
    te[ 4 ] = - c * f;
    te[ 8 ] = d;

    te[ 1 ] = af + be * d;
    te[ 5 ] = ae - bf * d;
    te[ 9 ] = - b * c;

    te[ 2 ] = bf - ae * d;
    te[ 6 ] = be + af * d;
    te[ 10 ] = a * c;

  } else if ( order == "YXZ" ) {

    float ce = c * e, cf = c * f, de = d * e, df = d * f;

    te[ 0 ] = ce + df * b;
    te[ 4 ] = de * b - cf;
    te[ 8 ] = a * d;

    te[ 1 ] = a * f;
    te[ 5 ] = a * e;
    te[ 9 ] = - b;

    te[ 2 ] = cf * b - de;
    te[ 6 ] = df + ce * b;
    te[ 10 ] = a * c;

  } else if ( order == "ZXY" ) {

    float ce = c * e, cf = c * f, de = d * e, df = d * f;

    te[ 0 ] = ce - df * b;
    te[ 4 ] = - a * f;
    te[ 8 ] = de + cf * b;

    te[ 1 ] = cf + de * b;
    te[ 5 ] = a * e;
    te[ 9 ] = df - ce * b;

    te[ 2 ] = - a * d;
    te[ 6 ] = b;
    te[ 10 ] = a * c;

  } else if ( order == "ZYX" ) {

    float ae = a * e, af = a * f, be = b * e, bf = b * f;

    te[ 0 ] = c * e;
    te[ 4 ] = be * d - af;
    te[ 8 ] = ae * d + bf;

    te[ 1 ] = c * f;
    te[ 5 ] = bf * d + ae;
    te[ 9 ] = af * d - be;

    te[ 2 ] = - d;
    te[ 6 ] = b * c;
    te[ 10 ] = a * c;

  } else if ( order == "YZX" ) {

    float ac = a * c, ad = a * d, bc = b * c, bd = b * d;

    te[ 0 ] = c * e;
    te[ 4 ] = bd - ac * f;
    te[ 8 ] = bc * f + ad;

    te[ 1 ] = f;
    te[ 5 ] = a * e;
    te[ 9 ] = - b * e;

    te[ 2 ] = - d * e;
    te[ 6 ] = ad * f + bc;
    te[ 10 ] = ac - bd * f;

  } else if ( order == "XZY" ) {

    float ac = a * c, ad = a * d, bc = b * c, bd = b * d;

    te[ 0 ] = c * e;
    te[ 4 ] = - f;
    te[ 8 ] = d * e;

    te[ 1 ] = ac * f + bd;
    te[ 5 ] = a * e;
    te[ 9 ] = ad * f - bc;

    te[ 2 ] = bc * f - ad;
    te[ 6 ] = b * e;
    te[ 10 ] = bd * f + ac;

  }

  // bottom row
  te[ 3 ] = 0;
  te[ 7 ] = 0;
  te[ 11 ] = 0;

  // last column
  te[ 12 ] = 0;
  te[ 13 ] = 0;
  te[ 14 ] = 0;
  te[ 15 ] = 1;
}
void transposeMatrix(float *matrix) {
  float *te = matrix;
  float tmp;

  tmp = te[ 1 ]; te[ 1 ] = te[ 4 ]; te[ 4 ] = tmp;
  tmp = te[ 2 ]; te[ 2 ] = te[ 8 ]; te[ 8 ] = tmp;
  tmp = te[ 6 ]; te[ 6 ] = te[ 9 ]; te[ 9 ] = tmp;

  tmp = te[ 3 ]; te[ 3 ] = te[ 12 ]; te[ 12 ] = tmp;
  tmp = te[ 7 ]; te[ 7 ] = te[ 13 ]; te[ 13 ] = tmp;
  tmp = te[ 11 ]; te[ 11 ] = te[ 14 ]; te[ 14 ] = tmp;
}

bool D3D11TextureHelper::CompositeLayer(LayerData& layer, mojom::XRFrameDataPtr &frame_data_, mojom::VRDisplayInfoPtr &display_info) {
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

  float localVsData[32] = {};
  // float4x4 lookRotation;
  // float halfFOVInRadians;
  auto &pose = frame_data_->pose;
  auto &displayInfo = display_info;
  
  // auto &posePosition = pose->position;
  auto &poseOrientation = pose->orientation;
  float position[3] = {0, 0, 0};
  // float position[3] = {posePosition->x(), posePosition->y(), posePosition->z()};
  float quaternion[4] = {poseOrientation->x(), poseOrientation->y(), poseOrientation->z(), poseOrientation->w()};
  float scale[3] = {1, 1, 1};
  
  composeMatrix(localVsData, position, quaternion, scale);
  // getMatrixInverse(localVsData, localVsData);

  float euler[3];
  makeEulerFromRotationMatrix(euler, localVsData, "ZYX");
  // const float eulerZ = euler[2];
  // euler[0] = 0;
  euler[1] *= -1;
  euler[2] *= -1;
  makeMatrixFromEuler(localVsData, euler, "ZYX");

  /* euler[0] = 0;
  euler[1] = 0;
  euler[2] = -eulerZ;
  float eulerMatrix[16];
  makeMatrixFromEuler(eulerMatrix, euler, "ZYX");
  multiplyMatrices(localVsData, localVsData, eulerMatrix); */
  
  transposeMatrix(localVsData);

  TRACE_EVENT1("gpu", "OpenVR fov", "up", displayInfo->left_eye->field_of_view->up_degrees);
  TRACE_EVENT1("gpu", "OpenVR fov", "down", displayInfo->left_eye->field_of_view->down_degrees);
  TRACE_EVENT1("gpu", "OpenVR fov", "left", displayInfo->left_eye->field_of_view->left_degrees);
  TRACE_EVENT1("gpu", "OpenVR fov", "right", displayInfo->left_eye->field_of_view->right_degrees);

  // multiplyMatrices(localVsData, eulerMatrix, localVsData);

  // getMatrixInverse(localVsData, localVsData);

  /* float scale2[3] = {1, 1, -1};
  float scaleMatrix[16];
  makeScaleMatrix(scaleMatrix, scale2);
  
  multiplyMatrices(lookRotation, scaleMatrix, lookRotation); */
  
  // getMatrixInverse(localVsData, frame_data_->universeFromHmd.data());

  /* float scale2[3] = {1, 1, -1};
  float scaleMatrix[16];
  makeScaleMatrix(scaleMatrix, scale2);
  multiplyMatrices(localVsData, localVsData, scaleMatrix); */

  /* glm::mat4 universeFromHmd = glmMatFromVrMat(rRenderPoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
  m_hmdFromUniverse = glm::inverse(universeFromHmd);
  m_vargglesLookRotation = glm::scale(m_hmdFromUniverse, glm::vec3(1, 1, -1)); */

  float m_eyeFOVX = displayInfo->left_eye->field_of_view->left_degrees + displayInfo->left_eye->field_of_view->right_degrees;
  float m_eyeFOVY = displayInfo->left_eye->field_of_view->up_degrees + displayInfo->left_eye->field_of_view->down_degrees;
  const float fovScalarX = std::tan((m_eyeFOVX / 2.0f) * (M_PI / 180.0f)) / std::tan(M_PI * 0.25);
  const float fovScalarY = std::tan((m_eyeFOVY / 2.0f) * (M_PI / 180.0f)) / std::tan(M_PI * 0.25);
  localVsData[16+0] = fovScalarX;
  localVsData[16+1] = fovScalarY;
  
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

constexpr int textureWidth = 4096;
bool D3D11TextureHelper::UpdateBackbufferSizes() {
  if (!EnsureInitialized())
    return false;

  if (!render_state_.source_.source_texture_ &&
      !render_state_.overlay_.source_texture_) {
    return false;
  }
      
  
  /* if (force_viewport_) {
    target_size_ = gfx::Size(textureWidth, textureWidth);
    return true;
  } */

  /* if (render_state_.source_.source_texture_ &&
      render_state_.overlay_.source_texture_) { */
    target_left_ = gfx::RectF(0 /*x*/, 0 /*y*/, 0.5f /*width*/, 1 /*height*/);
    target_right_ =
        gfx::RectF(0.5f /*x*/, 0 /*y*/, 0.5f /*width*/, 1 /*height*/);
    target_size_ = gfx::Size(textureWidth, textureWidth);
    return true;
  // }

  /* LayerData* layer = render_state_.overlay_.source_texture_
                         ? &render_state_.overlay_
                         : &render_state_.source_;
  D3D11_TEXTURE2D_DESC desc_desired;
  layer->source_texture_->GetDesc(&desc_desired);
  target_left_ = layer->left_;
  target_right_ = layer->right_;
  target_size_ = gfx::Size(desc_desired.Width, desc_desired.Height);
  return true; */
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
  
  desc_desired.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;

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

HANDLE D3D11TextureHelper::GetSharedHandle() {
  IDXGIResource *dxgiResource;
  HRESULT hr = render_state_.target_texture_->QueryInterface(__uuidof(IDXGIResource), (void **)&dxgiResource);
  if (SUCCEEDED(hr)) {
    HANDLE handle = nullptr;
    dxgiResource->GetSharedHandle(&handle);
    dxgiResource->Release();
    return handle;
  } else {
    return nullptr;
  }
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
