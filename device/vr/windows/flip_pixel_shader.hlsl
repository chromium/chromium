// Copyright 2017 The Chromium Authors. All rights reserved.
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








cbuffer PS_CONSTANT_BUFFER : register(b0) {
  float4x4 lookRotation;
  float fovScalarX;
  float fovScalarY;
}

float4 flip_pixel(PixelShaderInput input) : SV_TARGET
{
  const float PI = 3.141592;
  const float HALF_PI = 0.5 * PI;
  const float QUARTER_PI = 0.25 * PI;
  
  float2 xy = float2(input.tex.x, input.tex.y);

  // then subtractingin half pi  for top, adding pi back for bot
  bool isTop = xy.y >= 0;
  if (isTop) {
          //[-PI_HALF, PI_HALF]
          xy.y -= HALF_PI;
  } else {
          // [-PI_HALF, PI_HALF]
          xy.y += HALF_PI;
  }

  // current coordinate space is left eye on top, right eye on bottom
  // both eyes go from -pi -> pi left to right and -pi/2 -> pi/2 bottom to top

  // Get scalar for modifying projection from cubemap (90 fov) to eye target fov
  // float fovScalarX = tan(halfFOVInRadiansX) / tan(QUARTER_PI);
  // float fovScalarY = tan(halfFOVInRadiansY) / tan(QUARTER_PI);

  // create vector looking out at equirect CubeMap
  float3 cubeMapLookupDirection = float3(sin(xy.x), 1.0, cos(xy.x)) * float3(cos(xy.y), sin(xy.y), cos(xy.y));

  // rotate look direction by inverse of horizontal stageSpace look vector.
  // this is a trick to prevent a full cube map render of the scene, the only valid
  // equirectangular projections will be near wahtever is treated as forward and backward traditionally
  cubeMapLookupDirection = mul(lookRotation, float4(cubeMapLookupDirection, 1)).xyz;
  cubeMapLookupDirection.x /= abs(cubeMapLookupDirection.z) * fovScalarX;
  cubeMapLookupDirection.y /= abs(cubeMapLookupDirection.z) * fovScalarY;
  
  if (cubeMapLookupDirection.x >= -1.0 && cubeMapLookupDirection.x <= 1.0 && cubeMapLookupDirection.y >= -1.0 && cubeMapLookupDirection.y <= 1.0) {
    // project the vector onto the 2d texture
    // this will be wrong everywhere that is not near the rotated forward plane of the cubeMap
    // U = ((X/|Z|) + 1) / 2
    // V = ((Y/|Z|) + 1) / 2
    // always project the +Z axis of a cube map
    // X/|Z|, -Y/|Z| places uv coords in -1, 1. + 1 / 2 shifts to 0 -> 1
    // fovScalar scales U/V from 90 degrees into eye fov that was rendered with.
    float projectLookOntoUAxis = (cubeMapLookupDirection.x + 1) / 2;
    float projectLookOntoVAxis = 1 - ((cubeMapLookupDirection.y + 1) / 2);

    float2 eyeUV = float2(projectLookOntoUAxis, projectLookOntoVAxis);

    // copy color from the right eye texture
    eyeUV.x *= 0.5;
    if (isTop) {
      eyeUV.x += 0.5;
    }
    float4 c = my_texture.Sample(my_sampler, eyeUV).rgba;
    /* if (c.r == 0 && c.g == 0 && c.b == 0) {
      c.a = 0;
    } */
    return c;
  } else {
    return float4(0.0, 0.0, 0.0, 0.0);
  }
}
