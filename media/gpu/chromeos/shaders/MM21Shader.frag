// Frag shader for MM21->ARGB conversion.

#version 450
#extension GL_EXT_samplerless_texture_functions : require

precision mediump float;
precision mediump int;

layout(location = 0) in mediump vec2 intraTileX;
layout(location = 1) in mediump vec2 intraTileY;

layout(location = 2) in flat highp vec2 yOffset;
layout(location = 3) in flat highp vec2 xOffset;

layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants {
  layout(offset = 24) vec2 planeStrides;
} pushConstants;

// Ideally we would just use a 1D texture, but Vulkan has very tight limits
// on how large those can be, so we use 2D arrays instead and reinterpret our
// address as texture coordinates.
// TODO(b/304781371): Since the dimensions of these textures are arbitrary,
// investigate if there are some dimensions which are more efficient than
// others. For example, powers of 2 might make the division faster, or a
// width of 16*32 might eliminate the need to do any division at all.
layout(binding = 0) uniform texture2D lumaTexture;
layout(binding = 1) uniform texture2D chromaTexture;

const mat3 colorConversion = mat3(1.164, 1.164, 1.164,
          0.0, -0.391, 2.018,
          1.596, -0.813, 0.0);

void main() {
  vec2 linearIdx = (floor(intraTileY) * vec2(16, 8)) +
                   floor(intraTileX) + xOffset;
  // Like in the corresponding vertex shader, we really wanted integer
  // division and modulo, but floating point is faster.
  highp vec2 detiledY = floor(linearIdx / pushConstants.planeStrides);
  highp vec2 detiledX = linearIdx - (detiledY * pushConstants.planeStrides);
  detiledY += yOffset;

  vec3 yuv;
  yuv.r = texelFetch(lumaTexture, ivec2(detiledX[0], detiledY[0]), 0).r;
  yuv.gb = texelFetch(chromaTexture, ivec2(detiledX[1], detiledY[1]), 0).rg;

  // Standard BT.601 YUV->RGB color conversion.
  yuv.r -= 16.0/255.0;
  yuv.gb -= vec2(0.5, 0.5);
  outColor = vec4(colorConversion * yuv, 1.0);
}
