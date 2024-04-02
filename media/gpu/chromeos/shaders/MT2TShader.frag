// Frag shader for MT2T->AR30 conversion.

#version 450
#extension GL_EXT_samplerless_texture_functions : require

precision highp float;
precision highp int;

layout(location = 0) in vec2 intraTileX;
layout(location = 1) in vec2 intraTileY;
layout(location = 2) in flat ivec2 linearBase;

layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants {
  layout(offset = 16) vec2 planeStrides;
} pushConstants;

layout(binding = 0) uniform texture2D lumaTexture;
layout(binding = 1) uniform texture2D chromaTexture;

const mat3 colorConversion = mat3(1.164, 1.164, 1.164,
          0.0, -0.391, 2.018,
          1.596, -0.813, 0.0);

void main() {
  vec2 blockIdx = floor(intraTileY / 4.0);

  // MSB indices need to be adjusted by how many LSB bytes are serialized
  // before the current MSB. Every 64 pixel block starts with 16 bytes of LSB
  // data.
  vec2 msbLinearIdx = (floor(intraTileY) * vec2(16, 8)) +
                      floor(intraTileX) + vec2(linearBase);
  msbLinearIdx += (blockIdx + 1.0) * vec2(16, 8);

  // Likewise, we need to find the address of our LSB byte. Since each LSB byte
  // encodes the LSBs for a 1x4 mini-tile, we can compute a base address using
  // blockIdx * 16 bytes, and then offset it by the intra tile X coordinate.
  vec2 lsbLinearIdx = linearBase + blockIdx * vec2(80, 40);
  lsbLinearIdx += floor(intraTileX);

  // 0.5 is a floating point issue fudge factor.
  vec4 linearIdx = vec4(msbLinearIdx, lsbLinearIdx) + 0.5;

  vec4 strides = vec4(pushConstants.planeStrides, pushConstants.planeStrides);
  vec4 detiledY = floor(linearIdx / strides);
  vec4 detiledX = linearIdx - (detiledY * strides);

  vec3 yuv;
  yuv.r = texelFetch(lumaTexture, ivec2(detiledX.r, detiledY.r), 0).r;
  yuv.gb = texelFetch(chromaTexture, ivec2(detiledX.g, detiledY.g), 0).rg;

  vec3 yuvLsb;
  yuvLsb.r = texelFetch(lumaTexture, ivec2(detiledX.b, detiledY.b), 0).r;
  yuvLsb.gb = texelFetch(chromaTexture, ivec2(detiledX.a, detiledY.a), 0).rg;

  // LSBs are packed into their corresponding byte with the top of the tile
  // packed into the least significant 2 bits of the byte first.
  ivec3 shift = (ivec3(intraTileY.rgg) % 4) * 2;
  // Both our texelFetch values and our outColor are floating point conversions
  // of fundamentally integer data, so we need to first convert to int,
  // then perform our bitwise operations, and then convert back to float.
  // TODO(b/304781371): This particular operation seems to be relatively
  // expensive, possibly because of converting between ints and floats multiple
  // times. Investigate if there are faster ways to accomplish the same thing.
  yuvLsb = vec3((ivec3(yuvLsb * 255.0) >> shift) & 0x3) * (1 / 1023.0);
  yuv += yuvLsb;

  yuv.r -= 16.0/255.0;
  yuv.gb -= vec2(0.5, 0.5);
  outColor = vec4(colorConversion * yuv, 1.0);
}
