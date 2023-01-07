// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_D3D_CAPTURE_TEST_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_WIN_D3D_CAPTURE_TEST_UTILS_H_

#include <d3d11_4.h>
#include <wrl.h>
#include "base/memory/ref_counted.h"
#include "media/base/win/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

template <class... Interface>
class MockInterface
    : public base::RefCountedThreadSafe<MockInterface<Interface...>> {
 public:
  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) {
    if (riid == __uuidof(IUnknown)) {
      this->AddRef();
      *object = this;
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  IFACEMETHODIMP_(ULONG) AddRef() {
    base::RefCountedThreadSafe<MockInterface<Interface...>>::AddRef();
    return 1U;
  }
  IFACEMETHODIMP_(ULONG) Release() {
    base::RefCountedThreadSafe<MockInterface<Interface...>>::Release();
    return 1U;
  }

 protected:
  friend class base::RefCountedThreadSafe<MockInterface<Interface...>>;
  virtual ~MockInterface() = default;
};

template <class Interface, class... Interfaces>
class MockInterface<Interface, Interfaces...>
    : public MockInterface<Interfaces...>, public Interface {
 public:
  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (riid == __uuidof(Interface)) {
      this->AddRef();
      *object = static_cast<Interface*>(this);
      return S_OK;
    }
    return MockInterface<Interfaces...>::QueryInterface(riid, object);
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    return MockInterface<Interfaces...>::AddRef();
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    return MockInterface<Interfaces...>::Release();
  }
};

class MockD3D11DeviceContext final : public MockInterface<ID3D11DeviceContext> {
 public:
  MockD3D11DeviceContext();

