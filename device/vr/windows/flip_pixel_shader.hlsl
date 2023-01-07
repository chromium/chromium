// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Texture2D my_texture;
SamplerState my_sampler
{
  Filter = MIN_MAG_MIP_LINEAR;
  AddressU = Wrap;
  AddressV = Wrap;
};

struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
};

float4 flip_pixel(PixelShaderInput input) : SV_TARGET
{
  float2 texture_coords = float2(input.tex.x, input.tex.y);
  return my_texture.Sample(my_sampler, texture_coords).rgba;
}
