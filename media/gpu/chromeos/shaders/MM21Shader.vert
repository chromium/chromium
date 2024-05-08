// Vertex shader for MM21->ARGB conversion.
// We produce 2 right triangles for each MM21 tile. This allows us to
// compute a few important values in the vertex shader instead of the
// fragment shader. This is desirable because the vertex shader only runs
// 6 times (once for each vertex) per tile, which means the cost is amortized
// over 16*32/6 ~= 85 pixels.

#version 450

precision highp float;
precision highp int;

// We can actually exploit the rasterizer to figure out the intra tile
// coordinates for us.
layout(location = 0) out mediump vec2 intraTileX;
layout(location = 1) out mediump vec2 intraTileY;

layout(location = 2) flat out highp vec2 yOffset;
layout(location = 3) flat out highp vec2 xOffset;

layout( push_constant ) uniform constants {
  // Vulkan push constants have interesting alignment rules, so we use a vec2
  // when we could get away with a float just to make things simple.
  vec2 tilesPerRow;
  vec2 dims;
  vec2 planeStrides;
} pushConstants;

const vec2 kLumaTileDims = vec2(16.0, 32.0);
const vec2 kTileSize = vec2(512.0, 128.0);

const vec2 intraTileCoords[6] = vec2[6](
  vec2(16.0, 0.0),
  vec2(16.0, 32.0),
  vec2(0.0, 0.0),
  vec2(0.0, 0.0),
  vec2(16.0, 32.0),
  vec2(0.0, 32.0)
);

void main() {
  // We really want something like:
  // int tileIdx = gl_VertexIndex / 6;
  // int tileVertIdx = gl_VertexIndex % 6;
  // But integer division and modulo are *very* expensive on mobile GPUs, so
  // we use floating point multiplication, subtraction, and flooring to
  // approximate these operations.
  // 0.1 is a fudge factor to counteract floating point rounding errors.
  // Note that we multiply this value by 6, so using 0.5 like we do in the frag
  // shader isn't appropriate because that will genuinely change the integer
  // answer.
  float tileIdx = floor(float(gl_VertexIndex) * (1.0 / 6.0));
  float preciseTileIdx = tileIdx;
  tileIdx += 0.1;
  uint tileVertIdx = gl_VertexIndex - uint(tileIdx * 6.0);
  vec2 tileCoords;
  tileCoords.g = floor(tileIdx / pushConstants.tilesPerRow.x);
  tileCoords.r = floor(tileIdx - (tileCoords.g * pushConstants.tilesPerRow.x));
  vec2 pos = tileCoords * kLumaTileDims + intraTileCoords[tileVertIdx];
  pos = pos * 2.0 / pushConstants.dims - vec2(1.0, 1.0);
  gl_Position = vec4(pos, 0.0, 1.0);

  // Compute the base address for the whole tile.
  vec2 linearBase = preciseTileIdx * kTileSize;
  linearBase += 0.1;
  yOffset = floor(linearBase / pushConstants.planeStrides);
  xOffset = linearBase - (yOffset * pushConstants.planeStrides);

  vec4 intraTileCoord = vec4(intraTileCoords[tileVertIdx],
  			     intraTileCoords[tileVertIdx] / 2.0);
  intraTileX = intraTileCoord.rb;
  intraTileY = intraTileCoord.ga;
}