  // ID3D11DeviceContext
  IFACEMETHODIMP_(void)
  VSSetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer* const* constant_buffers_out) override;
  IFACEMETHODIMP_(void)
  PSSetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView* const* shader_resource_views_out) override;
  IFACEMETHODIMP_(void)
  PSSetShader(ID3D11PixelShader* pixel_shader,
              ID3D11ClassInstance* const* class_instances,
              UINT num_class_instances) override;
  IFACEMETHODIMP_(void)
  PSSetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState* const* samplers_out) override;
  IFACEMETHODIMP_(void)
  VSSetShader(ID3D11VertexShader* vertex_shader,
              ID3D11ClassInstance* const* class_instances,
              UINT num_class_instances) override;
  IFACEMETHODIMP_(void)
  DrawIndexed(UINT index_count,
              UINT start_index_location,
              INT base_vertex_location) override;
  IFACEMETHODIMP_(void)
  Draw(UINT vertex_count, UINT start_vertex_location) override;
  MOCK_METHOD5(OnMap,
               HRESULT(ID3D11Resource*,
                       UINT,
                       D3D11_MAP,
                       UINT,
                       D3D11_MAPPED_SUBRESOURCE*));
  IFACEMETHODIMP Map(ID3D11Resource* resource,
                     UINT subresource,
                     D3D11_MAP MapType,
                     UINT MapFlags,
                     D3D11_MAPPED_SUBRESOURCE* mapped_resource) override;
  MOCK_METHOD2(OnUnmap, void(ID3D11Resource*, UINT));
  IFACEMETHODIMP_(void)
  Unmap(ID3D11Resource* resource, UINT subresource) override;
  IFACEMETHODIMP_(void)
  PSSetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer* const* constant_buffers) override;
  IFACEMETHODIMP_(void)
  IASetInputLayout(ID3D11InputLayout* input_layout) override;
  IFACEMETHODIMP_(void)
  IASetVertexBuffers(UINT start_slot,
                     UINT num_buffers,
                     ID3D11Buffer* const* vertex_buffers,
                     const UINT* strides,
                     const UINT* offsets) override;
  IFACEMETHODIMP_(void)
  IASetIndexBuffer(ID3D11Buffer* index_buffer,
                   DXGI_FORMAT format,
                   UINT offset) override;
  IFACEMETHODIMP_(void)
  DrawIndexedInstanced(UINT index_count_per_instance,
                       UINT instance_count,
                       UINT start_index_location,
                       INT base_vertex_location,
                       UINT start_instance_location) override;
  IFACEMETHODIMP_(void)
  DrawInstanced(UINT vertex_count_per_instance,
                UINT instance_count,
                UINT start_vertex_location,
                UINT start_instance_location) override;
  IFACEMETHODIMP_(void)
  GSSetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer* const* constant_buffers) override;
  IFACEMETHODIMP_(void)
  GSSetShader(ID3D11GeometryShader* shader,
              ID3D11ClassInstance* const* class_instances,
              UINT num_class_instances) override;
  IFACEMETHODIMP_(void)
  IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology) override;
  IFACEMETHODIMP_(void)
  VSSetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView* const* shader_resource_views) override;
  IFACEMETHODIMP_(void)
  VSSetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState* const* samplers) override;
  IFACEMETHODIMP_(void) Begin(ID3D11Asynchronous* async) override;
  IFACEMETHODIMP_(void) End(ID3D11Asynchronous* async) override;
  IFACEMETHODIMP GetData(ID3D11Asynchronous* async,
                         void* data,
                         UINT data_size,
                         UINT get_data_flags) override;
  IFACEMETHODIMP_(void)
  SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue) override;
  IFACEMETHODIMP_(void)
  GSSetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView* const* shader_resource_views) override;
  IFACEMETHODIMP_(void)
  GSSetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState* const* samplers) override;
  IFACEMETHODIMP_(void)
  OMSetRenderTargets(UINT num_views,
                     ID3D11RenderTargetView* const* render_target_views,
                     ID3D11DepthStencilView* depth_stencil_view) override;
  IFACEMETHODIMP_(void)
  OMSetRenderTargetsAndUnorderedAccessViews(
      UINT num_rtvs,
      ID3D11RenderTargetView* const* render_target_views,
      ID3D11DepthStencilView* depth_stencil_view,
      UINT uav_start_slot,
      UINT num_uavs,
      ID3D11UnorderedAccessView* const* unordered_access_views,
      const UINT* uav_initial_counts) override;
  IFACEMETHODIMP_(void)
  OMSetBlendState(ID3D11BlendState* blend_state,
                  const FLOAT blend_factor[4],
                  UINT sample_mask) override;
  IFACEMETHODIMP_(void)
  OMSetDepthStencilState(ID3D11DepthStencilState* depth_stencil_state,
                         UINT stencil_ref) override;
  IFACEMETHODIMP_(void)
  SOSetTargets(UINT num_buffers,
               ID3D11Buffer* const* so_targets,
               const UINT* offsets) override;
  IFACEMETHODIMP_(void) DrawAuto() override;
  IFACEMETHODIMP_(void)
  DrawIndexedInstancedIndirect(ID3D11Buffer* buffer_for_args,
                               UINT aligned_byte_offset_for_args) override;
  IFACEMETHODIMP_(void)
  DrawInstancedIndirect(ID3D11Buffer* buffer_for_args,
                        UINT aligned_byte_offset_for_args) override;
  IFACEMETHODIMP_(void)
  Dispatch(UINT thread_group_count_x,
           UINT thread_group_count_y,
           UINT thread_group_count_z) override;
  IFACEMETHODIMP_(void)
  DispatchIndirect(ID3D11Buffer* buffer_for_args,
                   UINT aligned_byte_offset_for_args) override;
  IFACEMETHODIMP_(void)
  RSSetState(ID3D11RasterizerState* rasterizer_state) override;
  IFACEMETHODIMP_(void)
  RSSetViewports(UINT num_viewports, const D3D11_VIEWPORT* viewports) override;
  IFACEMETHODIMP_(void)
  RSSetScissorRects(UINT num_rects, const D3D11_RECT* rects) override;
  IFACEMETHODIMP_(void)
  CopySubresourceRegion(ID3D11Resource* dest_resource,
                        UINT dest_subresource,
                        UINT dest_x,
                        UINT dest_y,
                        UINT dest_z,
                        ID3D11Resource* source_resource,
                        UINT source_subresource,
                        const D3D11_BOX* source_box) override;
  MOCK_METHOD8(OnCopySubresourceRegion,
               void(ID3D11Resource*,
                    UINT,
                    UINT,
                    UINT,
                    UINT,
                    ID3D11Resource*,
                    UINT,
                    const D3D11_BOX*));
  IFACEMETHODIMP_(void)
  CopyResource(ID3D11Resource* dest_resource,
               ID3D11Resource* source_resource) override;
  IFACEMETHODIMP_(void)
  UpdateSubresource(ID3D11Resource* dest_resource,
                    UINT dest_subresource,
                    const D3D11_BOX* dest_box,
                    const void* source_data,
                    UINT source_row_pitch,
                    UINT source_depth_pitch) override;
  IFACEMETHODIMP_(void)
  CopyStructureCount(ID3D11Buffer* dest_buffer,
                     UINT dest_aligned_byte_offset,
                     ID3D11UnorderedAccessView* source_view) override;
  IFACEMETHODIMP_(void)
  ClearRenderTargetView(ID3D11RenderTargetView* render_target_view,
                        const FLOAT color_rgba[4]) override;
  IFACEMETHODIMP_(void)
  ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* unordered_access_view,
                               const UINT values[4]) override;
  IFACEMETHODIMP_(void)
  ClearUnorderedAccessViewFloat(
      ID3D11UnorderedAccessView* unordered_access_view,
      const FLOAT values[4]) override;
  IFACEMETHODIMP_(void)
  ClearDepthStencilView(ID3D11DepthStencilView* depth_stencil_view,
                        UINT clear_flags,
                        FLOAT depth,
                        UINT8 stencil) override;
  IFACEMETHODIMP_(void)
  GenerateMips(ID3D11ShaderResourceView* shader_resource_view) override;
  IFACEMETHODIMP_(void)
  SetResourceMinLOD(ID3D11Resource* resource, FLOAT min_lod) override;
  IFACEMETHODIMP_(FLOAT) GetResourceMinLOD(ID3D11Resource* resource) override;
  IFACEMETHODIMP_(void)
  ResolveSubresource(ID3D11Resource* dest_resource,
                     UINT dest_subresource,
                     ID3D11Resource* source_resource,
                     UINT source_subresource,
                     DXGI_FORMAT format) override;
  IFACEMETHODIMP_(void)
  ExecuteCommandList(ID3D11CommandList* command_list,
                     BOOL restore_context_state) override;
  IFACEMETHODIMP_(void)
  HSSetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView* const* shader_resource_views) override;
  IFACEMETHODIMP_(void)
  HSSetShader(ID3D11HullShader* hull_shader,
              ID3D11ClassInstance* const* class_instances,
              UINT num_class_instances) override;
  IFACEMETHODIMP_(void)
  HSSetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState* const* samplers) override;
  IFACEMETHODIMP_(void)
  HSSetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer* const* constant_buffers) override;
  IFACEMETHODIMP_(void)
  DSSetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView* const* shader_resource_views) override;
  IFACEMETHODIMP_(void)
  DSSetShader(ID3D11DomainShader* domain_shader,
              ID3D11ClassInstance* const* class_instances,
              UINT num_class_instances) override;
  IFACEMETHODIMP_(void)
  DSSetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState* const* samplers) override;
  IFACEMETHODIMP_(void)
  DSSetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer* const* constant_buffers) override;
  IFACEMETHODIMP_(void)
  CSSetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView* const* shader_resource_views) override;
  IFACEMETHODIMP_(void)
  CSSetUnorderedAccessViews(
      UINT start_slot,
      UINT num_uavs,
      ID3D11UnorderedAccessView* const* unordered_access_views,
      const UINT* uav_initial_counts) override;
  IFACEMETHODIMP_(void)
  CSSetShader(ID3D11ComputeShader* computer_shader,
              ID3D11ClassInstance* const* class_instances,
              UINT num_class_instances) override;
  IFACEMETHODIMP_(void)
  CSSetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState* const* samplers) override;
  IFACEMETHODIMP_(void)
  CSSetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer* const* constant_buffers) override;
  IFACEMETHODIMP_(void)
  VSGetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer** constant_buffers) override;
  IFACEMETHODIMP_(void)
  PSGetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView** shader_resource_views) override;
  IFACEMETHODIMP_(void)
  PSGetShader(ID3D11PixelShader** pixel_shader,
              ID3D11ClassInstance** class_instances,
              UINT* num_class_instances) override;
  IFACEMETHODIMP_(void)
  PSGetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState** samplers) override;
  IFACEMETHODIMP_(void)
  VSGetShader(ID3D11VertexShader** vertex_shader,
              ID3D11ClassInstance** class_instances,
              UINT* num_class_instances) override;
  IFACEMETHODIMP_(void)
  PSGetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer** constant_buffers) override;
  IFACEMETHODIMP_(void)
  IAGetInputLayout(ID3D11InputLayout** input_layout) override;
  IFACEMETHODIMP_(void)
  IAGetVertexBuffers(UINT start_slot,
                     UINT num_buffers,
                     ID3D11Buffer** vertex_buffers,
                     UINT* strides,
                     UINT* offsets) override;
  IFACEMETHODIMP_(void)
  IAGetIndexBuffer(ID3D11Buffer** index_buffer,
                   DXGI_FORMAT* format,
                   UINT* offset) override;
  IFACEMETHODIMP_(void)
  GSGetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer** constant_buffers) override;
  IFACEMETHODIMP_(void)
  GSGetShader(ID3D11GeometryShader** geometry_shader,
              ID3D11ClassInstance** class_instances,
              UINT* num_class_instances) override;
  IFACEMETHODIMP_(void)
  IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* topology) override;
  IFACEMETHODIMP_(void)
  VSGetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView** shader_resource_views) override;
  IFACEMETHODIMP_(void)
  VSGetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState** samplers) override;
  IFACEMETHODIMP_(void)
  GetPredication(ID3D11Predicate** predicate, BOOL* predicate_value) override;
  IFACEMETHODIMP_(void)
  GSGetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView** shader_resource_views) override;
  IFACEMETHODIMP_(void)
  GSGetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState** samplers) override;
  IFACEMETHODIMP_(void)
  OMGetRenderTargets(UINT num_views,
                     ID3D11RenderTargetView** render_target_views,
                     ID3D11DepthStencilView** depth_stencil_view) override;
  IFACEMETHODIMP_(void)
  OMGetRenderTargetsAndUnorderedAccessViews(
      UINT num_rtvs,
      ID3D11RenderTargetView** render_target_views,
      ID3D11DepthStencilView** depth_stencil_view,
      UINT uav_start_slot,
      UINT num_uavs,
      ID3D11UnorderedAccessView** unordered_access_views) override;
  IFACEMETHODIMP_(void)
  OMGetBlendState(ID3D11BlendState** blend_state,
                  FLOAT blend_factor[4],
                  UINT* sample_mask) override;
  IFACEMETHODIMP_(void)
  OMGetDepthStencilState(ID3D11DepthStencilState** depth_stencil_state,
                         UINT* stencil_ref) override;
  IFACEMETHODIMP_(void)
  SOGetTargets(UINT num_buffers, ID3D11Buffer** so_targets) override;
  IFACEMETHODIMP_(void)
  RSGetState(ID3D11RasterizerState** rasterizer_state) override;
  IFACEMETHODIMP_(void)
  RSGetViewports(UINT* num_viewports, D3D11_VIEWPORT* viewports) override;
  IFACEMETHODIMP_(void)
  RSGetScissorRects(UINT* num_rects, D3D11_RECT* rects) override;
  IFACEMETHODIMP_(void)
  HSGetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView** shader_resource_views) override;
  IFACEMETHODIMP_(void)
  HSGetShader(ID3D11HullShader** hull_shader,
              ID3D11ClassInstance** class_instances,
              UINT* num_class_instances) override;
  IFACEMETHODIMP_(void)
  HSGetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState** samplers) override;
  IFACEMETHODIMP_(void)
  HSGetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer** constant_buffers) override;
  IFACEMETHODIMP_(void)
  DSGetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView** shader_resource_views) override;
  IFACEMETHODIMP_(void)
  DSGetShader(ID3D11DomainShader** domain_shader,
              ID3D11ClassInstance** class_instances,
              UINT* num_class_instances) override;
  IFACEMETHODIMP_(void)
  DSGetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState** samplers) override;
  IFACEMETHODIMP_(void)
  DSGetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer** constant_buffers) override;
  IFACEMETHODIMP_(void)
  CSGetShaderResources(
      UINT start_slot,
      UINT num_views,
      ID3D11ShaderResourceView** shader_resource_views) override;
  IFACEMETHODIMP_(void)
  CSGetUnorderedAccessViews(
      UINT start_slot,
      UINT num_uavs,
      ID3D11UnorderedAccessView** unordered_access_views) override;
  IFACEMETHODIMP_(void)
  CSGetShader(ID3D11ComputeShader** pcomputer_shader,
              ID3D11ClassInstance** class_instances,
              UINT* num_class_instances) override;
  IFACEMETHODIMP_(void)
  CSGetSamplers(UINT start_slot,
                UINT num_samplers,
                ID3D11SamplerState** samplers) override;
  IFACEMETHODIMP_(void)
  CSGetConstantBuffers(UINT start_slot,
                       UINT num_buffers,
                       ID3D11Buffer** constant_buffers) override;
  IFACEMETHODIMP_(void) ClearState() override;
  IFACEMETHODIMP_(void) Flush() override;
  IFACEMETHODIMP_(D3D11_DEVICE_CONTEXT_TYPE) GetType() override;
  IFACEMETHODIMP_(UINT) GetContextFlags() override;
  IFACEMETHODIMP FinishCommandList(BOOL restore_deferred_context_state,
                                   ID3D11CommandList** command_list) override;

  // ID3D11DeviceChild
  IFACEMETHODIMP_(void) GetDevice(ID3D11Device** device) override;
  IFACEMETHODIMP GetPrivateData(REFGUID guid,
                                UINT* data_size,
                                void* data) override;
  IFACEMETHODIMP SetPrivateData(REFGUID guid,
                                UINT data_size,
                                const void* data) override;
  IFACEMETHODIMP SetPrivateDataInterface(REFGUID guid,
                                         const IUnknown* data) override;

 private:
  ~MockD3D11DeviceContext() override;
};

