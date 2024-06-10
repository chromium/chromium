// Frag shader for MT2T->AR30 conversion.

#version 450
#extension GL_EXT_samplerless_texture_functions : require

precision mediump float;
precision mediump int;

layout(location = 0) in vec2 intraTileX;
layout(location = 1) in vec2 intraTileY;

layout(location = 2) in flat highp vec2 yOffset;
layout(location = 3) in flat highp vec2 xOffset;

layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants {
  layout(offset = 24) highp vec2 planeStrides;
} pushConstants;

layout(binding = 0) uniform texture2D lumaTexture;
layout(binding = 1) uniform texture2D chromaTexture;

const mat3 colorConversion = mat3(1.164, 1.164, 1.164,
          0.0, -0.391, 2.018,
          1.596, -0.813, 0.0);

void main() {
  vec2 blockIdx = floor(intraTileY / 4.0);
  vec2 blockRowIdx = intraTileY - (blockIdx * 4.0);

  // MSB indices need to be adjusted by how many LSB bytes are serialized
  // before the current MSB. Every 64 pixel block starts with 16 bytes of LSB
  // data.
  highp vec2 msbLinearIdx = (floor(intraTileY) * vec2(16, 8)) +
                      floor(intraTileX) + xOffset;
  msbLinearIdx += (blockIdx + 1.0) * vec2(16, 8);

  // Likewise, we need to find the address of our LSB byte. Since each LSB byte
  // encodes the LSBs for a 1x4 mini-tile, we can compute a base address using
  // blockIdx * 16 bytes, and then offset it by the intra tile X coordinate.
  highp vec2 lsbLinearIdx = blockIdx * vec2(80, 40) + xOffset;
  lsbLinearIdx += floor(intraTileX);

  // 0.5 is a floating point issue fudge factor.
  highp vec4 linearIdx = vec4(msbLinearIdx, lsbLinearIdx);

  highp vec4 strides = vec4(pushConstants.planeStrides,
                            pushConstants.planeStrides);
  highp vec4 detiledY = floor(linearIdx / strides);
  highp vec4 detiledX = linearIdx - (detiledY * strides);
  detiledY += vec4(yOffset, yOffset);

  vec3 yuv;
  yuv.r = texelFetch(lumaTexture, ivec2(detiledX.r, detiledY.r), 0).r;
  yuv.gb = texelFetch(chromaTexture, ivec2(detiledX.g, detiledY.g), 0).rg;

  vec3 yuvLsb;
  yuvLsb.r = texelFetch(lumaTexture, ivec2(detiledX.b, detiledY.b), 0).r;
  yuvLsb.gb = texelFetch(chromaTexture, ivec2(detiledX.a, detiledY.a), 0).rg;

  // LSBs are packed into their corresponding byte with the top of the tile
  // packed into the least significant 2 bits of the byte first. So ideally,
  // we would bit shift the LSB byte based on what row within the block the
  // current pixel is in, mask all but the lower two bits, and then shift those
  // bits into where they need to go. But, because both texelFetch and outColor
  // are floating points, this requires integer conversions, which can be quite
  // slow. So instead, we use a multiplication to emulate a left shift, then
  // fract() to emulate discarding the bits that were shifted too high for
  // the register, and then division (multiplication by the reciprocal) to
  // emulate a right shift. The left shift constant should be
  // 2^(2*(3-blockRowIdx)), and the right shift constant should be 2^-8 in
  // order for this to work. The one hitch is that the floating point color
  // values range from 0.0 to 1.0, but baked into our bit shifting trick is
  // the assumption that the colors range from 0.0 to 255.0/256.0 and 0.0 to
  // 1023.0/1024.0 respectively. We can fix this by factoring into our shift
  // multiplication constants the conversion terms 255.0/256.0 and
  // 1024.0/1023.0.
  //
  // Note that the nature of floating point division means we don't emulate
  // discarding bits that are shifted too low, we just keep them as a tiny
  // fractional component. This means this approach is only approximately
  // correct. But, it is guaranteed to be correct within 1/1024, which is
  // all we need for 10-bit color accuracy.
  vec3 shift = ldexp(vec2((255.0/256.0)), ivec2(2.0 * (3.0 - blockRowIdx))).rgg;
  yuvLsb *= shift;
  yuvLsb = fract(yuvLsb);
  yuvLsb *= (1.0 / 256.0 * 1024.0 / 1023.0);
  yuv += yuvLsb;

  yuv.r -= 16.0/255.0;
  yuv.gb -= vec2(0.5, 0.5);
  outColor = vec4(colorConversion * yuv, 1.0);
}
