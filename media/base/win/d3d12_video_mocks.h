// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_D3D12_VIDEO_MOCKS_H_
#define MEDIA_BASE_WIN_D3D12_VIDEO_MOCKS_H_

#include <d3d12video.h>
#include <wrl.h>

#include "media/base/win/test_utils.h"

namespace media {

class D3D12VideoDevice3Mock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D12VideoDevice3> {
 public:
  D3D12VideoDevice3Mock();
  ~D3D12VideoDevice3Mock() override;

  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID riid, void** ppvObject));

  // Interfaces of ID3D12VideoDevice

  MOCK_STDCALL_METHOD3(
      CheckFeatureSupport,
      HRESULT(D3D12_FEATURE_VIDEO FeatureVideo,
              _Inout_updates_bytes_(
                  FeatureSupportDataSize) void* pFeatureSupportData,
              UINT FeatureSupportDataSize));
  MOCK_STDCALL_METHOD3(CreateVideoDecoder,
                       HRESULT(const D3D12_VIDEO_DECODER_DESC* pDesc,
                               REFIID riid,
                               void** ppVideoDecoder));
  MOCK_STDCALL_METHOD3(
      CreateVideoDecoderHeap,
      HRESULT(const D3D12_VIDEO_DECODER_HEAP_DESC* pVideoDecoderHeapDesc,
              REFIID riid,
              void** ppVideoDecoderHeap));
  MOCK_STDCALL_METHOD6(
      CreateVideoProcessor,
      HRESULT(UINT NodeMask,
              const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC* pOutputStreamDesc,
              UINT NumInputStreamDescs,
              const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC* pInputStreamDescs,
              REFIID riid,
              void** ppVideoProcessor));

  // Interfaces of ID3D12VideoDevice1

  MOCK_STDCALL_METHOD4(
      CreateVideoMotionEstimator,
      HRESULT(const D3D12_VIDEO_MOTION_ESTIMATOR_DESC* pDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoMotionEstimator));

  MOCK_STDCALL_METHOD4(
      CreateVideoMotionVectorHeap,
      HRESULT(const D3D12_VIDEO_MOTION_VECTOR_HEAP_DESC* pDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoMotionVectorHeap));

  // Interfaces of ID3D12VideoDevice2

  MOCK_STDCALL_METHOD4(
      CreateVideoDecoder1,
      HRESULT(const D3D12_VIDEO_DECODER_DESC* pDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoDecoder));

  MOCK_STDCALL_METHOD4(
      CreateVideoDecoderHeap1,
      HRESULT(const D3D12_VIDEO_DECODER_HEAP_DESC* pVideoDecoderHeapDesc,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoDecoderHeap));

  MOCK_STDCALL_METHOD7(
      CreateVideoProcessor1,
      HRESULT(UINT NodeMask,
              const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC* pOutputStreamDesc,
              UINT NumInputStreamDescs,
              const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC* pInputStreamDescs,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoProcessor));

  MOCK_STDCALL_METHOD6(
      CreateVideoExtensionCommand,
      HRESULT(const D3D12_VIDEO_EXTENSION_COMMAND_DESC* pDesc,
              const void* pCreationParameters,
              SIZE_T CreationParametersDataSizeInBytes,
              ID3D12ProtectedResourceSession* pProtectedResourceSession,
              REFIID riid,
              void** ppVideoExtensionCommand));

  MOCK_STDCALL_METHOD5(ExecuteExtensionCommand,
                       HRESULT(ID3D12VideoExtensionCommand* pExtensionCommand,
                               const void* pExecutionParameters,
                               SIZE_T ExecutionParametersSizeInBytes,
                               void* pOutputData,
                               SIZE_T OutputDataSizeInBytes));

  // Interfaces of ID3D12VideoDevice3

  MOCK_STDCALL_METHOD3(CreateVideoEncoder,
                       HRESULT(const D3D12_VIDEO_ENCODER_DESC* pDesc,
                               REFIID riid,
                               void** ppVideoEncoder));
  MOCK_STDCALL_METHOD3(CreateVideoEncoderHeap,
                       HRESULT(const D3D12_VIDEO_ENCODER_HEAP_DESC* pDesc,
                               REFIID riid,
                               void** ppVideoEncoderHeap));
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_D3D12_VIDEO_MOCKS_H_
