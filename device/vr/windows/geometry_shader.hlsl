// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
struct VertexShaderOutput
{
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
  uint idx : TEXCOORD1;
};

struct GeometryShaderOutput
{
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
  uint idx : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)]
void geometry(triangle VertexShaderOutput input[3], inout TriangleStream<GeometryShaderOutput> stream)
{
  GeometryShaderOutput output;
  [unroll(3)]
  for (int i = 0; i < 3; ++i)
  {
    output.pos = input[i].pos;
    output.tex = input[i].tex;
    output.idx = input[i].idx;
    stream.Append(output);
  }
}
