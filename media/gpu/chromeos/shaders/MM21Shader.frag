#version 450
#extension GL_EXT_samplerless_texture_functions : require

precision mediump float;
precision mediump int;

layout( push_constant ) uniform fragmentConstants {
  // Takes into account offset from vertex shader push constants.
  layout(offset = 32) uvec2 codedDims;
  uvec2 visibleDims;

  // Multiplication is much faster than division.
  // Note: IEEE 754 single precision floating point guarantees us a 23-bit
  // mantissa. This means that values up to 8388608 should be exactly
  // representable. 4K is 8294400 pixels, so we're cutting it a little close.
  highp vec2 inverseWidth;
} pushConstants;

layout(binding = 0) uniform texture2D yTexture;
layout(binding = 1) uniform texture2D uvTexture;

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

const mat3 colorConversion = mat3(1.164, 1.164, 1.164,
          0.0, -0.391, 2.018,
          1.596, -0.813, 0.0);
const uvec2 kYTileDims = uvec2(16, 32);
const uvec2 kUVTileDims = uvec2(8, 16);

// TODO(b/304781371): Re-write this shader to assume every input texture is
// exactly 512 pixels wide. We can skip a large amount of these calculations
// in that circumstance because detiledY just becomes tileIdx, and detiledX
// just becomes offsetInTile. Unfortunately, that tactic requires a number of
// hacks to "lie" to the GMB and SharedImage infrastructure.

mat4x3 detileTexels(uvec4 iCoordX, uvec4 iCoordY) {
  mat4x3 ret;

  const uint width = pushConstants.codedDims.x;
  const uint uvWidth = width / uint(2);
  const uint numTilesPerRowY = width / kYTileDims.x;
  const uint numTilesPerRowUV = uvWidth / kUVTileDims.x;

  uvec4 tileX = iCoordX / kYTileDims.x;
  uvec4 tileY = iCoordY / kYTileDims.y;
  uvec4 tileIdx = (tileY * numTilesPerRowY) + tileX;
  uvec4 inTileX = iCoordX % kYTileDims.x;
  uvec4 inTileY = iCoordY % kYTileDims.y;
  uvec4 offsetInTile = (inTileY * kYTileDims.x) + inTileX;
  highp uvec4 linearIdx = tileIdx;
  linearIdx = linearIdx * kYTileDims.x;
  linearIdx = linearIdx * kYTileDims.y;
  linearIdx = linearIdx + offsetInTile;
  highp uvec4 detiledY = uvec4(vec4(linearIdx) * pushConstants.inverseWidth.x);
  highp uvec4 detiledX = linearIdx - (detiledY * width);

  ret[0].r = texelFetch(yTexture, ivec2(detiledX.r, detiledY.r), 0).r;
  ret[1].r = texelFetch(yTexture, ivec2(detiledX.g, detiledY.g), 0).r;
  ret[2].r = texelFetch(yTexture, ivec2(detiledX.b, detiledY.b), 0).r;
  ret[3].r = texelFetch(yTexture, ivec2(detiledX.a, detiledY.a), 0).r;

  iCoordX = iCoordX / uint(2);
  iCoordY = iCoordY / uint(2);
  inTileX = iCoordX % kUVTileDims.x;
  inTileY = iCoordY % kUVTileDims.y;
  offsetInTile = (inTileY * kUVTileDims.x) + inTileX;
  linearIdx = tileIdx;
  linearIdx = linearIdx * kUVTileDims.x;
  linearIdx = linearIdx * kUVTileDims.y;
  linearIdx = linearIdx + offsetInTile;
  detiledY = uvec4(vec4(linearIdx) * pushConstants.inverseWidth.y);
  detiledX = linearIdx - (detiledY * uvWidth);

  ret[0].gb = texelFetch(uvTexture, ivec2(detiledX.r, detiledY.r), 0).rg;
  ret[1].gb = texelFetch(uvTexture, ivec2(detiledX.g, detiledY.g), 0).rg;
  ret[2].gb = texelFetch(uvTexture, ivec2(detiledX.b, detiledY.b), 0).rg;
  ret[3].gb = texelFetch(uvTexture, ivec2(detiledX.a, detiledY.a), 0).rg;

  return ret;
}

void main() {
  // Manually do bilinear filtering.
  // TODO: This algorithm doesn't work for downsampling by more than 2.
  vec2 unnormalizedCoord = texCoord *
        vec2(pushConstants.visibleDims - uvec2(1, 1));
  uvec2 topLeftCoord = uvec2(floor(unnormalizedCoord));
  uvec2 bottomRightCoord = uvec2(ceil(unnormalizedCoord));
  uvec2 topRightCoord =
    uvec2(ceil(unnormalizedCoord.x), floor(unnormalizedCoord.y));
  uvec2 bottomLeftCoord =
    uvec2(floor(unnormalizedCoord.x), ceil(unnormalizedCoord.y));
  uvec4 iCoordX = uvec4(topLeftCoord.x, topRightCoord.x,
                        bottomLeftCoord.x, bottomRightCoord.x);
  uvec4 iCoordY = uvec4(topLeftCoord.y, topRightCoord.y,
                        bottomLeftCoord.y, bottomRightCoord.y);
  mat4x3 pixels = detileTexels(iCoordX, iCoordY);
  vec3 topLeft = pixels[0];
  vec3 topRight = pixels[1];
  vec3 bottomLeft = pixels[2];
  vec3 bottomRight = pixels[3];
  vec3 top = mix(topRight, topLeft,
                 float(topRightCoord.x) - unnormalizedCoord.x);
  vec3 bottom = mix(bottomRight, bottomLeft,
                    float(bottomRightCoord.x) - unnormalizedCoord.x);
  vec3 yuv = mix(bottom, top, float(bottomLeftCoord.y) - unnormalizedCoord.y);

  // Do RGB conversion
  yuv.r -= 16.0/256.0;
  yuv.gb -= vec2(0.5, 0.5);
  vec3 rgbColor = colorConversion * yuv;

  outColor = vec4(rgbColor.r, rgbColor.g, rgbColor.b, 1.0);
}
