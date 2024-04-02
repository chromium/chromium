// Vertex shader for MT2T->AR30 conversion.
// This shader is very similar to its MM21 counterpart, with the important
// difference being that the linear base address needs to take into account the
// packed LSB data, so we multiply it by 10/8=5/4.

#version 450

precision highp float;
precision highp int;

layout(location = 0) out vec2 intraTileX;
layout(location = 1) out vec2 intraTileY;
layout(location = 2) out flat ivec2 linearBase;

layout( push_constant ) uniform constants {
  // Vulkan push constants have interesting alignment rules, so we use a vec2
  // when we could get away with a float just to make things simple.
  vec2 tilesPerRow;
  vec2 dims;
} pushConstants;

const ivec2 kLumaTileDims = ivec2(16, 32);
const int kLumaTileSize = kLumaTileDims.x * kLumaTileDims.y;

const vec2 intraTileCoords[6] = vec2[6](
  vec2(16.0, 0.0),
  vec2(16.0, 32.0),
  vec2(0.0, 0.0),
  vec2(0.0, 0.0),
  vec2(16.0, 32.0),
  vec2(0.0, 32.0)
);

void main() {
  float tileIdx = floor(float(gl_VertexIndex) * (1.0 / 6.0)) + 0.1;
  uint tileVertIdx = gl_VertexIndex - uint(tileIdx * 6.0);
  highp ivec2 tileCoords;
  tileCoords[1] = int(tileIdx / pushConstants.tilesPerRow.x);
  tileCoords[0] = int(tileIdx - (tileCoords[1] * pushConstants.tilesPerRow.x));
  vec2 pos = vec2(tileCoords * kLumaTileDims) + intraTileCoords[tileVertIdx];
  pos = pos * 2 / pushConstants.dims - vec2(1.0, 1.0);
  gl_Position = vec4(pos, 0.0, 1.0);

  linearBase.r = int(tileIdx) * kLumaTileSize;
  linearBase.g = linearBase.r / 4;
  linearBase = linearBase * 5 / 4;

  vec4 intraTileCoord = vec4(intraTileCoords[tileVertIdx],
  			     intraTileCoords[tileVertIdx] / 2.0);
  intraTileX = intraTileCoord.rb;
  intraTileY = intraTileCoord.ga;
}
