// Vertex shader for scaling, rotate, and crop.

#version 450

layout(location = 0) out vec2 outTexCoord;

layout( push_constant ) uniform constants {
  vec2 vertices[6];
  vec2 visibleScale;
} pushConstants;

// Texture coordinates are always fixed.
vec2 texCoords[6] = vec2[6](
  vec2(1.0, 0.0),
  vec2(1.0, 1.0),
  vec2(0.0, 0.0),
  vec2(0.0, 0.0),
  vec2(1.0, 1.0),
  vec2(0.0, 1.0)
);

void main() {
  // Adjust the gl_Position according to scaling, rotate, and crop parameters.
  gl_Position = vec4(pushConstants.vertices[gl_VertexIndex], 0.0, 1.0);

  // We over-allocate the pivot buffer, so we need to scale texture coordinates
  // accordingly.
  outTexCoord = texCoords[gl_VertexIndex] * pushConstants.visibleScale;
}