class MockD3D11Device final : public MockInterface<ID3D11Device1> {
 public:
  MockD3D11Device();

  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (riid == __uuidof(ID3D11Device)) {
      this->AddRef();
      *object = static_cast<ID3D11Device*>(this);
      return S_OK;
    }
    return MockInterface::QueryInterface(riid, object);
  }

  // ID3D11Device
  IFACEMETHODIMP CreateBuffer(const D3D11_BUFFER_DESC* desc,
                              const D3D11_SUBRESOURCE_DATA* initial_data,
                              ID3D11Buffer** ppBuffer);
  IFACEMETHODIMP CreateTexture1D(const D3D11_TEXTURE1D_DESC* desc,
                                 const D3D11_SUBRESOURCE_DATA* initial_data,
                                 ID3D11Texture1D** texture1D);
  IFACEMETHODIMP CreateTexture2D(const D3D11_TEXTURE2D_DESC* desc,
                                 const D3D11_SUBRESOURCE_DATA* initial_data,
                                 ID3D11Texture2D** texture2D);
  MOCK_METHOD3(OnCreateTexture2D,
               HRESULT(const D3D11_TEXTURE2D_DESC*,
                       const D3D11_SUBRESOURCE_DATA*,
                       ID3D11Texture2D**));
  IFACEMETHODIMP CreateTexture3D(const D3D11_TEXTURE3D_DESC* desc,
                                 const D3D11_SUBRESOURCE_DATA* initial_data,
                                 ID3D11Texture3D** texture2D);
  IFACEMETHODIMP CreateShaderResourceView(
      ID3D11Resource* resource,
      const D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
      ID3D11ShaderResourceView** srv);
  IFACEMETHODIMP CreateUnorderedAccessView(
      ID3D11Resource* resource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc,
      ID3D11UnorderedAccessView** uaview);
  IFACEMETHODIMP CreateRenderTargetView(
      ID3D11Resource* resource,
      const D3D11_RENDER_TARGET_VIEW_DESC* desc,
      ID3D11RenderTargetView** rtv);
  IFACEMETHODIMP CreateDepthStencilView(
      ID3D11Resource* resource,
      const D3D11_DEPTH_STENCIL_VIEW_DESC* desc,
      ID3D11DepthStencilView** depth_stencil_view);
  IFACEMETHODIMP CreateInputLayout(
      const D3D11_INPUT_ELEMENT_DESC* input_element_descs,
      UINT num_elements,
      const void* shader_bytecode,
      SIZE_T bytecode_length,
      ID3D11InputLayout** input_layout);
  IFACEMETHODIMP CreateVertexShader(const void* shader_bytecode,
                                    SIZE_T bytecode_length,
                                    ID3D11ClassLinkage* class_linkage,
                                    ID3D11VertexShader** vertex_shader);
  IFACEMETHODIMP CreateGeometryShader(const void* shader_bytecode,
                                      SIZE_T bytecode_length,
                                      ID3D11ClassLinkage* class_linkage,
                                      ID3D11GeometryShader** geometry_shader);
  IFACEMETHODIMP CreateGeometryShaderWithStreamOutput(
      const void* shader_bytecode,
      SIZE_T bytecode_length,
      const D3D11_SO_DECLARATION_ENTRY* so_declaration,
      UINT num_entries,
      const UINT* buffer_strides,
      UINT num_strides,
      UINT rasterized_stream,
      ID3D11ClassLinkage* class_linkage,
      ID3D11GeometryShader** geometry_shader);
  IFACEMETHODIMP CreatePixelShader(const void* shader_bytecode,
                                   SIZE_T bytecode_length,
                                   ID3D11ClassLinkage* class_linkage,
                                   ID3D11PixelShader** pixel_shader);
  IFACEMETHODIMP CreateHullShader(const void* shader_bytecode,
                                  SIZE_T bytecode_length,
                                  ID3D11ClassLinkage* class_linkage,
                                  ID3D11HullShader** hull_shader);
  IFACEMETHODIMP CreateDomainShader(const void* shader_bytecode,
                                    SIZE_T bytecode_length,
                                    ID3D11ClassLinkage* class_linkage,
                                    ID3D11DomainShader** domain_shader);
  IFACEMETHODIMP CreateComputeShader(const void* shader_bytecode,
                                     SIZE_T bytecode_length,
                                     ID3D11ClassLinkage* class_linkage,
                                     ID3D11ComputeShader** compute_shader);
  IFACEMETHODIMP CreateClassLinkage(ID3D11ClassLinkage** linkage);
  IFACEMETHODIMP CreateBlendState(const D3D11_BLEND_DESC* blend_state_desc,
                                  ID3D11BlendState** blend_state);
  IFACEMETHODIMP CreateDepthStencilState(
      const D3D11_DEPTH_STENCIL_DESC* depth_stencil_desc,
      ID3D11DepthStencilState** depth_stencil_state);
  IFACEMETHODIMP CreateRasterizerState(
      const D3D11_RASTERIZER_DESC* rasterizer_desc,
      ID3D11RasterizerState** rasterizer_state);
  IFACEMETHODIMP CreateSamplerState(const D3D11_SAMPLER_DESC* sampler_desc,
                                    ID3D11SamplerState** sampler_state);
  IFACEMETHODIMP CreateQuery(const D3D11_QUERY_DESC* query_desc,
                             ID3D11Query** query);
  IFACEMETHODIMP CreatePredicate(const D3D11_QUERY_DESC* predicate_desc,
                                 ID3D11Predicate** predicate);
  IFACEMETHODIMP CreateCounter(const D3D11_COUNTER_DESC* counter_desc,
                               ID3D11Counter** counter);
  IFACEMETHODIMP CreateDeferredContext(UINT context_flags,
                                       ID3D11DeviceContext** deferred_context);
  IFACEMETHODIMP OpenSharedResource(HANDLE resource,
                                    REFIID returned_interface,
                                    void** resource_out);
  IFACEMETHODIMP CheckFormatSupport(DXGI_FORMAT format, UINT* format_support);
  IFACEMETHODIMP CheckMultisampleQualityLevels(DXGI_FORMAT format,
                                               UINT sample_count,
                                               UINT* num_quality_levels);
  IFACEMETHODIMP_(void) CheckCounterInfo(D3D11_COUNTER_INFO* counter_info);
  IFACEMETHODIMP CheckCounter(const D3D11_COUNTER_DESC* desc,
                              D3D11_COUNTER_TYPE* type,
                              UINT* active_counters,
                              LPSTR name,
                              UINT* name_length,
                              LPSTR units,
                              UINT* units_length,
                              LPSTR description,
                              UINT* description_length);
  IFACEMETHODIMP CheckFeatureSupport(D3D11_FEATURE feature,
                                     void* feature_support_data,
                                     UINT feature_support_data_size);
  IFACEMETHODIMP GetPrivateData(REFGUID guid, UINT* data_size, void* data);
  IFACEMETHODIMP SetPrivateData(REFGUID guid, UINT data_size, const void* data);
  IFACEMETHODIMP SetPrivateDataInterface(REFGUID guid, const IUnknown* data);
  IFACEMETHODIMP_(D3D_FEATURE_LEVEL) GetFeatureLevel();
  IFACEMETHODIMP_(UINT) GetCreationFlags();
  IFACEMETHODIMP GetDeviceRemovedReason();
  MOCK_METHOD0(OnGetDeviceRemovedReason, HRESULT());
  IFACEMETHODIMP_(void)
  GetImmediateContext(ID3D11DeviceContext** immediate_context);
  IFACEMETHODIMP SetExceptionMode(UINT raise_flags);
  IFACEMETHODIMP_(UINT) GetExceptionMode();

  // ID3D11Device1
  IFACEMETHODIMP_(void)
  GetImmediateContext1(ID3D11DeviceContext1** immediate_context);
  IFACEMETHODIMP CreateDeferredContext1(
      UINT context_flags,
      ID3D11DeviceContext1** deferred_context);
  IFACEMETHODIMP CreateBlendState1(const D3D11_BLEND_DESC1* blend_state_desc,
                                   ID3D11BlendState1** blend_state);
  IFACEMETHODIMP CreateRasterizerState1(
      const D3D11_RASTERIZER_DESC1* rasterizer_desc,
      ID3D11RasterizerState1** rasterizer_state);
  IFACEMETHODIMP CreateDeviceContextState(
      UINT flags,
      const D3D_FEATURE_LEVEL* feature_levels,
      UINT feature_level_count,
      UINT sdk_version,
      REFIID emulated_interface,
      D3D_FEATURE_LEVEL* chosen_feature_level,
      ID3DDeviceContextState** context_state);
  IFACEMETHODIMP OpenSharedResource1(HANDLE resource,
                                     REFIID returned_interface,
                                     void** resource_out);
  MOCK_METHOD3(DoOpenSharedResource1, HRESULT(HANDLE, REFIID, void**));
  IFACEMETHODIMP OpenSharedResourceByName(LPCWSTR name,
                                          DWORD desired_access,
                                          REFIID returned_interface,
                                          void** resource_out);

  void SetupDefaultMocks();

  Microsoft::WRL::ComPtr<MockD3D11DeviceContext> mock_immediate_context_;

 private:
  ~MockD3D11Device() override;
};

