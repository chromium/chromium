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

#ifndef SERVICES_ML_OPENGL_METAL_MAC_FRAGMENt_SHADER_H_
#define SERVICES_ML_OPENGL_METAL_MAC_FRAGMENt_SHADER_H_

static const char* FRAGMENT_SHADER = R"V0G0N(


#ifdef GL_ES
precision highp float;
#endif

// Declare inputs and outputs
// varTexcoord : TexCoord for the fragment computed by the rasterizer based on
//               the varTexcoord values output in the vertex shader.
// gl_FragColor : Implicitly declare in fragments shaders less than 1.40.
//                Output color of our fragment.
// fragColor : Output color of our fragment.  Basically the same as gl_FragColor,
//             but we must explicitly declared this in shaders version 1.40 and
//             above.

#if __VERSION__ >= 140
in vec2      varTexcoord;
out vec4     fragColor;
#else
varying vec2 varTexcoord;
#endif

uniform sampler2D texture1;

void main (void)
{
	#if __VERSION__ >= 140
    vec4 Color = texture(texture1, varTexcoord.st, 0.0);
    fragColor = ((1.0 - Color.w)) + (Color * Color.w);
	#else
    vec4 Color  = texture2D(texture1, varTexcoord.st, 0.0);
    gl_FragColor = ((1.0 - Color.w)) + (Color * Color.w);
	#endif
}

)V0G0N";

#endif  // SERVICES_ML_MPS_PROTOCOLS_IMPL_H_
