/*
 Copyright © 2018 Apple Inc.

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#ifndef SERVICES_ML_OPENGL_METAL_MAC_VERTEX_SHADER_H_
#define SERVICES_ML_OPENGL_METAL_MAC_VERTEX_SHADER_H_

static const char* VERTEX_SHADER = R"V0G0N(


#ifdef GL_ES
precision highp float;
#endif

uniform mat4 modelViewProjectionMatrix;

// Declare inputs and outputs
// inPosition : Position attributes from the VAO/VBOs
// inTexcoord : Texcoord attributes from the VAO/VBOs
// varTexcoord : TexCoord we'll pass to the rasterizer
// gl_Position : implicitly declared in all vertex shaders. Clip space position
//               passed to rasterizer used to build the triangles

#if __VERSION__ >= 140
in vec4  inPosition;  
in vec2  inTexcoord;
out vec2 varTexcoord;
#else
attribute vec4 inPosition;  
attribute vec2 inTexcoord;
varying vec2 varTexcoord;
#endif

void main (void) 
{
    gl_Position = inPosition;

    varTexcoord = inTexcoord;
}

)V0G0N";

#endif  // SERVICES_ML_OPENGL_METAL_MAC_VERTEX_SHADER_H_
