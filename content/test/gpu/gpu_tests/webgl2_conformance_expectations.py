# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tests.webgl_conformance_expectations import WebGLConformanceExpectations

# See the GpuTestExpectations class for documentation.

class WebGL2ConformanceExpectations(WebGLConformanceExpectations):
  def __init__(self, is_asan=False):
    super(WebGL2ConformanceExpectations, self).__init__(is_asan=is_asan)

  def SetExpectations(self):
    # ===================================
    # Extension availability expectations
    # ===================================
    # It's expected that not all extensions will be available on all platforms.
    # Having a test listed here is not necessarily a problem.

    # Skip these, rather than expect them to fail, to speed up test
    # execution. The browser is restarted even after expected test
    # failures.
    self.Skip('WebglExtension_WEBGL_compressed_texture_astc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_etc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_etc1',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_pvrtc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_s3tc_srgb',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_multiview',
        ['mac', 'linux', 'android'], bug=864524)
    # The multiview extension is only expected to be supported through ANGLE.
    self.Skip('WebglExtension_WEBGL_multiview',
        ['win', 'no_passthrough'], bug=864524)
    # # ANGLE's OpenGL backend supports multiview only on NVIDIA.
    self.Skip('WebglExtension_WEBGL_multiview',
        ['win', 'passthrough', 'opengl', 'intel'], bug=864524)
    self.Skip('WebglExtension_EXT_disjoint_timer_query_webgl2',
        ['android'], bug=808744)
    self.Fail('WebglExtension_EXT_disjoint_timer_query_webgl2',
        ['linux', 'intel'], bug=867675)
    self.Skip('WebglExtension_KHR_parallel_shader_compile',
        ['no_passthrough'], bug=849576)

    # ========================
    # Conformance expectations
    # ========================

    # Failing new test
    self.Fail(
        'conformance2/glsl3/const-struct-from-array-as-function-parameter.html',
        bug=874620)

    # Failing new test added in https://github.com/KhronosGroup/WebGL/pull/2665
    self.Fail('conformance2/textures/misc/tex-subimage3d-canvas-bug.html',
              ['linux'], bug=859400)
    self.Fail('conformance2/textures/misc/tex-subimage3d-canvas-bug.html',
              ['win', 'opengl', 'passthrough'], bug=859400)

    # Failing new test added in https://github.com/KhronosGroup/WebGL/pull/2658
    self.Fail('conformance2/transform_feedback/' +
              'same-buffer-two-binding-points.html', bug=866089)

    # Too slow (take about one hour to run)
    self.Skip('deqp/functional/gles3/builtinprecision/*.html', bug=619403)

    # Timing out on multiple platforms right now.
    self.Skip('conformance/glsl/bugs/sampler-array-struct-function-arg.html',
        bug=757097)

    # Flakes heavily on many OpenGL configurations
    self.Fail('conformance2/transform_feedback/too-small-buffers.html',
        ['no_angle'], bug=832238)
    self.Fail('conformance2/transform_feedback/too-small-buffers.html',
        ['opengl'], bug=832238)

    # Failing on Windows and Linux with NVIDIA GPUs and OpenGL driver.
    self.Fail('conformance/glsl/bugs/vector-scalar-arithmetic-inside-loop.html',
        ['nvidia'], bug=772651)
    self.Fail('conformance/glsl/bugs/' +
        'vector-scalar-arithmetic-inside-loop-complex.html',
        ['nvidia'], bug=772651)
    # All platforms.
    self.Fail('conformance2/glsl3/tricky-loop-conditions.html', bug=483282)
    self.Fail('conformance2/glsl3/array-length-side-effects.html',
        bug=2142) # angle bug ID

    # Need to implement new error semantics
    # https://github.com/KhronosGroup/WebGL/pull/2607
    self.Fail('conformance/rendering/draw-elements-out-of-bounds.html',
        bug=849572)

    # All non-ANGLE platforms.
    self.Fail('conformance2/attribs/' +
        'gl-bindAttribLocation-aliasing-inactive.html',
        ['no_angle'], bug=829541)

    # OffscreenCanvas.commit
    # TODO(fserb): Tests are not up to date with proposed API
    self.Fail('conformance2/offscreencanvas/methods-2.html', bug=838133)
    self.Fail('conformance2/offscreencanvas/methods-2-worker.html', bug=838133)
    self.Fail('conformance/offscreencanvas/' +
      'context-attribute-preserve-drawing-buffer.html', bug=838133)
    self.Fail('conformance/offscreencanvas/methods.html', bug=838133)
    self.Fail('conformance/offscreencanvas/methods-worker.html', bug=838133)

    # This test needs to be rewritten to measure its expected
    # performance; it's currently too flaky even on release bots.
    self.Skip('conformance/rendering/texture-switch-performance.html',
        bug=735483)
    self.Skip('conformance2/rendering/texture-switch-performance.html',
        bug=735483)

    self.Fail('conformance2/rendering/depth-stencil-feedback-loop.html',
        bug=660844) # WebGL 2.0.1
    self.Fail('conformance/rendering/rendering-sampling-feedback-loop.html',
        bug=660844) # WebGL 2.0.1
    self.Fail('conformance2/textures/misc/' +
        'integer-cubemap-specification-order-bug.html',
        bug=483282) # owner:cwallez, test might be buggy
    self.Fail('conformance/textures/misc/tex-sub-image-2d-bad-args.html',
        bug=625738)

    # Need to implement new lifetime/deletion semantics.
    self.Fail('conformance2/vertex_arrays/vertex-array-object.html', bug=739604)
    self.Fail('deqp/functional/gles3/lifetime.html', bug=898350)

    # Need to forbid generation of mipmaps for zero-sized textures.
    self.Fail('conformance2/textures/misc/tex-mipmap-levels.html', bug=898351)

    # The following actually passes on gl_passthrough and also Mac Intel with
    # command buffer.
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturelodoffset.html',
        bug=794335)

    self.Fail('conformance2/textures/misc/copy-texture-image-same-texture.html',
        ['mac', 'linux'], bug=809233)
    self.Fail('conformance2/textures/misc/copy-texture-image-same-texture.html',
        ['win', 'nvidia', 'opengl'], bug=809594)
    self.Fail('conformance2/uniforms/' +
        'incompatible-texture-type-for-sampler.html',
        bug=809237)

    self.Fail('conformance/glsl/bugs/assign-to-swizzled-twice-in-function.html',
        ['nvidia'], bug=798117)
    self.Fail('conformance/glsl/bugs/' +
        'in-parameter-passed-as-inout-argument-and-global.html',
        ['nvidia'], bug=792210)

    # Windows only.
    self.Fail('conformance2/buffers/uniform-buffers.html',
        ['win'], bug=757098)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
        ['win'], bug=2103) # angle bug ID
    self.Fail('conformance2/glsl3/array-initialize-with-same-name-array.html',
        ['win'], bug=757098)
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['win', 'd3d11'], bug=644740)
    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['win', 'd3d11'], bug=705865)
    self.Flaky('conformance2/textures/svg_image/' +
        'tex-2d-rgb565-rgb-unsigned_short_5_6_5.html',
        ['win'], bug=736926)
    self.Fail('conformance2/uniforms/uniform-blocks-with-arrays.html',
        ['win'], bug=2103) # angle bug ID
    self.Fail('conformance2/rendering/' +
        'framebuffer-texture-changing-base-level.html',
        ['win'], bug=2291) # angle bug ID
    self.Fail('conformance/glsl/bugs/if-return-and-elseif.html',
        ['win'], bug=2325) # angle bug ID

    # Win / NVidia
    self.Flaky('deqp/functional/gles3/fbomultisample*',
        ['win', 'nvidia', 'd3d11'], bug=631317)
    self.Fail('conformance2/rendering/' +
        'draw-with-integer-texture-base-level.html',
        ['win', 'nvidia', 'd3d11'], bug=679639)
    self.Flaky('deqp/functional/gles3/textureshadow/*.html',
        ['win', 'nvidia', 'd3d11'], bug=735464)

    # Win / NVIDIA Quadro P400 / D3D11 flaky failures
    self.Fail('deqp/data/gles3/shaders/functions.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_lines.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_triangles.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_lines.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_triangles.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_lines.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_triangles.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_lines.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_triangles.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/interpolation_flat.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance2/textures/video/*',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance2/textures/image_bitmap_from_video/*',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance/extensions/oes-texture-half-float-with-video.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance2/rendering/attrib-type-match.html',
        ['win', ('nvidia', 0x1cb3), 'd3d11'], bug=782254)

    # WIN / OpenGL / NVIDIA failures
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rgb565-rgb-unsigned_byte.html',
        ['win', ('nvidia', 0x1cb3), 'opengl'], bug=781668)
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['win', ('nvidia', 0x1cb3), 'opengl'], bug=715001)
    self.Fail('conformance/textures/misc/texture-size.html',
        ['win', ('nvidia', 0x1cb3), 'opengl'], bug=703779)
    self.Skip('conformance2/rendering/blitframebuffer-size-overflow.html',
        ['win', 'nvidia', 'opengl', 'passthrough'], bug=830046)
    self.Flaky('conformance2/transform_feedback/switching-objects.html',
        ['win', 'nvidia', 'opengl', 'no_passthrough'], bug=832238)
    self.Flaky('deqp/data/gles3/shaders/conversions.html',
        ['win', 'nvidia', 'opengl', 'passthrough'], bug=887578)
    self.Flaky('deqp/functional/gles3/transformfeedback/*',
        ['win', ('nvidia', 0x1cb3), 'opengl'], bug=822733)

    # Win / AMD
    self.Fail('conformance2/rendering/blitframebuffer-stencil-only.html',
        ['win', 'amd', 'd3d11'], bug=483282) # owner:jmadill

    self.Flaky('deqp/functional/gles3/draw/draw_arrays_instanced.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('deqp/functional/gles3/draw/draw_elements.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('deqp/functional/gles3/draw/draw_range_elements.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('deqp/functional/gles3/draw/random.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('deqp/functional/gles3/samplerobject.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('deqp/functional/gles3/textureshadow/' +
        '2d_array_nearest_mipmap_linear_less.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance/glsl/bugs/logic-inside-block-without-braces.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance/glsl/functions/glsl-function-mod-float.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance/renderbuffers/' +
        'depth-renderbuffer-initialization.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance/renderbuffers/renderbuffer-initialization.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance2/glsl3/vector-dynamic-indexing.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance2/renderbuffers/' +
        'multisampled-depth-renderbuffer-initialization.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rgb9_e5-rgb-half_float.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rgb9_e5-rgb-float.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rgb32f-rgb-float.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance2/textures/misc/' +
        'copy-texture-image-webgl-specific.html',
        ['win', 'amd', 'd3d11'], bug=828984)
    self.Flaky('conformance2/textures/webgl_canvas/*', ['win', 'amd'],
        bug=878780)

    # Recent AMD drivers seem to have a regression with 3D textures.
    self.Fail('conformance2/textures/canvas_sub_rectangle/tex-3d-*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('conformance2/textures/image/tex-3d-*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('conformance2/textures/image_data/tex-3d-*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('conformance2/textures/misc/tex-unpack-params.html',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('conformance2/textures/video/tex-3d-*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('deqp/functional/gles3/shadertexturefunction/*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('deqp/functional/gles3/texturefiltering/3d_*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_texsubimage3d_*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_3d*',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_unpack_params.html',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage3d_unpack_params.html',
        ['win', 'amd', 'd3d11'], bug=2424) # ANGLE bug ID

    # Have seen this time out. Think it may be because it's currently
    # the first test that runs in the shard, and the browser might not
    # be coming up correctly.
    self.Flaky('deqp/functional/gles3/multisample.html',
        ['win', ('amd', 0x6613)], bug=687374)

    # Win / Intel
    self.Fail('conformance/rendering/rendering-stencil-large-viewport.html',
        ['win', 'intel', 'd3d11'], bug=782317)

    # Seems to cause the harness to fail immediately afterward
    self.Skip('conformance2/textures/video/tex-2d-rgba16f-rgba-half_float.html',
        ['win', 'intel', 'd3d11'], bug=648337)
    # TODO(kbr): re-enable after fix for http://crbug.com/898350
    # self.Flaky('deqp/functional/gles3/lifetime.html',
    #     ['win', 'intel', 'd3d11'], bug=620379)
    self.Flaky('deqp/functional/gles3/textureformat/unsized_3d.html',
        ['win', 'intel', 'd3d11'], bug=614418)

    # It's unfortunate that these suppressions need to be so broad, but it
    # looks like the D3D11 device can be lost spontaneously on this
    # configuration while running basically any test.
    self.Flaky('conformance/*', ['win', 'intel', 'd3d11'], bug=628395)
    self.Flaky('conformance2/*', ['win', 'intel', 'd3d11'], bug=628395)
    self.Flaky('deqp/*', ['win', 'intel', 'd3d11'], bug=628395)

    # Passthrough command decoder
    self.Fail('conformance/misc/webgl-specific-stencil-settings.html',
        ['passthrough'], bug=844349)

    # Passthrough command decoder / D3D11
    self.Fail('deqp/functional/gles3/shaderstruct.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    # Passthrough command decoder / OpenGL
    self.Fail('conformance2/misc/uninitialized-test-2.html',
        ['passthrough', 'opengl'], bug=602688)
    self.Fail('conformance2/rendering/draw-buffers-dirty-state-bug.html',
        ['passthrough', 'opengl'], bug=602688)
    self.Fail('conformance2/state/gl-get-calls.html', ['passthrough', 'opengl'],
        bug=602688)
    self.Fail('deqp/functional/gles3/integerstatequery.html',
        ['passthrough', 'opengl'], bug=602688)
    self.Fail('conformance/textures/canvas/' +
        'tex-2d-alpha-alpha-unsigned_byte.html',
        ['passthrough', 'opengl'], bug=602688)
    self.Fail('conformance/textures/canvas/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['passthrough', 'opengl'], bug=602688)
    self.Fail('deqp/functional/gles3/shadercommonfunction.html',
        ['passthrough', 'opengl'], bug=795030)
    self.Fail('deqp/functional/gles3/shaderpackingfunction.html',
        ['passthrough', 'opengl'], bug=794341)
    self.Fail('conformance2/rendering/attrib-type-match.html',
        ['passthrough', 'opengl'], bug=814905)

    # Passthrough command decoder / OpenGL / Windows
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['win', 'passthrough', 'opengl'], bug=835364)
    self.Flaky('conformance/renderbuffers/' +
        'depth-renderbuffer-initialization.html',
        ['win', 'passthrough', 'opengl'], bug=835364)

    # These Transform Feedback tests seem flaky on ANGLE/GL with passthrough.
    self.Flaky('conformance2/transform_feedback/switching-objects.html',
        ['passthrough', 'opengl'], bug=832238)

    # Passthrough command decoder / OpenGL / Intel
    self.Fail('conformance2/textures/video/tex-2d-rgb32f-rgb-float.html',
        ['passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance2/textures/video/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance/misc/uninitialized-test.html',
        ['passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance/textures/misc/texture-attachment-formats.html',
        ['passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance/renderbuffers/framebuffer-state-restoration.html',
        ['passthrough', 'opengl', 'intel'], bug=602688)

    # Passthrough command decoder / Windows / OpenGL / Intel
    # This case causes no-over-optimization-on-uniform-array fail.
    self.Skip('conformance/ogles/GL/gl_FragCoord/gl_FragCoord_001_to_003.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=884210)
    self.Flaky('conformance/glsl/variables/gl-pointcoord.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=854100)
    self.Fail('conformance2/textures/misc/copy-texture-image-same-texture.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=809594)
    self.Fail('conformance2/renderbuffers/' +
        'multisampled-depth-renderbuffer-initialization.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=2760) # ANGLE bug
    self.Fail('conformance/glsl/constructors/glsl-construct-mat2.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance2/textures/misc/texture-npot.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance2/textures/misc/npot-video-sizing.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance2/glsl3/' +
        'vector-dynamic-indexing-swizzled-lvalue.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing.html',
        ['win', 'passthrough', 'opengl', 'intel'], bug=602688)
    self.Fail('deqp/functional/gles3/shaderbuiltinvar.html', # ANGLE bug
        ['win', 'passthrough', 'opengl', 'intel'], bug=2880)

    # Passthrough command decoder / Linux / OpenGL / NVIDIA
    self.Fail('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-luminance_alpha-luminance_alpha-unsigned_byte.html',
        ['linux', 'passthrough', 'opengl', 'nvidia'], bug=773861)
    self.Fail('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-luminance-luminance-unsigned_byte.html',
        ['linux', 'passthrough', 'opengl', 'nvidia'], bug=773861)
    self.Fail('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['linux', 'passthrough', 'opengl', 'nvidia'], bug=766918)
    self.Fail('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgb-rgb-unsigned_short_5_6_5.html',
        ['linux', 'passthrough', 'opengl', 'nvidia'], bug=766918)
    self.Flaky('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgb565-rgb-unsigned_short_5_6_5.html',
        ['linux', 'passthrough', 'opengl', 'nvidia'], bug=766918)
    self.Fail('deqp/functional/gles3/shaderoperator/common_functions_*.html',
        ['linux', 'passthrough', 'opengl', 'nvidia'], bug=793055)

    # Passthrough command decoder / Linux / OpenGL / Intel
    self.Flaky('conformance/extensions/webgl-compressed-texture-s3tc.html',
        ['linux', 'passthrough', 'opengl', 'intel'], bug=872302)
    self.Fail('conformance2/renderbuffers/' +
        'multisampled-depth-renderbuffer-initialization.html',
        ['linux', 'passthrough', 'opengl', 'intel'], bug=2760) # ANGLE bug
    self.Fail('conformance2/renderbuffers/' +
        'multisampled-stencil-renderbuffer-initialization.html',
        ['linux', 'passthrough', 'opengl', 'intel'], bug=2760) # ANGLE bug
    # TODO(kbr): re-enable after crbug.com/898351 is fixed
    # self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
    #     ['linux', 'passthrough', 'opengl', 'intel'], bug=2761) # ANGLE bug

    # Regressions in 10.12.4.
    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['sierra', 'intel'], bug=705865)
    # TODO(kbr): re-enable after crbug.com/898351 is fixed
    # self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
    #     ['sierra', 'intel'], bug=705865)
    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['sierra', 'amd'], bug=870856)
    # TODO(kbr): re-enable after crbug.com/898351 is fixed
    # self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
    #     ['sierra', 'amd'], bug=870856)

    # Regressions in 10.13
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex2d_00.html',
        ['highsierra', 'mojave', ('intel', 0xa2e)], bug=774826)
    self.Fail('deqp/functional/gles3/fboinvalidate/format_00.html',
        ['highsierra', 'mojave', ('intel', 0xa2e)], bug=774826)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_05.html',
        ['highsierra', 'mojave', ('intel', 0xa2e)], bug=774826)
    self.Fail('conformance2/glsl3/array-assign.html',
        ['highsierra', 'mojave', ('nvidia', 0xfe9)], bug=774827)
    self.Fail('deqp/functional/gles3/fborender/resize_03.html',
        ['highsierra', 'mojave', ('nvidia', 0xfe9)], bug=774827)
    self.Fail('deqp/functional/gles3/shaderindexing/mat_00.html',
        ['highsierra', 'mojave', ('nvidia', 0xfe9)], bug=774827)
    self.Fail('deqp/functional/gles3/shaderindexing/mat_02.html',
        ['highsierra', 'mojave', ('nvidia', 0xfe9)], bug=774827)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_00.html',
        ['highsierra', 'mojave', ('nvidia', 0xfe9)], bug=774827)

    # Fails on multiple GPU types.
    self.Fail('conformance/glsl/misc/fragcolor-fragdata-invariant.html',
        ['mac'], bug=844311)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-swizzled-lvalue.html',
        ['mac'], bug=709351)
    self.Fail('conformance2/rendering/' +
        'framebuffer-completeness-unaffected.html',
        ['mac', 'nvidia', 'intel'], bug=630800)
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['mac', 'nvidia', 'intel'], bug=630800)
    self.Fail('deqp/functional/gles3/negativeshaderapi.html',
        ['mac', 'amd', 'intel'], bug=811614)

    # Mac Retina NVIDIA
    self.Fail('deqp/functional/gles3/shaderindexing/mat_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=728271)
    self.Fail('deqp/functional/gles3/shaderindexing/tmp.html',
        ['mac', ('nvidia', 0xfe9)], bug=728271)
    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['mac', ('nvidia', 0xfe9)], bug=641209)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_04.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance/attribs/gl-disabled-vertex-attrib.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Flaky(
        'conformance/extensions/webgl-compressed-texture-size-limit.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance/programs/' +
        'gl-bind-attrib-location-long-names-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance/programs/gl-bind-attrib-location-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance2/glsl3/loops-with-side-effects.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance2/textures/misc/tex-input-validation.html',
        ['mac', ('nvidia', 0xfe9), 'no_angle'], bug=483282)
    self.Flaky('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba16f-rgba-half_float.html',
        ['mac', ('nvidia', 0xfe9)], bug=682834)
    self.Fail('conformance/glsl/bugs/init-array-with-loop.html',
        ['mac', ('nvidia', 0xfe9)], bug=784817)

    self.Fail('deqp/functional/gles3/draw/random.html',
        ['sierra', ('nvidia', 0xfe9)], bug=716652)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/pixelbufferobject.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/negativevertexarrayapi.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/shaderindexing/varying.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_2d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_2d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_2d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_2d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_02.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_03.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_04.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_2d_array_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_2d_array_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_3d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_3d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage3d_pbo_3d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage3d_pbo_3d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)

    self.Fail('deqp/functional/gles3/fragmentoutput/array.fixed.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/basic.fixed.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/random_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/random_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/random_02.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/fbocolorbuffer/clear.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex2d_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex2darray_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex3d_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/texcube_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/blend.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/draw/draw_arrays.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_arrays_instanced.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_elements.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_elements_instanced.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_range_elements.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/fboinvalidate/format_02.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/negativeshaderapi.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Flaky('deqp/functional/gles3/vertexarrays/' +
        'multiple_attributes.output.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)

    self.Fail('conformance2/uniforms/draw-with-uniform-blocks.html',
        ['mac', ('nvidia', 0xfe9)], bug=795052)

    # When these fail on this configuration, they fail multiple times in a row.
    self.Fail('deqp/functional/gles3/shaderoperator/*',
        ['mac', 'nvidia'], bug=756537)

    # Mac AMD
    # TODO(kbr): uncomment the following two exepectations after test
    # has been made more robust.
    # self.Fail('conformance/rendering/texture-switch-performance.html',
    #     ['mac', 'amd'], bug=735483)
    # self.Fail('conformance2/rendering/texture-switch-performance.html',
    #     ['mac', 'amd'], bug=735483)

    # The following two failures are a regression in the Mac AMD
    # OpenGL driver on 10.13.6 specifically. Unfortunately when the
    # tests fail, they fail three times in a row, so we must mark them
    # failing rather than flaky.
    # TODO(kbr): re-enable after crbug.com/898351 is fixed
    # self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
    #     ['highsierra', 'amd'], bug=870856)
    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['highsierra', 'amd'], bug=870856)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_lines.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_points.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_triangles.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_separate_lines.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_separate_points.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_separate_triangles.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_lines.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_points.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_triangles.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_lines.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_points.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_triangles.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'interpolation_centroid.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'interpolation_flat.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'interpolation_smooth.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'point_size.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'position.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_lines.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_points.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_triangles.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_lines.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_points.html',
        ['sierra', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_triangles.html',
        ['sierra', 'amd'], bug=483282)

    self.Flaky('deqp/functional/gles3/shaderindexing/mat_00.html',
        ['mac', 'amd'], bug=751254)
    self.Flaky('deqp/functional/gles3/shaderindexing/mat_01.html',
        ['mac', 'amd'], bug=636648)
    self.Flaky('deqp/functional/gles3/shaderindexing/mat_02.html',
        ['mac', 'amd'], bug=644360)
    self.Flaky('deqp/functional/gles3/shaderindexing/tmp.html',
        ['mac', 'amd'], bug=659871)

    # These seem to be provoking intermittent GPU process crashes on
    # the MacBook Pros with AMD GPUs.
    self.Flaky('deqp/functional/gles3/texturefiltering/*',
        ['mac', 'amd'], bug=663601)
    self.Flaky('deqp/functional/gles3/textureshadow/*',
        ['mac', 'amd'], bug=663601)
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'teximage2d_unpack_params.html',
        ['mac', 'amd'], bug=679058)

    self.Fail('conformance2/rendering/clipping-wide-points.html',
        ['mac', 'amd'], bug=642822)

    # Mac Intel
    self.Fail('conformance/rendering/canvas-alpha-bug.html',
        ['mac', ('intel', 0x0a2e)], bug=886970)
    self.Fail('conformance2/rendering/framebuffer-texture-level1.html',
        ['mac', 'intel'], bug=680278)
    self.Fail('conformance2/textures/misc/angle-stuck-depth-textures.html',
        ['mac', 'no_passthrough', 'intel'], bug=679692)
    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['mac', 'intel'], bug=641209)
    self.Fail('deqp/functional/gles3/texturefiltering/2d_combinations_01.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/' +
        'cube_combinations_01.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/' +
        '2d_array_combinations_01.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/3d_combinations_06.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/3d_combinations_07.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/3d_combinations_08.html',
        ['mac', 'intel'], bug=606074)

    self.Fail('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['mac', 'intel'], bug=483282)

    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturelod.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturegrad.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojgrad.html',
        ['mac', 'intel'], bug=483282)

    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-r8ui-red_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)

    self.Fail('conformance2/textures/image_data/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['mac', 'intel'], bug=665197)
    self.Fail('conformance2/textures/image_data/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['mac', 'intel'], bug=665197)
    self.Fail('conformance2/textures/image_data/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['mac', 'intel'], bug=665197)

    self.Fail('conformance2/textures/misc/' +
        'integer-cubemap-texture-sampling.html',
        ['mac', 'intel'], bug=658930)

    self.Fail('conformance2/renderbuffers/' +
        'multisampled-depth-renderbuffer-initialization.html',
        ['mac', 'intel'], bug=731877)

    self.Fail('conformance/rendering/rendering-stencil-large-viewport.html',
        ['mac', 'intel'], bug=782317)

    # Linux only.
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_byte.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgb-rgb-unsigned_byte.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgb-rgb-unsigned_short_5_6_5.html',
               ['linux'], bug=627525)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-nv-driver-bug.html',
        ['linux'], bug=483282)

    # Linux Multi-vendor failures.
    self.Skip('deqp/data/gles3/shaders/qualification_order.html',
        ['linux', 'amd', 'intel'], bug=483282)
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['linux', 'amd', 'intel'], bug=618447)
    self.Fail('conformance2/rendering/clipping-wide-points.html',
        ['linux', 'amd', 'intel'], bug=662644) # WebGL 2.0.1

    # Linux NVIDIA
    # This test is flaky both with and without ANGLE.
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['linux', 'nvidia'], bug=618447)
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_cube.html',
        ['linux', 'nvidia'], bug=618447)
    self.Fail('conformance/glsl/bugs/unary-minus-operator-float-bug.html',
        ['linux', 'nvidia'], bug=672380)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-swizzled-lvalue.html',
        ['linux', 'nvidia'], bug=709351)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-3d-srgb8_alpha8-rgba-unsigned_byte.html',
        ['linux', 'nvidia'], bug=679677)
    self.Fail('conformance2/rendering/framebuffer-texture-level1.html',
        ['linux', 'nvidia', 'opengl'], bug=680278)
    self.Fail('conformance2/rendering/multisampling-fragment-evaluation.html',
        ['linux', 'nvidia', 'no_passthrough'], bug=682815)
    self.Fail('conformance2/textures/image/' +
        'tex-3d-rg8ui-rg_integer-unsigned_byte.html',
        ['linux', ('nvidia', 0xf02)], bug=680282)
    self.Flaky('conformance2/textures/image_bitmap_from_image_data/' +
        'tex-2d-srgb8-rgb-unsigned_byte.html',
        ['linux', 'no_passthrough', 'nvidia'], bug=694354)

    # Linux NVIDIA Quadro P400
    # This test causes a lost device and then the next test fails.
    self.Skip('conformance2/rendering/blitframebuffer-size-overflow.html',
        ['linux', ('nvidia', 0x1cb3)], bug=709320)
    # Observed flaky on Swarmed bots. Some of these were directly
    # observed, some not. We can't afford any flakes on the tryservers
    # so mark them all flaky.
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_lines.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_points.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_triangles.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'array_separate_lines.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'array_separate_points.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'array_separate_triangles.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_lines.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_points.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_triangles.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_lines.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_points.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_triangles.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'interpolation_centroid.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'interpolation_flat.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'interpolation_smooth.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'point_size.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'position.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_lines.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_points.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_triangles.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'random_separate_lines.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'random_separate_points.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)
    self.Flaky('deqp/functional/gles3/transformfeedback/' +
        'random_separate_triangles.html',
        ['linux', ('nvidia', 0x1cb3)], bug=780706)

    # Linux NVIDIA Quadro P400, OpenGL backend
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['linux', ('nvidia', 0x1cb3)], bug=715001)
    self.Fail('conformance/textures/misc/texture-size.html',
        ['linux', ('nvidia', 0x1cb3), 'opengl'], bug=703779)
    self.Fail('conformance/extensions/webgl-compressed-texture-size-limit.html',
        ['linux', ('nvidia', 0x1cb3), 'opengl'], bug=703779)
    self.Fail('conformance/textures/misc/texture-size-limit.html',
        ['linux', ('nvidia', 0x1cb3), 'opengl'], bug=703779)
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['linux', ('nvidia', 0x1cb3), 'opengl'], bug=703779)

    # Linux Intel
    self.Fail('conformance2/textures/misc/tex-subimage3d-pixel-buffer-bug.html',
       ['linux', 'intel'], bug=662644) # WebGL 2.0.1

    self.Fail('deqp/functional/gles3/shadertexturefunction/texturesize.html',
       ['linux', 'intel'], bug=666384)
    self.Fail('conformance2/textures/misc/tex-3d-mipmap-levels-intel-bug.html',
       ['linux', 'intel'], bug=666384)

    # Linux Intel with ANGLE only
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['linux', 'intel', 'opengl'], bug=598902)
    self.Fail('conformance2/rendering/blitframebuffer-filter-srgb.html',
        ['linux', 'intel', 'opengl'], bug=680276)
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['linux', 'intel', 'opengl'], bug=680276)

    # Linux Intel HD 530
    self.Fail('conformance/extensions/webgl-compressed-texture-astc.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('conformance2/rendering/blitframebuffer-filter-outofbounds.html',
        ['linux', 'no_passthrough', 'intel'], bug=680720)
    self.Fail('conformance2/rendering/blitframebuffer-filter-srgb.html',
        ['linux', 'intel', 'no_angle'], bug=680720)
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['linux', 'intel', 'no_angle'], bug=680720)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_00.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('conformance2/glsl3/' +
        'vector-dynamic-indexing-swizzled-lvalue.html',
        ['linux', 'intel'], bug=709874)

    # Linux AMD only.
    # It looks like AMD shader compiler rejects many valid ES3 semantics.
    self.Fail('conformance/glsl/misc/fragcolor-fragdata-invariant.html',
        ['linux', 'amd'], bug=844311)
    self.Fail('conformance2/attribs/gl-vertex-attrib-normalized-int.html',
        ['linux', 'amd'], bug=766776)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-swizzled-lvalue.html',
        ['linux', 'amd'], bug=709351)
    self.Fail('deqp/functional/gles3/multisample.html',
        ['linux', 'amd'], bug=617290)
    self.Fail('deqp/data/gles3/shaders/conversions.html',
        ['linux', 'amd'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/arrays.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/internalformatquery.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturestatequery.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/buffercopy.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/samplerobject.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderprecision_int.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturefiltering/3d*',
        ['linux', 'amd'], bug=606114)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texture.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texturegrad.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texelfetchoffset.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/vertexarrays/' +
        'single_attribute.first.html',
        ['linux', 'amd'], bug=694877)

    self.Fail('deqp/functional/gles3/negativetextureapi.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/array_separate*.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('conformance2/misc/uninitialized-test-2.html',
        ['linux', 'no_passthrough', 'amd'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-from-fbo-test.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/rendering/blitframebuffer-filter-srgb.html',
        ['linux', 'amd'], bug=634525)
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['linux', 'amd'], bug=662644) # WebGL 2.0.1
    self.Fail('conformance2/renderbuffers/framebuffer-texture-layer.html',
        ['linux', 'amd'], bug=295792)
    # TODO(kbr): re-enable after crbug.com/898351 is fixed
    # self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
    #     ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/textures/misc/copy-texture-image-luma-format.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/vertex_arrays/' +
        'vertex-array-object-and-disabled-attributes.html',
        ['linux', 'amd'], bug=899754)

    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_03.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_04.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_params.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_depth_pbo.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_copyteximage2d.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_03.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_04.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage2d_format_depth_stencil.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_2d_array_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_2d_array_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_2d_array_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_03.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_depth_stencil.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_size.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/vertexarrays/' +
        'single_attribute.output_type.unsigned_int.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/draw/*.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/textureshadow/*.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/mul_dynamic_highp.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/mul_dynamic_lowp.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/mul_dynamic_mediump.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/pre_decrement.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_00.html',
        ['linux', 'amd'], bug=658832)

    self.Fail('deqp/functional/gles3/shaderoperator/unary_operator_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderoperator/unary_operator_02.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('conformance2/glsl3/vector-dynamic-indexing.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-pack-parameters.html',
        ['linux', 'amd', 'no_angle'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-unpack-params.html',
        ['linux', 'amd', 'no_angle'], bug=483282)
    # TODO(kbr): re-enable after next conformance roll. crbug.com/736499
    # self.Fail('conformance2/extensions/ext-color-buffer-float.html',
    #     ['linux', 'amd'], bug=633022)
    self.Fail('conformance2/rendering/blitframebuffer-filter-outofbounds.html',
        ['linux', 'no_passthrough', 'amd'], bug=655147)

    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['linux', 'amd'], bug=705865)
    self.Fail('conformance2/textures/image/' +
        'tex-2d-r11f_g11f_b10f-rgb-float.html',
        ['linux', 'amd'], bug=705865)

    # Uniform buffer related failures
    self.Fail('deqp/functional/gles3/uniformbuffers/single_struct_array.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/single_nested_struct.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/' +
        'single_nested_struct_array.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/multi_basic_types.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/multi_nested_struct.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/random.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/buffers/uniform-buffers.html',
        ['linux', 'amd'], bug=658842)
    self.Fail('conformance2/rendering/uniform-block-buffer-size.html',
        ['linux', 'amd'], bug=658844)
    self.Fail('conformance2/uniforms/uniform-blocks-with-arrays.html',
        ['linux', 'amd'], bug=2103) # angle bug ID
    self.Fail('conformance2/uniforms/simple-buffer-change.html',
        ['linux', 'amd', 'no_angle'], bug=809595)

    # Linux AMD R7 240
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba16f-rgba-float.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba16f-rgba-half_float.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba32f-rgba-float.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba4-rgba-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba4-rgba-unsigned_short_4_4_4_4.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-3d-rgb10_a2-rgba-unsigned_int_2_10_10_10_rev.html',
        ['linux', ('amd', 0x6613)], bug=847217)
    self.Fail('conformance2/textures/video/tex-2d-rg32f-rg-float.html',
        ['linux', ('amd', 0x6613)], bug=847217)
    self.Fail('conformance2/textures/image_data/' +
        'tex-3d-rgb32f-rgb-float.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_data/' +
        'tex-3d-rgb565-rgb-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_data/' +
        'tex-3d-rgb565-rgb-unsigned_short_5_6_5.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_data/' +
        'tex-3d-rgb5_a1-rgba-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/misc/' +
        'tex-image-with-bad-args-from-dom-elements.html',
        ['linux', ('amd', 0x6613), 'no_angle'], bug=832864)
    self.Fail('conformance2/transform_feedback/switching-objects.html',
        ['linux', ('amd', 0x6613), 'no_angle'], bug=696345)

    self.Fail('conformance2/buffers/get-buffer-sub-data-validity.html',
        ['linux', ('amd', 0x6613)], bug=851159)

    # Conflicting expectations to test that the
    # "Expectations have no collisions" unittest works.
    # page_name = 'conformance/glsl/constructors/glsl-construct-ivec4.html'

    # Conflict when all conditions match
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])

    # Conflict when all conditions match (and different sets)
    # self.Fail(page_name,
    #     ['linux', 'win', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['linux', 'mac', ('nvidia', 0x1), 'amd', 'debug', 'opengl'])

    # Conflict with one aspect not specified
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])

    # Conflict with one aspect not specified (in both conditions)
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])

    # Conflict even if the GPU is specified in a device ID
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', 'nvidia', 'debug'])

    # Test there are no conflicts between two different devices
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x2), 'debug'])

    # Test there are no conflicts between two devices with different vendors
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('amd', 0x1), 'debug'])

    # Conflicts if there is a device and nothing specified for the other's
    # GPU vendors
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', 'debug'])

    # Test no conflicts happen when only one aspect differs
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['win', ('nvidia', 0x1), 'debug', 'opengl'])

    # Conflicts if between a generic os condition and a specific version
    # self.Fail(page_name,
    #     ['xp', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['win', ('nvidia', 0x1), 'debug', 'opengl'])
