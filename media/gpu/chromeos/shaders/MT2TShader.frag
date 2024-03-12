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

vec4 getLumaMSBs(highp uvec4 linearIdx, const uint width) {
  vec4 ret;

  // MT2T differs from MM21 because it's a 10-bit format that serializes its
  // pixels into 80 byte "blocks". The first 16 bytes are all of the 2-bit LSB
  // components of the pixels packed together, while the rest of the 64 bytes
  // are the 8-bit MSB components. So, to get the MSBs, we need to skip 16
  // bytes for every 64 pixel block.
  highp uvec4 msbLinearIdx = linearIdx + (linearIdx / 64 + 1) * 16;
  highp uvec4 detiledY = uvec4(vec4(msbLinearIdx)
                         * pushConstants.inverseWidth.x);
  highp uvec4 detiledX = msbLinearIdx - (detiledY * width);

  // Fetch all the MSBs.
  ret.r = texelFetch(yTexture, ivec2(detiledX.r, detiledY.r), 0).r;
  ret.g = texelFetch(yTexture, ivec2(detiledX.g, detiledY.g), 0).r;
  ret.b = texelFetch(yTexture, ivec2(detiledX.b, detiledY.b), 0).r;
  ret.a = texelFetch(yTexture, ivec2(detiledX.a, detiledY.a), 0).r;

  return ret;
}

vec4 getLumaLSBs(highp uvec4 linearIdx, const uint width) {
  // All the LSBs are packed into the first 16 bytes of the 64 pixel block, so
  // we move our "pointer" to the beginning of the block.
  highp uvec4 lsbLinearIdx = (linearIdx / 64) * 80;
  // LSBs are packed into bytes by columns rather than in raster order. So we
  // index into our LSB array using the column.
  lsbLinearIdx = lsbLinearIdx + (linearIdx % 16);
  highp uvec4 detiledY = uvec4(vec4(lsbLinearIdx)
                         * pushConstants.inverseWidth.x);
  highp uvec4 detiledX = lsbLinearIdx - (detiledY * width);

  // Fetch all the LSB bytes.
  vec4 lumaLsbs;
  lumaLsbs.r = texelFetch(yTexture, ivec2(detiledX.r, detiledY.r), 0).r;
  lumaLsbs.g = texelFetch(yTexture, ivec2(detiledX.g, detiledY.g), 0).r;
  lumaLsbs.b = texelFetch(yTexture, ivec2(detiledX.b, detiledY.b), 0).r;
  lumaLsbs.a = texelFetch(yTexture, ivec2(detiledX.a, detiledY.a), 0).r;

  // Which 16 pixel "row" in the 64 pixel block we are in determines which 2
  // bits to extract from the packed LSB byte.
  uvec4 packingIdx = ((linearIdx / 16) % 4) * 2;
  // texelFetch returns a floating point between 0.0 and 1.0, but we really
  // want the raw integer value of this byte so we can unpack it using bitwise
  // operations. So we assume texelFetch interpolates values from 0-255 to
  // 0.0-1.0. In order to convert from the later to the former, we just need
  // to multiply by 255.
  // Likewise, the outColor is expected to be a value between 0.0 and 1.0.
  // Instead of converting all of our pixel values to 10-bit integers and using
  // bitwise operations to insert the LSBs into the right location, then
  // turning everything back into floating points by dividing by 1023, we can
  // just multiply our LSBs by 1/1023 and add that value to the floating point
  // MSBs.
  lumaLsbs = vec4((uvec4(lumaLsbs * 255.0) >> packingIdx) & 0x3)
             * (1.0 / 1023.0);

  return lumaLsbs;
}

