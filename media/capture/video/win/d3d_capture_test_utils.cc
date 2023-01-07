// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/d3d_capture_test_utils.h"

namespace media {

MockD3D11DeviceContext::MockD3D11DeviceContext() = default;
MockD3D11DeviceContext::~MockD3D11DeviceContext() = default;

IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSSetConstantBuffers(
    UINT start_slot,
    UINT num_buffers,
    ID3D11Buffer* const* constant_buffers_out) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSSetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView* const* shader_resource_views_out) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSSetShader(ID3D11PixelShader* pixel_shader,
                                    ID3D11ClassInstance* const* class_instances,
                                    UINT num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSSetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState* const* samplers_out) {
}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSSetShader(ID3D11VertexShader* vertex_shader,
                                    ID3D11ClassInstance* const* class_instances,
                                    UINT num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DrawIndexed(UINT index_count,
                                    UINT start_index_location,
                                    INT base_vertex_location) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::Draw(UINT vertex_count, UINT start_vertex_location) {}
IFACEMETHODIMP MockD3D11DeviceContext::Map(
    ID3D11Resource* resource,
    UINT subresource,
    D3D11_MAP MapType,
    UINT MapFlags,
    D3D11_MAPPED_SUBRESOURCE* mapped_resource) {
  return OnMap(resource, subresource, MapType, MapFlags, mapped_resource);
}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::Unmap(ID3D11Resource* resource, UINT subresource) {
  OnUnmap(resource, subresource);
}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSSetConstantBuffers(
    UINT start_slot,
    UINT num_buffers,
    ID3D11Buffer* const* constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IASetInputLayout(ID3D11InputLayout* input_layout) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IASetVertexBuffers(UINT start_slot,
                                           UINT num_buffers,
                                           ID3D11Buffer* const* vertex_buffers,
                                           const UINT* strides,
                                           const UINT* offsets) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IASetIndexBuffer(ID3D11Buffer* index_buffer,
                                         DXGI_FORMAT format,
                                         UINT offset) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DrawIndexedInstanced(UINT index_count_per_instance,
                                             UINT instance_count,
                                             UINT start_index_location,
                                             INT base_vertex_location,
                                             UINT start_instance_location) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DrawInstanced(UINT vertex_count_per_instance,
                                      UINT instance_count,
                                      UINT start_vertex_location,
                                      UINT start_instance_location) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSSetConstantBuffers(
    UINT start_slot,
    UINT num_buffers,
    ID3D11Buffer* const* constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSSetShader(ID3D11GeometryShader* shader,
                                    ID3D11ClassInstance* const* class_instances,
                                    UINT num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IASetPrimitiveTopology(
    D3D11_PRIMITIVE_TOPOLOGY topology) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSSetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView* const* shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSSetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState* const* samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::Begin(ID3D11Asynchronous* async) {}
IFACEMETHODIMP_(void) MockD3D11DeviceContext::End(ID3D11Asynchronous* async) {}
IFACEMETHODIMP MockD3D11DeviceContext::GetData(ID3D11Asynchronous* async,
                                               void* data,
                                               UINT data_size,
                                               UINT get_data_flags) {
  return E_NOTIMPL;
}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::SetPredication(ID3D11Predicate* pPredicate,
                                       BOOL PredicateValue) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSSetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView* const* shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSSetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState* const* samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMSetRenderTargets(
    UINT num_views,
    ID3D11RenderTargetView* const* render_target_views,
    ID3D11DepthStencilView* depth_stencil_view) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
    UINT num_rtvs,
    ID3D11RenderTargetView* const* render_target_views,
    ID3D11DepthStencilView* depth_stencil_view,
    UINT uav_start_slot,
    UINT num_uavs,
    ID3D11UnorderedAccessView* const* unordered_access_views,
    const UINT* uav_initial_counts) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMSetBlendState(ID3D11BlendState* blend_state,
                                        const FLOAT blend_factor[4],
                                        UINT sample_mask) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMSetDepthStencilState(
    ID3D11DepthStencilState* depth_stencil_state,
    UINT stencil_ref) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::SOSetTargets(UINT num_buffers,
                                     ID3D11Buffer* const* so_targets,
                                     const UINT* offsets) {}
IFACEMETHODIMP_(void) MockD3D11DeviceContext::DrawAuto() {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DrawIndexedInstancedIndirect(
    ID3D11Buffer* buffer_for_args,
    UINT aligned_byte_offset_for_args) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DrawInstancedIndirect(
    ID3D11Buffer* buffer_for_args,
    UINT aligned_byte_offset_for_args) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::Dispatch(UINT thread_group_count_x,
                                 UINT thread_group_count_y,
                                 UINT thread_group_count_z) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DispatchIndirect(ID3D11Buffer* buffer_for_args,
                                         UINT aligned_byte_offset_for_args) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::RSSetState(ID3D11RasterizerState* rasterizer_state) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::RSSetViewports(UINT num_viewports,
                                       const D3D11_VIEWPORT* viewports) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::RSSetScissorRects(UINT num_rects,
                                          const D3D11_RECT* rects) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CopySubresourceRegion(ID3D11Resource* dest_resource,
                                              UINT dest_subresource,
                                              UINT dest_x,
                                              UINT dest_y,
                                              UINT dest_z,
                                              ID3D11Resource* source_resource,
                                              UINT source_subresource,
                                              const D3D11_BOX* source_box) {
  OnCopySubresourceRegion(dest_resource, dest_subresource, dest_x, dest_y,
                          dest_z, source_resource, source_subresource,
                          source_box);
}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CopyResource(ID3D11Resource* dest_resource,
                                     ID3D11Resource* source_resource) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::UpdateSubresource(ID3D11Resource* dest_resource,
                                          UINT dest_subresource,
                                          const D3D11_BOX* dest_box,
                                          const void* source_data,
                                          UINT source_row_pitch,
                                          UINT source_depth_pitch) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CopyStructureCount(
    ID3D11Buffer* dest_buffer,
    UINT dest_aligned_byte_offset,
    ID3D11UnorderedAccessView* source_view) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::ClearRenderTargetView(
    ID3D11RenderTargetView* render_target_view,
    const FLOAT color_rgba[4]) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::ClearUnorderedAccessViewUint(
    ID3D11UnorderedAccessView* unordered_access_view,
    const UINT values[4]) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::ClearUnorderedAccessViewFloat(
    ID3D11UnorderedAccessView* unordered_access_view,
    const FLOAT values[4]) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::ClearDepthStencilView(
    ID3D11DepthStencilView* depth_stencil_view,
    UINT clear_flags,
    FLOAT depth,
    UINT8 stencil) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GenerateMips(
    ID3D11ShaderResourceView* shader_resource_view) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::SetResourceMinLOD(ID3D11Resource* resource,
                                          FLOAT min_lod) {}
IFACEMETHODIMP_(FLOAT)
MockD3D11DeviceContext::GetResourceMinLOD(ID3D11Resource* resource) {
  return 0;
}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::ResolveSubresource(ID3D11Resource* dest_resource,
                                           UINT dest_subresource,
                                           ID3D11Resource* source_resource,
                                           UINT source_subresource,
                                           DXGI_FORMAT format) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::ExecuteCommandList(ID3D11CommandList* command_list,
                                           BOOL restore_context_state) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSSetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView* const* shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSSetShader(ID3D11HullShader* hull_shader,
                                    ID3D11ClassInstance* const* class_instances,
                                    UINT num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSSetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState* const* samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSSetConstantBuffers(
    UINT start_slot,
    UINT num_buffers,
    ID3D11Buffer* const* constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSSetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView* const* shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSSetShader(ID3D11DomainShader* domain_shader,
                                    ID3D11ClassInstance* const* class_instances,
                                    UINT num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSSetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState* const* samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSSetConstantBuffers(
    UINT start_slot,
    UINT num_buffers,
    ID3D11Buffer* const* constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSSetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView* const* shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSSetUnorderedAccessViews(
    UINT start_slot,
    UINT num_uavs,
    ID3D11UnorderedAccessView* const* unordered_access_views,
    const UINT* uav_initial_counts) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSSetShader(ID3D11ComputeShader* computer_shader,
                                    ID3D11ClassInstance* const* class_instances,
                                    UINT num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSSetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState* const* samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSSetConstantBuffers(
    UINT start_slot,
    UINT num_buffers,
    ID3D11Buffer* const* constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSGetConstantBuffers(UINT start_slot,
                                             UINT num_buffers,
                                             ID3D11Buffer** constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSGetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView** shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSGetShader(ID3D11PixelShader** pixel_shader,
                                    ID3D11ClassInstance** class_instances,
                                    UINT* num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSGetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState** samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSGetShader(ID3D11VertexShader** vertex_shader,
                                    ID3D11ClassInstance** class_instances,
                                    UINT* num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::PSGetConstantBuffers(UINT start_slot,
                                             UINT num_buffers,
                                             ID3D11Buffer** constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout** input_layout) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IAGetVertexBuffers(UINT start_slot,
                                           UINT num_buffers,
                                           ID3D11Buffer** vertex_buffers,
                                           UINT* strides,
                                           UINT* offsets) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IAGetIndexBuffer(ID3D11Buffer** index_buffer,
                                         DXGI_FORMAT* format,
                                         UINT* offset) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSGetConstantBuffers(UINT start_slot,
                                             UINT num_buffers,
                                             ID3D11Buffer** constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSGetShader(ID3D11GeometryShader** geometry_shader,
                                    ID3D11ClassInstance** class_instances,
                                    UINT* num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::IAGetPrimitiveTopology(
    D3D11_PRIMITIVE_TOPOLOGY* topology) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSGetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView** shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::VSGetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState** samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GetPredication(ID3D11Predicate** predicate,
                                       BOOL* predicate_value) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSGetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView** shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GSGetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState** samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMGetRenderTargets(
    UINT num_views,
    ID3D11RenderTargetView** render_target_views,
    ID3D11DepthStencilView** depth_stencil_view) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(
    UINT num_rtvs,
    ID3D11RenderTargetView** render_target_views,
    ID3D11DepthStencilView** depth_stencil_view,
    UINT uav_start_slot,
    UINT num_uavs,
    ID3D11UnorderedAccessView** unordered_access_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMGetBlendState(ID3D11BlendState** blend_state,
                                        FLOAT blend_factor[4],
                                        UINT* sample_mask) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::OMGetDepthStencilState(
    ID3D11DepthStencilState** depth_stencil_state,
    UINT* stencil_ref) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::SOGetTargets(UINT num_buffers,
                                     ID3D11Buffer** so_targets) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::RSGetState(ID3D11RasterizerState** rasterizer_state) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::RSGetViewports(UINT* num_viewports,
                                       D3D11_VIEWPORT* viewports) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::RSGetScissorRects(UINT* num_rects, D3D11_RECT* rects) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSGetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView** shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSGetShader(ID3D11HullShader** hull_shader,
                                    ID3D11ClassInstance** class_instances,
                                    UINT* num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSGetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState** samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::HSGetConstantBuffers(UINT start_slot,
                                             UINT num_buffers,
                                             ID3D11Buffer** constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSGetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView** shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSGetShader(ID3D11DomainShader** domain_shader,
                                    ID3D11ClassInstance** class_instances,
                                    UINT* num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSGetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState** samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::DSGetConstantBuffers(UINT start_slot,
                                             UINT num_buffers,
                                             ID3D11Buffer** constant_buffers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSGetShaderResources(
    UINT start_slot,
    UINT num_views,
    ID3D11ShaderResourceView** shader_resource_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSGetUnorderedAccessViews(
    UINT start_slot,
    UINT num_uavs,
    ID3D11UnorderedAccessView** unordered_access_views) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSGetShader(ID3D11ComputeShader** pcomputer_shader,
                                    ID3D11ClassInstance** class_instances,
                                    UINT* num_class_instances) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSGetSamplers(UINT start_slot,
                                      UINT num_samplers,
                                      ID3D11SamplerState** samplers) {}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::CSGetConstantBuffers(UINT start_slot,
                                             UINT num_buffers,
                                             ID3D11Buffer** constant_buffers) {}
IFACEMETHODIMP_(void) MockD3D11DeviceContext::ClearState() {}
IFACEMETHODIMP_(void) MockD3D11DeviceContext::Flush() {}
IFACEMETHODIMP_(D3D11_DEVICE_CONTEXT_TYPE) MockD3D11DeviceContext::GetType() {
  return D3D11_DEVICE_CONTEXT_IMMEDIATE;
}
IFACEMETHODIMP_(UINT) MockD3D11DeviceContext::GetContextFlags() {
  return 0;
}
IFACEMETHODIMP MockD3D11DeviceContext::FinishCommandList(
    BOOL restore_deferred_context_state,
    ID3D11CommandList** command_list) {
  return E_NOTIMPL;
}
IFACEMETHODIMP_(void)
MockD3D11DeviceContext::GetDevice(ID3D11Device** device) {}
IFACEMETHODIMP MockD3D11DeviceContext::GetPrivateData(REFGUID guid,
                                                      UINT* data_size,
                                                      void* data) {
  return E_NOTIMPL;
}
IFACEMETHODIMP MockD3D11DeviceContext::SetPrivateData(REFGUID guid,
                                                      UINT data_size,
                                                      const void* data) {
  return E_NOTIMPL;
}
IFACEMETHODIMP MockD3D11DeviceContext::SetPrivateDataInterface(
    REFGUID guid,
    const IUnknown* data) {
  return E_NOTIMPL;
}

MockD3D11Device::MockD3D11Device()
    : mock_immediate_context_(new MockD3D11DeviceContext()) {}
MockD3D11Device::~MockD3D11Device() {}

IFACEMETHODIMP MockD3D11Device::CreateBuffer(
    const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data,
    ID3D11Buffer** ppBuffer) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateTexture1D(
    const D3D11_TEXTURE1D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data,
    ID3D11Texture1D** texture1D) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateTexture2D(
    const D3D11_TEXTURE2D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data,
    ID3D11Texture2D** texture2D) {
  Microsoft::WRL::ComPtr<MockD3D11Texture2D> mock_texture(
      new MockD3D11Texture2D());
  HRESULT hr = mock_texture.CopyTo(IID_PPV_ARGS(texture2D));
  OnCreateTexture2D(desc, initial_data, texture2D);
  return hr;
}

IFACEMETHODIMP MockD3D11Device::CreateTexture3D(
    const D3D11_TEXTURE3D_DESC* desc,
    const D3D11_SUBRESOURCE_DATA* initial_data,
    ID3D11Texture3D** texture2D) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateShaderResourceView(
    ID3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
    ID3D11ShaderResourceView** srv) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateUnorderedAccessView(
    ID3D11Resource* resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc,
    ID3D11UnorderedAccessView** uaview) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateRenderTargetView(
    ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc,
    ID3D11RenderTargetView** rtv) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateDepthStencilView(
    ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* desc,
    ID3D11DepthStencilView** depth_stencil_view) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC* input_element_descs,
    UINT num_elements,
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    ID3D11InputLayout** input_layout) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateVertexShader(
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    ID3D11ClassLinkage* class_linkage,
    ID3D11VertexShader** vertex_shader) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateGeometryShader(
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    ID3D11ClassLinkage* class_linkage,
    ID3D11GeometryShader** geometry_shader) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateGeometryShaderWithStreamOutput(
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    const D3D11_SO_DECLARATION_ENTRY* so_declaration,
    UINT num_entries,
    const UINT* buffer_strides,
    UINT num_strides,
    UINT rasterized_stream,
    ID3D11ClassLinkage* class_linkage,
    ID3D11GeometryShader** geometry_shader) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreatePixelShader(
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    ID3D11ClassLinkage* class_linkage,
    ID3D11PixelShader** pixel_shader) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateHullShader(
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    ID3D11ClassLinkage* class_linkage,
    ID3D11HullShader** hull_shader) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateDomainShader(
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    ID3D11ClassLinkage* class_linkage,
    ID3D11DomainShader** domain_shader) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateComputeShader(
    const void* shader_bytecode,
    SIZE_T bytecode_length,
    ID3D11ClassLinkage* class_linkage,
    ID3D11ComputeShader** compute_shader) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateClassLinkage(
    ID3D11ClassLinkage** linkage) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateBlendState(
    const D3D11_BLEND_DESC* blend_state_desc,
    ID3D11BlendState** blend_state) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC* depth_stencil_desc,
    ID3D11DepthStencilState** depth_stencil_state) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateRasterizerState(
    const D3D11_RASTERIZER_DESC* rasterizer_desc,
    ID3D11RasterizerState** rasterizer_state) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateSamplerState(
    const D3D11_SAMPLER_DESC* sampler_desc,
    ID3D11SamplerState** sampler_state) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateQuery(const D3D11_QUERY_DESC* query_desc,
                                            ID3D11Query** query) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreatePredicate(
    const D3D11_QUERY_DESC* predicate_desc,
    ID3D11Predicate** predicate) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateCounter(
    const D3D11_COUNTER_DESC* counter_desc,
    ID3D11Counter** counter) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateDeferredContext(
    UINT context_flags,
    ID3D11DeviceContext** deferred_context) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::OpenSharedResource(HANDLE resource,
                                                   REFIID returned_interface,
                                                   void** resource_out) {
  return DoOpenSharedResource1(resource, returned_interface, resource_out);
}

IFACEMETHODIMP MockD3D11Device::CheckFormatSupport(DXGI_FORMAT format,
                                                   UINT* format_support) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CheckMultisampleQualityLevels(
    DXGI_FORMAT format,
    UINT sample_count,
    UINT* num_quality_levels) {
  return E_NOTIMPL;
}

IFACEMETHODIMP_(void)
MockD3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO* counter_info) {}

IFACEMETHODIMP MockD3D11Device::CheckCounter(const D3D11_COUNTER_DESC* desc,
                                             D3D11_COUNTER_TYPE* type,
                                             UINT* active_counters,
                                             LPSTR name,
                                             UINT* name_length,
                                             LPSTR units,
                                             UINT* units_length,
                                             LPSTR description,
                                             UINT* description_length) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CheckFeatureSupport(
    D3D11_FEATURE feature,
    void* feature_support_data,
    UINT feature_support_data_size) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::GetPrivateData(REFGUID guid,
                                               UINT* data_size,
                                               void* data) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::SetPrivateData(REFGUID guid,
                                               UINT data_size,
                                               const void* data) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::SetPrivateDataInterface(REFGUID guid,
                                                        const IUnknown* data) {
  return E_NOTIMPL;
}

IFACEMETHODIMP_(D3D_FEATURE_LEVEL) MockD3D11Device::GetFeatureLevel() {
  return D3D_FEATURE_LEVEL_11_1;
}

IFACEMETHODIMP_(UINT) MockD3D11Device::GetCreationFlags() {
  return 0;
}

IFACEMETHODIMP MockD3D11Device::GetDeviceRemovedReason() {
  return OnGetDeviceRemovedReason();
}

IFACEMETHODIMP_(void)
MockD3D11Device::GetImmediateContext(ID3D11DeviceContext** immediate_context) {
  mock_immediate_context_.CopyTo(immediate_context);
}

IFACEMETHODIMP MockD3D11Device::SetExceptionMode(UINT raise_flags) {
  return E_NOTIMPL;
}

IFACEMETHODIMP_(UINT) MockD3D11Device::GetExceptionMode() {
  return 0;
}

IFACEMETHODIMP_(void)
MockD3D11Device::GetImmediateContext1(
    ID3D11DeviceContext1** immediate_context) {}

IFACEMETHODIMP MockD3D11Device::CreateDeferredContext1(
    UINT context_flags,
    ID3D11DeviceContext1** deferred_context) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateBlendState1(
    const D3D11_BLEND_DESC1* blend_state_desc,
    ID3D11BlendState1** blend_state) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateRasterizerState1(
    const D3D11_RASTERIZER_DESC1* rasterizer_desc,
    ID3D11RasterizerState1** rasterizer_state) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::CreateDeviceContextState(
    UINT flags,
    const D3D_FEATURE_LEVEL* feature_levels,
    UINT feature_level_count,
    UINT sdk_version,
    REFIID emulated_interface,
    D3D_FEATURE_LEVEL* chosen_feature_level,
    ID3DDeviceContextState** context_state) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockD3D11Device::OpenSharedResource1(HANDLE resource,
                                                    REFIID returned_interface,
                                                    void** resource_out) {
  return DoOpenSharedResource1(resource, returned_interface, resource_out);
}

IFACEMETHODIMP MockD3D11Device::OpenSharedResourceByName(
    LPCWSTR name,
    DWORD desired_access,
    REFIID returned_interface,
    void** resource_out) {
  return E_NOTIMPL;
}

// Setup default actions for mocked methods
void MockD3D11Device::SetupDefaultMocks() {
  ON_CALL(*this, OnGetDeviceRemovedReason).WillByDefault([]() { return S_OK; });
  ON_CALL(*this, DoOpenSharedResource1)
      .WillByDefault([](HANDLE, REFIID, void**) { return E_NOTIMPL; });
  ON_CALL(*mock_immediate_context_.Get(), OnMap)
      .WillByDefault([](ID3D11Resource*, UINT, D3D11_MAP, UINT,
                        D3D11_MAPPED_SUBRESOURCE*) { return E_NOTIMPL; });
  ON_CALL(*this, OnCreateTexture2D)
      .WillByDefault([](const D3D11_TEXTURE2D_DESC*,
                        const D3D11_SUBRESOURCE_DATA*,
                        ID3D11Texture2D** texture) {
        static_cast<MockD3D11Texture2D*>(*texture)->SetupDefaultMocks();
        return S_OK;
      });
}

IFACEMETHODIMP MockDXGIResource::CreateSubresourceSurface(
    UINT index,
    IDXGISurface2** surface) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::CreateSharedHandle(
    const SECURITY_ATTRIBUTES* attributes,
    DWORD access,
    LPCWSTR name,
    HANDLE* handle) {
  // Need to provide a real handle to client, so create an event handle
  *handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  return S_OK;
}

IFACEMETHODIMP MockDXGIResource::GetSharedHandle(HANDLE* shared_handle) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::GetUsage(DXGI_USAGE* usage) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::SetEvictionPriority(UINT eviction_priority) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::GetEvictionPriority(UINT* eviction_priority) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::GetDevice(REFIID riid, void** device) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::SetPrivateData(REFGUID name,
                                                UINT data_size,
                                                const void* data) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::SetPrivateDataInterface(
    REFGUID name,
    const IUnknown* unknown) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::GetPrivateData(REFGUID name,
                                                UINT* data_size,
                                                void* data) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::GetParent(REFIID riid, void** parent) {
  return E_NOTIMPL;
}

IFACEMETHODIMP MockDXGIResource::AcquireSync(UINT64 key, DWORD milliseconds) {
  return S_OK;
}
IFACEMETHODIMP MockDXGIResource::ReleaseSync(UINT64 key) {
  return S_OK;
}

MockDXGIResource::~MockDXGIResource() {}

MockD3D11Texture2D::MockD3D11Texture2D(D3D11_TEXTURE2D_DESC desc,
                                       ID3D11Device* device)
    : desc_(desc), device_(device) {}
MockD3D11Texture2D::MockD3D11Texture2D() {}

IFACEMETHODIMP MockD3D11Texture2D::QueryInterface(REFIID riid, void** object) {
  if (riid == __uuidof(IDXGIResource1) || riid == __uuidof(IDXGIKeyedMutex)) {
    if (!mock_resource_) {
      mock_resource_ = new MockDXGIResource();
    }
    return mock_resource_.CopyTo(riid, object);
  }
  return MockInterface::QueryInterface(riid, object);
}

IFACEMETHODIMP_(void) MockD3D11Texture2D::GetDesc(D3D11_TEXTURE2D_DESC* desc) {
  *desc = desc_;
}
IFACEMETHODIMP_(void)
MockD3D11Texture2D::GetType(D3D11_RESOURCE_DIMENSION* resource_dimension) {}
IFACEMETHODIMP_(void)
MockD3D11Texture2D::SetEvictionPriority(UINT eviction_priority) {}
IFACEMETHODIMP_(UINT) MockD3D11Texture2D::GetEvictionPriority() {
  return 0;
}
IFACEMETHODIMP_(void) MockD3D11Texture2D::GetDevice(ID3D11Device** device) {
  if (device_) {
    device_.CopyTo(device);
  }
}
IFACEMETHODIMP MockD3D11Texture2D::GetPrivateData(REFGUID guid,
                                                  UINT* data_size,
                                                  void* data) {
  return E_NOTIMPL;
}
IFACEMETHODIMP MockD3D11Texture2D::SetPrivateDataInterface(
    REFGUID guid,
    const IUnknown* data) {
  return E_NOTIMPL;
}

void MockD3D11Texture2D::SetupDefaultMocks() {
  ON_CALL(*this, SetPrivateData(testing::_, testing::_, testing::_))
      .WillByDefault(testing::Return(E_NOTIMPL));
  ON_CALL(*this,
          SetPrivateData(WKPDID_D3DDebugObjectName, testing::_, testing::_))
      .WillByDefault(testing::Return(S_OK));
}

MockD3D11Texture2D::~MockD3D11Texture2D() {}

}  // namespace media
