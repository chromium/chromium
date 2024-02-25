#version 450

layout(location = 0) out vec2 outTexCoord;

layout( push_constant ) uniform constants {
  vec2 vertices[4];
} pushConstants;

vec2 texCoords[4] = vec2[4](
  vec2(1.0, 0.0),
  vec2(1.0, 1.0),
  vec2(0.0, 0.0),
  vec2(0.0, 1.0)
);

void main() {
  gl_Position = vec4(pushConstants.vertices[gl_VertexIndex], 0.0, 1.0);
  outTexCoord = texCoords[gl_VertexIndex];
}
