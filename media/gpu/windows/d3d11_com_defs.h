// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_
#define MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_

#include <d3d11.h>
#include <d3d11_4.h>
#include <wrl/client.h>

// TODO(tmathmeyer) Ensure that Microsoft::WRL::ComPtr doesn't show up
// in any of the other files in media/gpu/windows.
namespace media {

// We want to shorten this so the |using| statements are single line -
// this improves readability greatly.
#define COM Microsoft::WRL::ComPtr

// Keep these sorted alphabetically.
using ComD3D11CryptoSession = COM<ID3D11CryptoSession>;
using ComD3D11Device = COM<ID3D11Device>;
using ComD3D11DeviceContext = COM<ID3D11DeviceContext>;
using ComD3D11Multithread = COM<ID3D11Multithread>;
using ComD3D11Query = COM<ID3D11Query>;
using ComD3D11Texture2D = COM<ID3D11Texture2D>;
using ComD3D11VideoContext = COM<ID3D11VideoContext>;
using ComD3D11VideoContext1 = COM<ID3D11VideoContext1>;
using ComD3D11VideoDecoder = COM<ID3D11VideoDecoder>;
using ComD3D11VideoDecoderOutputView = COM<ID3D11VideoDecoderOutputView>;
using ComD3D11VideoDevice = COM<ID3D11VideoDevice>;
using ComD3D11VideoDevice1 = COM<ID3D11VideoDevice1>;
using ComD3D11VideoProcessor = COM<ID3D11VideoProcessor>;
using ComD3D11VideoProcessorEnumerator = COM<ID3D11VideoProcessorEnumerator>;
using ComD3D11VideoProcessorInputView = COM<ID3D11VideoProcessorInputView>;
using ComD3D11VideoProcessorOutputView = COM<ID3D11VideoProcessorOutputView>;

using ComDXGIAdapter3 = COM<IDXGIAdapter3>;
using ComDXGIDevice2 = COM<IDXGIDevice2>;

#undef COM

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_COM_DEFS_H_
