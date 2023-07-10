// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_
#define MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_

#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <mfapi.h>
#include <mftransform.h>
#include <strmif.h>
#include <wrl/client.h>

namespace media {

// We want to shorten this so the |using| statements are single line -
// this improves readability greatly.
#define COM Microsoft::WRL::ComPtr

// Keep these sorted alphabetically.
using ComD3D11CryptoSession = COM<ID3D11CryptoSession>;
using ComD3D11Debug = COM<ID3D11Debug>;
using ComD3D11Device = COM<ID3D11Device>;
using ComD3D11Device1 = COM<ID3D11Device1>;
using ComD3D11DeviceContext = COM<ID3D11DeviceContext>;
using ComD3D11InfoQueue = COM<ID3D11InfoQueue>;
using ComD3D11Multithread = COM<ID3D11Multithread>;
using ComD3D11Query = COM<ID3D11Query>;
using ComD3D11Texture2D = COM<ID3D11Texture2D>;
using ComD3D11VideoContext = COM<ID3D11VideoContext>;
using ComD3D11VideoContext1 = COM<ID3D11VideoContext1>;
using ComD3D11VideoContext2 = COM<ID3D11VideoContext2>;
using ComD3D11VideoDecoder = COM<ID3D11VideoDecoder>;
using ComD3D11VideoDecoderOutputView = COM<ID3D11VideoDecoderOutputView>;
using ComD3D11VideoDevice = COM<ID3D11VideoDevice>;
using ComD3D11VideoDevice1 = COM<ID3D11VideoDevice1>;
using ComD3D11VideoProcessor = COM<ID3D11VideoProcessor>;
using ComD3D11VideoProcessorEnumerator = COM<ID3D11VideoProcessorEnumerator>;
using ComD3D11VideoProcessorInputView = COM<ID3D11VideoProcessorInputView>;
using ComD3D11VideoProcessorOutputView = COM<ID3D11VideoProcessorOutputView>;

using ComDXGIAdapter = COM<IDXGIAdapter>;
using ComDXGIAdapter3 = COM<IDXGIAdapter3>;
using ComDXGIDevice = COM<IDXGIDevice>;
using ComDXGIDevice2 = COM<IDXGIDevice2>;
using ComDXGIFactory = COM<IDXGIFactory>;
using ComDXGIKeyedMutex = COM<IDXGIKeyedMutex>;
using ComDXGIOutput = COM<IDXGIOutput>;
using ComDXGIOutput6 = COM<IDXGIOutput6>;
using ComDXGIResource1 = COM<IDXGIResource1>;

using ComCodecAPI = COM<ICodecAPI>;
using ComMFActivate = COM<IMFActivate>;
using ComMFAttributes = COM<IMFAttributes>;
using ComMFMediaBuffer = COM<IMFMediaBuffer>;
using ComMFMediaEvent = COM<IMFMediaEvent>;
using ComMFMediaEventGenerator = COM<IMFMediaEventGenerator>;
using ComMFMediaType = COM<IMFMediaType>;
using ComMFSample = COM<IMFSample>;
using ComMFTransform = COM<IMFTransform>;

#undef COM

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_