class MockDXGIResource final
    : public MockInterface<IDXGIResource1, IDXGIKeyedMutex> {
 public:
  // IDXGIResource1
  IFACEMETHODIMP CreateSubresourceSurface(UINT index, IDXGISurface2** surface);
  IFACEMETHODIMP CreateSharedHandle(const SECURITY_ATTRIBUTES* attributes,
                                    DWORD access,
                                    LPCWSTR name,
                                    HANDLE* handle);
  // IDXGIResource
  IFACEMETHODIMP GetSharedHandle(HANDLE* shared_handle);
  IFACEMETHODIMP GetUsage(DXGI_USAGE* usage);
  IFACEMETHODIMP SetEvictionPriority(UINT eviction_priority);
  IFACEMETHODIMP GetEvictionPriority(UINT* eviction_priority);
  // IDXGIDeviceSubObject
  IFACEMETHODIMP GetDevice(REFIID riid, void** device);
  // IDXGIObject
  IFACEMETHODIMP SetPrivateData(REFGUID name, UINT data_size, const void* data);
  IFACEMETHODIMP SetPrivateDataInterface(REFGUID name, const IUnknown* unknown);
  IFACEMETHODIMP GetPrivateData(REFGUID name, UINT* data_size, void* data);
  IFACEMETHODIMP GetParent(REFIID riid, void** parent);
  // IDXGIKeyedMutex
  IFACEMETHODIMP AcquireSync(UINT64 key, DWORD milliseconds) override;
  IFACEMETHODIMP ReleaseSync(UINT64 key) override;

 private:
  ~MockDXGIResource() override;
};

