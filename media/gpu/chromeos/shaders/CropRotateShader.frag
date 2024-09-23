// Simple fragment shader for scaling, rotate, and crop.
// All of the logic is in the corresponding vertex shader.

#version 450

layout(binding = 0) uniform sampler2D inTexture;

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = texture(inTexture, texCoord);
}
