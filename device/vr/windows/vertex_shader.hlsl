// Copyright 2017 The Chromium Authors. All rights reserved.
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
  const float PI = 3.141592;
  const float HALF_PI = 0.5 * PI;
  const float QUARTER_PI = 0.25 * PI;

  PixelShaderInput output;
  float4 pos = float4(input.pos, 1.0f, 1.0f);

  output.pos = pos;
  output.idx = input.idx;
  
  float2 uv = float2(input.tex.x, input.tex.y);

  // convert UV (0,0 upper right) (1, 1 lower left) to XY (0,0 lower left) (1, 1 upper right)
  float2 xy = float2(uv.x, 1 - uv.y);

  // convert to -1, -1 lower left, 1, 1 upper right
  xy = float2(2.0 * xy - 1.0);

  // (-pi, -pi_half) lower left, (pi, pi_half) upper right
  xy *= float2(PI, HALF_PI);

  // Convert from one equirect to 2 stacked equirects (left on top of right eye)
  // scaling by two ([-.5, .5 pi] to [-pi. pi])
  xy.y *= 2;
  
  output.tex = xy;

  return output;
}