class MockD3D11Texture2D final : public MockInterface<ID3D11Texture2D> {
 public:
  MockD3D11Texture2D(D3D11_TEXTURE2D_DESC desc, ID3D11Device* device);
  MockD3D11Texture2D();
  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override;
  // ID3D11Texture2D
  IFACEMETHODIMP_(void) GetDesc(D3D11_TEXTURE2D_DESC* desc);
  // ID3D11Resource
  IFACEMETHODIMP_(void) GetType(D3D11_RESOURCE_DIMENSION* resource_dimension);
  IFACEMETHODIMP_(void) SetEvictionPriority(UINT eviction_priority);
  IFACEMETHODIMP_(UINT) GetEvictionPriority();
  // ID3D11DeviceChild
  IFACEMETHODIMP_(void) GetDevice(ID3D11Device** device);
  IFACEMETHODIMP GetPrivateData(REFGUID guid, UINT* data_size, void* data);
  MOCK_STDCALL_METHOD3(SetPrivateData,
                       HRESULT(REFGUID guid, UINT data_size, const void* data));
  IFACEMETHODIMP SetPrivateDataInterface(REFGUID guid, const IUnknown* data);

  void SetupDefaultMocks();

  Microsoft::WRL::ComPtr<MockDXGIResource> mock_resource_;

 private:
  ~MockD3D11Texture2D() override;
  D3D11_TEXTURE2D_DESC desc_ = {};
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_D3D_CAPTURE_TEST_UTILS_H_
