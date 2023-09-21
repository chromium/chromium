#version 450
#extension GL_EXT_samplerless_texture_functions : require

precision mediump float;
precision mediump int;

layout( push_constant ) uniform constants {
  uvec2 codedDims;
  uvec2 visibleDims;
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

vec3 detileTexel(uvec2 iCoord) {
  vec3 yuv;
  uint width = pushConstants.codedDims.x;
  uint uvWidth = width / uint(2);
  uint numTilesPerRow = pushConstants.codedDims.x / kYTileDims.x;
  uvec2 tileCoords = iCoord / kYTileDims;
  uint tileIdx = (tileCoords.y * numTilesPerRow) + tileCoords.x;
  uvec2 inTileCoord = iCoord % kYTileDims;
  uint offsetInTile = (inTileCoord.y * kYTileDims.x) + inTileCoord.x;
  highp uint linearIdx = tileIdx;
  linearIdx = linearIdx * kYTileDims.x;
  linearIdx = linearIdx * kYTileDims.y;
  linearIdx = linearIdx + offsetInTile;
  uint detiledY = linearIdx / width;
  uint detiledX = linearIdx % width;
  yuv.r = texelFetch(yTexture, ivec2(detiledX, detiledY), 0).r;
  iCoord = iCoord / uint(2);
  tileCoords = iCoord / kUVTileDims;
  numTilesPerRow = uvWidth / kUVTileDims.x;
  tileIdx = (tileCoords.y * numTilesPerRow) + tileCoords.x;
  inTileCoord = iCoord % kUVTileDims;
  offsetInTile = (inTileCoord.y * kUVTileDims.x) + inTileCoord.x;
  linearIdx = tileIdx;
  linearIdx = linearIdx * kUVTileDims.x;
  linearIdx = linearIdx * kUVTileDims.y;
  linearIdx = linearIdx + offsetInTile;
  detiledY = linearIdx / uvWidth;
  detiledX = linearIdx % uvWidth;
  yuv.gb = texelFetch(uvTexture, ivec2(detiledX, detiledY), 0).rg;
  return yuv;
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
  vec3 topLeft = detileTexel(topLeftCoord);
  vec3 topRight = detileTexel(topRightCoord);
  vec3 bottomLeft = detileTexel(bottomLeftCoord);
  vec3 bottomRight = detileTexel(bottomRightCoord);
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