// The logic for Chroma MSBs is basically the same as the luma, with the key
// difference that because we are pretending the chroma plane is RG88 instead
// R8, we need to divide all of our constants for 80 byte block unpacking by
// 2 to account for the fact that RG88 elements are 2 bytes instead of 1.
mat2x4 getChromaMSBs(highp uvec4 linearIdx, const uint uvWidth) {
  highp uvec4 msbLinearIdx = linearIdx + (linearIdx / 32 + 1) * 8;
  highp uvec4 detiledY = uvec4(vec4(msbLinearIdx)
                         * pushConstants.inverseWidth.y);
  highp uvec4 detiledX = msbLinearIdx - (detiledY * uvWidth);

  mat4x2 tmp;
  tmp[0] = texelFetch(uvTexture, ivec2(detiledX.r, detiledY.r), 0).rg;
  tmp[1] = texelFetch(uvTexture, ivec2(detiledX.g, detiledY.g), 0).rg;
  tmp[2] = texelFetch(uvTexture, ivec2(detiledX.b, detiledY.b), 0).rg;
  tmp[3] = texelFetch(uvTexture, ivec2(detiledX.a, detiledY.a), 0).rg;

  return transpose(tmp);
}

// Chroma LSBs are similar to Luma LSBs, except with the 2 byte elements again.
mat2x4 getChromaLSBs(highp uvec4 linearIdx, const uint uvWidth) {
  highp uvec4 lsbLinearIdx = (linearIdx / 32) * 40;
  lsbLinearIdx = lsbLinearIdx + (linearIdx % 8);
  highp uvec4 detiledY = uvec4(vec4(lsbLinearIdx)
                         * pushConstants.inverseWidth.y);
  highp uvec4 detiledX = lsbLinearIdx - (detiledY * uvWidth);

  mat4x2 tmp;
  tmp[0] = texelFetch(uvTexture, ivec2(detiledX.r, detiledY.r), 0).rg;
  tmp[1] = texelFetch(uvTexture, ivec2(detiledX.g, detiledY.g), 0).rg;
  tmp[2] = texelFetch(uvTexture, ivec2(detiledX.b, detiledY.b), 0).rg;
  tmp[3] = texelFetch(uvTexture, ivec2(detiledX.a, detiledY.a), 0).rg;

  mat2x4 tmpTranspose = transpose(tmp);
  // Because U and V values are always serialized adjacent to each other, the
  // LSB unpacking bitshift calculation is actually identical for the chroma
  // values of a given pixel, since U and V will be in the same row.
  uvec4 packingIdx = ((linearIdx / 8) % 4) * 2;
  tmpTranspose[0] = vec4((uvec4(tmpTranspose[0] * 255.0) >> packingIdx) & 0x3)
                    * (1.0 / 1023.0);
  tmpTranspose[1] = vec4((uvec4(tmpTranspose[1] * 255.0) >> packingIdx) & 0x3)
                    * (1.0 / 1023.0);

  return tmpTranspose;
}

mat4x3 detileTexels(uvec4 iCoordX, uvec4 iCoordY) {
  mat3x4 ret;

  const uint width = pushConstants.codedDims.x;
  const uint uvWidth = width / uint(2);
  const uint numTilesPerRowY = width / kYTileDims.x;
  const uint numTilesPerRowUV = uvWidth / kUVTileDims.x;

  // Luma detiling logic, same as MM21.
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

  ret[0] = getLumaMSBs(linearIdx, width);
  ret[0] = ret[0] + getLumaLSBs(linearIdx, width);

  // Chroma detiling logic, same as MM21.
  iCoordX = iCoordX / uint(2);
  iCoordY = iCoordY / uint(2);
  inTileX = iCoordX % kUVTileDims.x;
  inTileY = iCoordY % kUVTileDims.y;
  offsetInTile = (inTileY * kUVTileDims.x) + inTileX;
  linearIdx = tileIdx;
  linearIdx = linearIdx * kUVTileDims.x;
  linearIdx = linearIdx * kUVTileDims.y;
  linearIdx = linearIdx + offsetInTile;

  mat2x4 chroma = getChromaMSBs(linearIdx, uvWidth);
  mat2x4 chromaLSBs = getChromaLSBs(linearIdx, uvWidth);

  ret[1] = chroma[0] + chromaLSBs[0];
  ret[2] = chroma[1] + chromaLSBs[1];

  return transpose(ret);
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
  yuv.r -= 16.0/255.0;
  yuv.gb -= vec2(0.5, 0.5);
  vec3 rgbColor = colorConversion * yuv;

  outColor = vec4(rgbColor.r, rgbColor.g, rgbColor.b, 1.0);
}
