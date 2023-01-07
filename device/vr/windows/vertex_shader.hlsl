// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
struct VertexShaderInput
{
  float2 pos : POSITION;
  float2 tex : TEXCOORD0;
  uint idx : TEXCOORD1;
};

struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
  uint idx : TEXCOORD1;
};

PixelShaderInput vertex(VertexShaderInput input)
{
  PixelShaderInput output;
  float4 pos = float4(input.pos, 1.0f, 1.0f);

  output.pos = pos;
  output.tex = input.tex;
  output.idx = input.idx;

  return output;
}
