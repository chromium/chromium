#version 450

layout(location = 0) out vec2 outTexCoord;

vec2 positions[6] = vec2[](
  vec2(-1.0, -1.0),
  vec2(-1.0, 1.0),
  vec2(1.0, 1.0),
  vec2(1.0, 1.0),
  vec2(1.0, -1.0),
  vec2(-1.0, -1.0)
);

void main() {
  vec2 texCoord = positions[gl_VertexIndex];
  gl_Position = vec4(texCoord, 0.0, 1.0);
  outTexCoord = (texCoord + vec2(1.0, 1.0)) / 2;
}
