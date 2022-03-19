// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OS tags.
const linux = 'linux';
const mac = 'mac';
const win = 'win';
const bigsur = 'bigsur';  // Mac 11.

// GPU tags.
const amd = 'amd';
const intel = 'intel';
const intel_hd630 = 'intel-0x5912';
const intel_uhd630 = 'intel-0x3e92';
const nvidia = 'nvidia';

// Expected results.
const Failure = 'Failure';
const RetryOnFailure = 'RetryOnFailure';
const Skip = 'Skip';
const Slow = 'Slow';

// Multiple expectations related to the same bug with the same tags.
// Format:
// {
//   {
//     b: bug (string),
//     t: [tag1, tag2, ...] (strings),
//     e: [expected_result] (strings),
//     w: run_in_worker (optional, boolean),
//     q: [query1, query2, ...] (strings)
//   }
// }
var expectation_groups = [
  //
  // Dawn bugs
  //
  {
    // Handling of base_vertex base_instance is not implemented for indirect
    // draws on D3D12.
    b: 'crbug.com/dawn/548',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,rendering,draw:arguments:indirect=true;*',
      'webgpu:shader,execution,robust_access_vertex:vertex_buffer_access:indexed=true;indirect=true;drawCallTestParameter="baseVertex";*'
    ],
  },
  {
    // Failures because stencil8 and depth16unorm aren't implemented.
    b: 'crbug.com/dawn/570',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,image_copy,buffer_related:bytes_per_row_alignment:format="depth16unorm";*',
      // Original query conflicts with a query for crbug.com/dawn/1125, so
      // specify some extra query parameters to avoid that.
      // 'webgpu:api,operation,rendering,depth_clip_clamp:depth_clamp_and_clip:format="depth16unorm";*',
      // Started not finding any cases.
      /*'webgpu:api,operation,rendering,depth_clip_clamp:depth_clamp_and_clip:format="depth16unorm";clampDepth=true;writeDepth=true;*',
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_clamp_and_clip:format="depth16unorm";clampDepth=false;writeDepth=false;*',
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_clamp_and_clip:format="depth16unorm";clampDepth=true;writeDepth=false;*',*/
      // Completely overlaps other queries for depth_test_input_clamped.
      // 'webgpu:api,operation,rendering,depth_clip_clamp:depth_test_input_clamped:format="depth16unorm";*',
    ],
  },
  {
    // Failures because stencil8 and depth16unorm aren't implemented.
    // These would normally be restricted to stencil8 and depth16unorm formats,
    // but some are currently handled as subcases within a single test, so the
    // entire test has to be disabled in those cases.
    b: 'crbug.com/dawn/666',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,encoding,cmds,copyTextureToTexture:texture_format_compatibility:*',
      // These currently conflict with a broader query from crbug.com/dawn/1071,
      // so they're handled with more platform-specific tags below.
      //'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      //'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:format="stencil8";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:format="depth16unorm";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:format="depth24unorm-stencil8";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:format="depth32float-stencil8";*',
      // TODO: Figure out why expectations for this aren't applying, probably
      // due to issues with %20 encoding/decoding.
      'webgpu:api,validation,attachment_compatibility:render_pass_and_bundle,depth_format:*',
      'webgpu:api,validation,attachment_compatibility:render_pass_or_bundle_and_pipeline,depth_format:*',
      'webgpu:api,validation,createRenderPipeline:color_formats_must_be_renderable:format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:color_formats_must_be_renderable:format="depth16unorm";*',
      'webgpu:api,validation,encoding,cmds,copyTextureToTexture:copy_aspects:format="stencil8";*',
      'webgpu:api,validation,encoding,cmds,copyTextureToTexture:copy_aspects:format="depth16unorm";*',
      'webgpu:api,validation,encoding,cmds,copyTextureToTexture:depth_stencil_copy_restrictions:format="stencil8";*',
      'webgpu:api,validation,encoding,cmds,copyTextureToTexture:depth_stencil_copy_restrictions:format="depth16unorm";*',
      // TODO: Figure out why the broader query still doesn't apply to any cases
      // 'webgpu:api,validation,queue,copyToTexture,ImageBitmap:destination_texture,format:format="stencil8";*',
      // 'webgpu:api,validation,queue,copyToTexture,ImageBitmap:destination_texture,format:format="depth16unorm";*',
      'webgpu:api,validation,encoding,cmds,buffer_texture_copies:depth_stencil_format,copy_usage_and_aspect:format="stencil8";*',
      'webgpu:api,validation,encoding,cmds,buffer_texture_copies:depth_stencil_format,copy_usage_and_aspect:format="depth16unorm";*',
      'webgpu:api,validation,encoding,cmds,buffer_texture_copies:depth_stencil_format,copy_buffer_offset:format="depth16unorm";*',
      'webgpu:api,validation,encoding,cmds,buffer_texture_copies:depth_stencil_format,copy_buffer_offset:format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:bound_on_bytes_per_row:method="CopyB2T";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:bound_on_bytes_per_row:method="CopyB2T";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:bound_on_bytes_per_row:method="CopyT2B";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:bound_on_bytes_per_row:method="CopyT2B";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:bound_on_bytes_per_row:method="WriteTexture";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:bound_on_bytes_per_row:method="WriteTexture";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:offset_alignment:method="CopyB2T";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:offset_alignment:method="CopyB2T";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:offset_alignment:method="CopyT2B";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:offset_alignment:method="CopyT2B";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:offset_alignment:method="WriteTexture";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:offset_alignment:method="WriteTexture";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:required_bytes_in_copy:method="CopyB2T";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:required_bytes_in_copy:method="CopyB2T";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:required_bytes_in_copy:method="CopyT2B";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:required_bytes_in_copy:method="CopyT2B";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:required_bytes_in_copy:method="WriteTexture";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:required_bytes_in_copy:method="WriteTexture";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:rows_per_image_alignment:method="CopyB2T";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:rows_per_image_alignment:method="CopyB2T";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:rows_per_image_alignment:method="CopyT2B";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:rows_per_image_alignment:method="CopyT2B";format="stencil8";*',
      'webgpu:api,validation,image_copy,layout_related:rows_per_image_alignment:method="WriteTexture";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,layout_related:rows_per_image_alignment:method="WriteTexture";format="stencil8";*',
      'webgpu:api,validation,image_copy,buffer_related:bytes_per_row_alignment:format="stencil8";*',
      'webgpu:api,validation,capability_checks,features,texture_formats:depth_stencil_state:format="depth24unorm-stencil8";*',
      'webgpu:api,validation,capability_checks,features,texture_formats:depth_stencil_state:format="depth32float-stencil8";*',
      'webgpu:api,validation,capability_checks,features,texture_formats:render_bundle_encoder_descriptor_depth_stencil_format:format="depth24unorm-stencil8";*',
      'webgpu:api,validation,capability_checks,features,texture_formats:render_bundle_encoder_descriptor_depth_stencil_format:format="depth32float-stencil8";*',
      'webgpu:api,validation,capability_checks,features,texture_formats:texture_descriptor:format="depth24unorm-stencil8";*',
      'webgpu:api,validation,capability_checks,features,texture_formats:texture_descriptor:format="depth32float-stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,depth_aspect,depth_test:isAsync=false;format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,depth_aspect,depth_test:isAsync=true;format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,depth_aspect,depth_write:isAsync=false;format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,depth_aspect,depth_write:isAsync=true;format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,format:isAsync=false;format="stencil8"',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,format:isAsync=true;format="stencil8"',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,stencil_aspect,stencil_test:isAsync=false;format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,stencil_aspect,stencil_test:isAsync=true;format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,stencil_aspect,stencil_write:isAsync=false;format="stencil8";*',
      'webgpu:api,validation,createRenderPipeline:depth_stencil_state,stencil_aspect,stencil_write:isAsync=true;format="stencil8";*',
      'webgpu:api,validation,texture,destroy:submit_a_destroyed_texture_as_attachment:depthStencilTextureAspect="stencil-only";*',
    ],
  },
  {
    // This was originally part of the larger crbug.com/dawn/666 group, but had
    // to be split out to avoid conflicts with crbug.com/dawn/1071 queries.
    b: 'crbug.com/dawn/666',
    t: [mac, amd],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
    ],
  },
  {
    // This was originally part of the larger crbug.com/dawn/666 group, but had
    // to be split out to avoid conflicts with crbug.com/dawn/1071 queries.
    b: 'crbug.com/dawn/666',
    t: [mac, nvidia],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
    ],
  },
  {
    // This was originally part of the larger crbug.com/dawn/666 group, but had
    // to be split out to avoid conflicts with crbug.com/dawn/1071 queries.
    b: 'crbug.com/dawn/666',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
    ],
  },
  {
    // This was originally part of the larger crbug.com/dawn/666 group, but had
    // to be split out to avoid conflicts with crbug.com/dawn/1071 queries.
    b: 'crbug.com/dawn/666',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=1;dimension="2d";format="stencil8";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="depth16unorm";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";depthOrArrayLayers=3;dimension="2d";format="stencil8";*',
    ],
  },
  // Currently conflicts with a broader query for crbug.com/dawn/1071, handled
  // in more specific queries below.
  /*
  {
    // Failures because stencil8 and depth16unorm aren't implemented.
    b: 'crbug.com/dawn/666',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
    ],
    w: true,
  },
  */
  {
    b: 'crbug.com/dawn/666',
    t: [mac, amd],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
    ],
    w: true,
  },
  {
    b: 'crbug.com/dawn/666',
    t: [mac, nvidia],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
    ],
    w: true,
  },
  {
    b: 'crbug.com/dawn/666',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
    ],
    w: true,
  },
  {
    b: 'crbug.com/dawn/666',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="depth16unorm";*',
    ],
    w: true,
  },
  {
    // maxArrayLayoutCount limit should be 256 instead of 2048.
    // These queries should be restricted to the following once they are no
    // longer subcases.
    // 'webgpu:api,validation,createTexture:texture_size,2d_texture,compressed_format:size=[4,4,2047];*',
    // 'webgpu:api,validation,createTexture:texture_size,2d_texture,compressed_format:size=[4,4,2048];*',
    // 'webgpu:api,validation,createTexture:texture_size,2d_texture,compressed_format:size=[4,4,2049];*',
    // 'webgpu:api,validation,createTexture:texture_size,2d_texture,uncompressed_format:size=[1,1,2047];*',
    // 'webgpu:api,validation,createTexture:texture_size,2d_texture,uncompressed_format:size=[1,1,2048];*',
    // 'webgpu:api,validation,createTexture:texture_size,2d_texture,uncompressed_format:size=[1,1,2049];*',
    b: 'crbug.com/dawn/685',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,createTexture:texture_size,2d_texture,compressed_format:*',
      'webgpu:api,validation,createTexture:texture_size,2d_texture,uncompressed_format:*',
    ],
  },
  {
    // Failures because stencil8 and depth16unorm aren't implemented.
    b: 'crbug.com/dawn/690',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="stencil8";*',
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="depth16unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="depth24plus-stencil8";*',
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="depth24unorm-stencil8";*',
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="depth32float-stencil8";*',
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="depth32float";copyMethod="CopyT2B";aspect="depth-only"',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="stencil8";*',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth16unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth24plus-stencil8";*',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth24unorm-stencil8";*',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth32float-stencil8";copyMethod="CopyT2B";aspect="depth-only"',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth32float-stencil8";aspect="stencil-only";*',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth32float";copyMethod="CopyT2B";aspect="depth-only"',
      // TODO: Figure out why the broader query doesn't find anything
      // 'webgpu:api,operation,command_buffer,copyTextureToTexture:copy_stencil_aspect:format="stencil8";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:copy_depth_stencil:format="depth16unorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:copy_depth_stencil:format="stencil8";*',
    ],
  },
  // TODO: Figure out why these queries don't find any cases.
  /*
  {
    b: 'crbug.com/dawn/704',
    t: [mac, intel],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_with_stencil_aspect:*',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_copy_with_stencil_aspect:*',
    ],
  },
  */
  {
    // baseVertex is always 0 for drawIndirect.
    b: 'crbug.com/dawn/722',
    t: [mac, intel],
    e: [Failure],
    q: [
      'webgpu:api,operation,rendering,draw:arguments:indirect=true;base_vertex=9;*',
    ],
  },
  {
    b: 'crbug.com/dawn/746',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,operation,shader_module,compilation_info:offset_and_length:valid=false;unicode=true',
    ],
  },
  {
    b: 'crbug.com/dawn/759',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,rendering,draw:vertex_attributes,basic:*',
    ],
  },
  {
    // Dawn validation requires that aspects of attachments is "all", which the
    // tests don't do.
    // See also crbug.com/dawn/603 for missing resource state
    // D3D12_RESOURCE_STATE_DEPTH_WRITE that happens on the same tests.
    b: 'crbug.com/dawn/812',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="DepthTest";format="depth24plus-stencil8";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="StencilTest";format="depth24plus-stencil8";*',
      'webgpu:api,validation,resource_usages,texture,in_pass_encoder:subresources_and_binding_types_combination_for_aspect:format="depth24plus-stencil8";*'
    ],
  },
  {
    // Depth/stencil textures with multiple mip levels don't clear properly on
    // Mac Intel. (By default they are disabled behind disallow_unsafe_apis.)
    // May be restrictable to Metal.
    b: 'crbug.com/dawn/838',
    t: [mac],
    e: [Failure],
    q: [
      // Conflicts with an identical query from crbug.com/dawn/812.
      // 'webgpu:api,validation,resource_usages,texture,in_pass_encoder:subresources_and_binding_types_combination_for_aspect:format="depth24plus-stencil8";*',
      // TODO: Figure out why the broader query doesn't find anything.
      // 'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_upload_to_stencil_aspect:stencilFormat="depth24plus-stencil8";*',
      // Conflicts with broader query from crbug.com/dawn/690.
      // 'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="depth24plus-stencil8";*',
      // TODO: Figure out why the broader  query doesn't find anything.
      // 'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_upload_to_stencil_aspect:stencilFormat="depth24plus-stencil8";*',
      // Conflicts with broader query from crbug.com/dawn/690.
      //'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth24plus-stencil8";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:format="depth32float";*',
    ],
  },
  {
    // Dawn implements validation of the limit at createShaderModule time, while
    // the CTS checks at createRenderPipeline time.
    b: 'crbug.com/dawn/986',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,vertex_state:vertex_shader_input_location_limit:*',
    ],
  },
  {
    // The D3D12 debug layers produce and incorrect warning: Missing State:
    // 0x1000: D3D12_RESOURCE_STATE_RESOLVE_DEST
    b: 'crbug.com/dawn/988',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pass,resolve:render_pass_resolve:*',
    ],
  },
  {
    b: 'crbug.com/dawn/995',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,buffer,create:createBuffer_invalid_and_oom:*',
    ],
  },
  {
    b: 'crbug.com/dawn/999',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,operation,buffers,map_oom:mappedAtCreation,smaller_getMappedRange:*',
      'webgpu:api,operation,buffers,map_oom:mappedAtCreation,full_getMappedRange:*',
    ],
  },
  {
    b: 'crbug.com/dawn/1002',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,buffer,destroy:error_buffer:*',
    ],
  },
  {
    // Precision. Need a better way to compare expected values.
    b: 'crbug.com/dawn/1003',
    t: null,
    e: [Failure],
    q: [
      'webgpu:util,texture,texel_data:unorm_texel_data_in_shader:format="rgba8unorm-srgb";*',
      'webgpu:util,texture,texel_data:unorm_texel_data_in_shader:format="bgra8unorm-srgb";*',
      'webgpu:util,texture,texel_data:ufloat_texel_data_in_shader:format="rg11b10ufloat";*',
      'webgpu:util,texture,texel_data:ufloat_texel_data_in_shader:format="rgb9e5ufloat";*',
    ],
  },
  {
    // Precision. Need a better way to compare expected values.
    b: 'crbug.com/dawn/1003',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:util,texture,texel_data:unorm_texel_data_in_shader:format="rgb10a2unorm";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:readMethod="Sample";format="rg11b10ufloat";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:readMethod="Sample";format="rgb9e5ufloat";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgb9e5ufloat";*',
    ],
  },
  {
    b: 'crbug.com/dawn/1003',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgb9e5ufloat";*',
    ],
  },
  {
    // Failures because readonly storage textures have been removed.
    b: 'crbug.com/dawn/1025',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,resource_usages,texture,in_pass_encoder:unused_bindings_in_pipeline:*',
    ],
  },
  {
    b: 'crbug.com/dawn/1046',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:shader,execution,robust_access_vertex:vertex_buffer_access:indexed=false;indirect=true;drawCallTestParameter="firstVertex";type="float32x4";*',
      'webgpu:shader,execution,robust_access_vertex:vertex_buffer_access:indexed=false;indirect=true;drawCallTestParameter="instanceCount";type="float32x4";*',
      'webgpu:shader,execution,robust_access_vertex:vertex_buffer_access:indexed=false;indirect=true;drawCallTestParameter="vertexCount";type="float32x4";*',
      'webgpu:shader,execution,robust_access_vertex:vertex_buffer_access:indexed=true;indirect=false;drawCallTestParameter="baseVertex";type="float32x4";*',
      'webgpu:shader,execution,robust_access_vertex:vertex_buffer_access:indexed=true;indirect=false;drawCallTestParameter="vertexCountInIndexBuffer";type="float32x4";*',
    ],
  },
  {
    // The copyTextureToTexture tests should allow information loss caused by
    // some bit patterns having the same value.
    b: 'crbug.com/dawn/1047',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="r8snorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg8snorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba8snorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="r8snorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rg8snorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgba8snorm";*',
    ],
  },
  {
    // Unexpected result. Possibly due to using dst-alpha on an attachment with
    // no alpha channel.
    b: 'crbug.com/dawn/1063',
    t: [win, intel],
    e: [Failure],
    q: [
      'webgpu:api,operation,render_pipeline,pipeline_output_targets:color,component_count,blend:*',
    ],
  },
  {
    // Error from debug layer.
    // Also affects crbug.com/dawn/1112.
    b: 'crbug.com/dawn/1064',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,adapter,requestDevice_limits:worse_than_default:*',
    ],
  },
  {
    // r8unorm/rg8unorm with multiple mip levels don't clear properly on Mac
    // Intel.
    b: 'crbug.com/dawn/1071',
    t: [mac, intel],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="r8unorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg8unorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="r8unorm";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rg8unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:initMethod="CopyB2T";checkMethod="FullCopyT2B";format="r8unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:initMethod="CopyB2T";checkMethod="FullCopyT2B";format="rg8unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:initMethod="WriteTexture";checkMethod="FullCopyT2B";format="r8unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:initMethod="WriteTexture";checkMethod="FullCopyT2B";format="rg8unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:initMethod="WriteTexture";checkMethod="PartialCopyT2B";format="r8unorm";*',
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:initMethod="WriteTexture";checkMethod="PartialCopyT2B";format="rg8unorm";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,color_attachment_only:colorFormat="r8unorm";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,color_attachment_only:colorFormat="rg8unorm";*',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";storeOperation="discard"',
      'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,depth_stencil_attachment_only:depthStencilFormat="stencil8";storeOperation="store"',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:format="r8unorm";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:format="rg8unorm";*',
      // Handled by the above broader queries.
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="CopyToBuffer";format="r8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="CopyToBuffer";format="rg8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="CopyToTexture";format="r8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="CopyToTexture";format="rg8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="r8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rg8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="CopyToBuffer";format="r8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="CopyToBuffer";format="rg8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="CopyToTexture";format="r8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="CopyToTexture";format="rg8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="Sample";format="r8unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="Sample";format="rg8unorm";*',
      // These should eventually be restricted to r8unorm and rg8unorm only.
      // 'webgpu:api,operation,render_pass,storeOp:render_pass_store_op,color_attachment_only:*',
      // Handled by an identical query in an unaffiliated group.
      // 'webgpu:api,validation,createTexture:mipLevelCount,format:*',
      // Recover the below two test expectations after new MSAA rules are
      // implemented (see crbug.com/dawn/1244 for more details).
      // 'webgpu:api,validation,createTexture:sampleCount,valid_sampleCount_with_other_parameter_varies:dimension="_undef_";*',
      // 'webgpu:api,validation,createTexture:sampleCount,valid_sampleCount_with_other_parameter_varies:dimension="2d";*',
      // Handled by a broader query in an unaffiliated group.
      // 'webgpu:api,validation,createTexture:sampleCount,valid_sampleCount_with_other_parameter_varies:dimension="3d";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyB2T";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="CopyT2B";*',
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";*',
    ],
  },
  {
    b: 'crbug.com/dawn/1071',
    t: [mac, intel],
    e: [Failure],
    w: true,
    q: [
      'webgpu:api,operation,render_pass,storeOp:*',
    ],
  },
  {
    // Incorrect results, only on Mac Intel.
    b: 'crbug.com/dawn/1083',
    t: [mac, intel],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:copy_depth_stencil:format="depth32float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:copy_depth_stencil:format="depth24plus";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:copy_depth_stencil:format="depth24plus-stencil8";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:copy_depth_stencil:format="depth32float-stencil8";*',
    ],
  },
  {
    b: 'crbug.com/dawn/1095',
    t: [linux, nvidia],
    e: [RetryOnFailure],
    q: [
      'webgpu:shader,execution,robust_access_vertex:vertex_buffer_access:*',
    ],
  },
  {
    b: 'crbug.com/dawn/1107',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="r32float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg16float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg32float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba16float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba32float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg11b10ufloat";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="r32float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rg16float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rg32float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgba16float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgba32float";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rg11b10ufloat";*',
    ],
  },
  {
    b: 'crbug.com/dawn/1111',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:shader,execution,zero_init:compute,zero_init:*',
    ],
  },
  {
    // Device lost failures for certain batches.
    b: 'crbug.com/dawn/1116',
    t: [mac, amd],
    e: [Failure],
    q: [
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=17;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=18;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=19;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=20;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=21;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=22;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=23;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=24;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=25;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=26;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=27;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=28;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=29;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=17;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=18;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=19;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=20;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=21;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=22;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=23;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=24;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=25;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=26;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=27;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=28;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=29;*',
    ],
  },
  {
    // Possibly Intel-only, flaky failures on Mac 11.5.2.
    b: 'crbug.com/dawn/1119',
    t: [bigsur],
    e: [Failure],
    w: true,
    q: [
      'webgpu:api,operation,rendering,basic:large_draw:*',
    ],
  },
  {
    // Need to clamp depth in shader on Vulkan.
    b: 'crbug.com/dawn/1125',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_clamp_and_clip:writeDepth=true;*',
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_test_input_clamped:unclippedDepth=false;*',
      // Started not finding any cases.
      /*'webgpu:api,operation,rendering,depth_clip_clamp:depth_clamp_and_clip:clampDepth=false;writeDepth=true;*',
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_test_input_clamped:clampDepth=false;*',*/
    ],
  },
  {
    // Failures because srgb-equality for compressed formats isn't implemented
    // in Dawn validation.
    b: 'crbug.com/dawn/1204',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,compressed,array:*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,compressed,non_array:*',
    ],
  },
  {
    // Failures because of the changes on the validation rules on the texture
    // format of multisampled texture creation.
    b: 'crbug.com/dawn/1244',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="r32sint";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="r32uint";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rg32sint";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rg32uint";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rgba32sint";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rgba32uint";*',
      // Conflicts with a broader query not associated with a bug.
      // 'webgpu:api,validation,createTexture:sampleCount,valid_sampleCount_with_other_parameter_varies:dimension="2d";*',
      // 'webgpu:api,validation,createTexture:sampleCount,valid_sampleCount_with_other_parameter_varies:dimension="_undef_";*',
      'webgpu:api,validation,createTexture:sampleCount,various_sampleCount_with_all_formats:*',
    ],
  },
  {
    // Originally part of the generic queries above, but had to be split due to
    // conflicts with queries associated with crbug.com/1237175.
    b: 'crbug.com/dawn/1244',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rg32float";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rgba32float";*',

    ],
  },
  {
    // Originally part of the generic queries above, but had to be split due to
    // conflicts with queries associated with crbug.com/1237175.
    b: 'crbug.com/dawn/1244',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rg32float";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rgba32float";*',

    ],
  },
  {
    // Originally part of the generic queries above, but had to be split due to
    // conflicts with queries associated with crbug.com/1237175.
    b: 'crbug.com/dawn/1244',
    t: [win, amd],
    e: [Failure],
    q: [
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rg32float";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rgba32float";*',

    ],
  },
  {
    // Originally part of the generic queries above, but had to be split due to
    // conflicts with queries associated with crbug.com/1237175.
    b: 'crbug.com/dawn/1244',
    t: [win, nvidia],
    e: [Failure],
    q: [
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rg32float";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="Sample";format="rgba32float";*',

    ],
  },
  {
    b: 'crbug.com/dawn/1256',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gpu_context_canvas:*',
    ],
  },
  {
    // # Device lost is triggered unexpectedly.
    b: 'crbug.com/dawn/1278',
    t: [win],
    e: [Failure],
    // More exact query that can be used once sub-cases are pulled out:
    // webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=32;dimension="3d";format="r8unorm";mipLevel=2;copyWidthModifier=-1;copyHeightModifier=-1;copyDepthModifier=0;*
    q: [
      'webgpu:api,validation,image_copy,texture_related:format:method="WriteTexture";depthOrArrayLayers=32;dimension="3d";format="r8unorm";*',
    ],
  },
  {
    // Wrong results in copyToTexture.
    b: 'crbug.com/dawn/1279',
    t: [win, intel],
    e: [Failure],
    q: [
      // Can be restricted to width=256 and height=255 once sub-cases are
      // pulled out.
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";orientation="flipY";srcDoFlipYDuringCopy=false;dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";orientation="none";srcDoFlipYDuringCopy=true;dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";srcDoFlipYDuringCopy=true;dstColorFormat="rg32float";*',
    ],
  },
  {
    // 3D texture issue.
    b: 'crbug.com/dawn/1288',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:dimension="3d";*',
    ],
  },
  {
    // 3D texture issue.
    b: 'crbug.com/dawn/1289',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:dimension="3d";*',
      'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow:dimension="3d";*',
    ],
  },
  {
    // This might not actually be the same root cause as the Windows suppression
    // above, as the Linux version was added when switching test harnesses and
    // the existing query seemed relevant.
    b: 'crbug.com/dawn/1289',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,image_copy:mip_levels:dimension="3d";*',
      // Conflicts with a broader query associated with crbug.com/dawn/690.
      // 'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_depth_stencil:format="depth32float-stencil8";copyMethod="CopyT2B";aspect="depth-only"',
      // 'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_depth_stencil:format="depth32float-stencil8";copyMethod="CopyT2B";aspect="depth-only"',
    ],
  },
  {
    b: 'crbug.com/dawn/1297',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,createRenderPipeline:pipeline_output_targets,blend_min_max:*',
    ],
  },
  {
    b: 'crbug.com/dawn/1314',
    t: [linux, nvidia],
    e: [Failure],
    q: [
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:orientation="flipY";dstColorFormat="rgba16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:orientation="none";srcDoFlipYDuringCopy=false;dstColorFormat="rgba16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:orientation="flipY";srcDoFlipYDuringCopy=true;dstColorFormat="rgba32float";dstPremultiplied=false',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:orientation="flipY";srcDoFlipYDuringCopy=true;dstColorFormat="rgba8unorm-srgb";dstPremultiplied=true',
    ]
  },
  {
    b: 'crbug.com/dawn/1319',
    t: [win, intel],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg16sint";dstFormat="rg16sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg16uint";dstFormat="rg16uint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg32float";dstFormat="rg32float";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg32sint";dstFormat="rg32sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rg32uint";dstFormat="rg32uint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba16sint";dstFormat="rgba16sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba16uint";dstFormat="rgba16uint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba32float";dstFormat="rgba32float";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba32sint";dstFormat="rgba32sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba32uint";dstFormat="rgba32uint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba8sint";dstFormat="rgba8sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba8snorm";dstFormat="rgba8snorm";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba8uint";dstFormat="rgba8uint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rg16uint";dstFormat="rg16uint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rg32sint";dstFormat="rg32sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgba16uint";dstFormat="rgba16uint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgba32sint";dstFormat="rgba32sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgba8sint";dstFormat="rgba8sint";dimension="2d"',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,non_array:srcFormat="rgba8uint";dstFormat="rgba8uint";dimension="2d"',
    ],
  },
  {
    b: 'crbug.com/dawn/1320',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="CopyToBuffer";format="rgba8uint";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="3d";readMethod="CopyToTexture";format="rgba8unorm";*',
    ],
  },
  //
  // Tint bugs
  //
  {
    // Timeout + compilation failure.
    b: 'crbug.com/tint/993',
    t: [mac],
    e: [Skip],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="function";*',
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="private";access="write";*',
    ],
  },
  {
    b: 'crbug.com/tint/993',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="function";*',
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="private";*',
    ],
  },
  {
    b: 'crbug.com/tint/993',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="workgroup";*',
    ],
  },
  {
    // Crashes on pipeline compilation in the driver.
    b: 'crbug.com/tint/993',
    t: [linux],
    e: [Skip],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="workgroup";*',
    ],
  },
  {
    b: 'crbug.com/tint/1215',
    t: null,
    e: [Failure],
    q: [
      'webgpu:shader,execution,shader_io,compute_builtins:inputs:*',
    ],
  },
  {
    b: 'crbug.com/tint/1216',
    t: null,
    e: [Failure],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="storage";storageMode="read_write";access="read";containerType="vector";*',
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="storage";storageMode="read";access="read";containerType="vector";*',
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="uniform";access="read";containerType="vector";*',
      // Currently conflict with a broader query from crbug.com/tint/993.
      // 'webgpu:shader,execution,robust_access:linear_memory:storageClass="workgroup";access="read";containerType="vector";*',
      // 'webgpu:shader,execution,robust_access:linear_memory:storageClass="workgroup";access="write";containerType="vector";*',
    ],
  },
  {
    // Originally part of the larger crbug.com/tin/1216 group, but split out to
    // resolve a conflict with a broad expectation from crbug.com/tint/993.
    b: 'crbug.com/tint/1216',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="private";access="read";containerType="vector";*',
    ],
  },
  {
    // Originally part of the larger crbug.com/tin/1216 group, but split out to
    // resolve a conflict with a broad expectation from crbug.com/tint/993.
    b: 'crbug.com/tint/1216',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="private";access="read";containerType="vector";*',
    ],
  },
  {
    b: 'crbug.com/tint/1228',
    t: null,
    e: [Failure],
    q: [
      'webgpu:shader,execution,builtin,log:float_builtin_functions,log:*',
      'webgpu:shader,execution,builtin,log2:float_builtin_functions,log2:*',
    ],
  },
  {
    b: 'crbug.com/tint/1228',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:shader,execution,builtin,abs:float_builtin_functions,abs_float:*',
    ],
  },
  {
    b: 'crbug.com/tint/1228',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:shader,execution,builtin,abs:float_builtin_functions,abs_float:*',
    ],
  },
  {
    // Failing since the test was added.
    b: 'crbug.com/tint/1287',
    t: [linux, nvidia],
    e: [Failure],
    q: [
      'webgpu:shader,execution,shader_io,shared_structs:shared_between_stages:*',
    ],
  },
  {
    // Failing since the test was added.
    b: 'crbug.com/tint/1287',
    t: [win, nvidia],
    e: [Failure],
    q: [
      'webgpu:shader,execution,shader_io,shared_structs:shared_between_stages:*',
    ],
  },
  {
    // KI due to support in Tint being rolled back.
    b: 'crbug.com/tint/1322',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:shader,execution,sampling,gradients_in_varying_loop:derivative_in_varying_loop:*',
    ],
  },
  {
    b: 'crbug.com/tint/1367',
    t: [linux, intel],
    e: [Failure],
    q: [
      'webgpu:shader,execution,builtin,countTrailingZeros:integer_builtin_functions,countTrailingZeros_signed:*',
      'webgpu:shader,execution,builtin,countTrailingZeros:integer_builtin_functions,countTrailingZeros_unsigned:*',
      'webgpu:shader,execution,builtin,firstTrailingBit:integer_builtin_functions,firstTrailingBit_signed:*',
      'webgpu:shader,execution,builtin,firstTrailingBit:integer_builtin_functions,firstTrailingBit_unsigned:*',
    ],
  },
  {
    b: 'crbug.com/tint/1464',
    t: [linux, intel],
    e: [Slow],
    q: [
      'webgpu:shader,execution,memory_model,atomicity:atomicity:*',
      'webgpu:shader,execution,memory_model,coherence:corr:*',
    ],
  },
  {
    b: 'crbug.com/tint/1467',
    t: [mac, intel],
    e: [Failure],
    q: [
      'webgpu:shader,execution,builtin,atan2:*',
    ],
  },
  {
    b: 'crbug.com/tint/1471',
    t: null,
    e: [Failure],
    q: [
      'webgpu:shader,execution,builtin,ldexp:float_builtin_functions,ldexp:*',
    ],
  },
  //
  // Chromium bugs
  //
  {
    // Flaky "Check failed: bytes_in_use_ == 0u".
    b: 'crbug.com/1005284',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,validation,texture,destroy:twice:*',
    ],
  },
  {
    // Very flaky, especially (but not exclusively!) with backend validation.
    b: 'crbug.com/1087130',
    t: [win],
    e: [RetryOnFailure],
    // This was originally just 'webgpu:api,validation,createView:*', but the
    // webgpu:api,validation,createView:format:* tests conflicted with an
    // unassociated query for stencil8/depth16unorm failures. So, we need to
    // explicitly list multiple queries instead.
    q: [
      'webgpu:api,validation,createView:dimension:*',
      'webgpu:api,validation,createView:aspect:*',
      'webgpu:api,validation,createView:array_layers:*',
      'webgpu:api,validation,createView:mip_levels:*',
      'webgpu:api,validation,createView:cube_faces_square:*',
      'webgpu:api,validation,createView:texture_state:*',
    ],
  },
  {
    b: 'crbug.com/1197369',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_image,crossOrigin:*',
    ],
  },
  {
    b: 'crbug.com/1197369',
    t: null,
    e: [Skip],
    q: [
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="flipY";*',
    ],
  },
  {
    b: 'crbug.com/1213657',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:*',
    ],
  },
  {
    b: 'crbug.com/1215024',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,createComputePipeline:enrty_point_name_must_match:stageEntryPoint="main%5Cu0000";*',
      'webgpu:api,validation,createComputePipeline:enrty_point_name_must_match:stageEntryPoint="main%5Cu0000a";*',
    ],
  },
  {
    // Crashes or fails with "Backing is being accessed by both GL and Vulkan".
    b: 'crbug.com/1234041',
    t: [linux],
    e: [Skip],
    // This was originally
    // 'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:*',
    // but that partially conflicts with a query from crbug.com/1197369 that
    // specifies CopyExternalImageToTexture:source_image,crossOrigin, so
    // additional splits have to be made.
    q: [
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_canvas,contexts:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_canvas,state:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_offscreenCanvas,contexts:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_offscreenCanvas,state:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_imageBitmap,state:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,state:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,device_mismatch:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,dimension:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,usage:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,sample_count:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,mipLevel:*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,format:*',
    ],
  },
  {
    // Shared image synchronization.
    b: 'crbug.com/1236130',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:drawTo2DCanvas:*',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:offscreenCanvas,snapshot:*',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,snapshot:*',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,uploadToWebGL:*',
    ],
  },
  {
    // StoreOpClear handling is overclearing resources that should be preserved.
    b: 'crbug.com/1237175',
    t: [win, intel],
    e: [Failure],
    q: [
      // Can be restricted to
      // uninitializeMethod="StoreOpClear";canaryOnCreation=true; once sub-cases
      // are pulled out.
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";format="rg32float";*',
      'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";format="rgba32float";*',
    ],
  },
  {
    // Null-deref on Intel, failure on NVIDIA.
    b: 'crbug.com/1237592',
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:web_platform,external_texture,video:importExternalTexture,*',
    ],
  },
  {
    // Test times out. Issue with hardware decoding?
    b: 'crbug.com/1238241',
    t: [win, nvidia],
    e: [Skip],
    q: [
      'webgpu:web_platform,external_texture,video:importExternalTexture,sample:*',
    ],
  },
  {
    // SharedImageBackingFactoryIOSurface takes rgba8unorm as bgra8unorm.
    // https://source.chromium.org/chromium/chromium/src/+/main:gpu/command_buffer/service/shared_image_backing_factory_iosurface.mm;l=217?q=SharedImageBackingFactoryIOSurface::CreateSharedImage&ss=chromium%2Fchromium%2Fsrc
    b: 'crbug.com/1241369',
    t: [mac],
    e: [Skip],
    q: [
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,snapshot:format="rgba8unorm";snapshotType="toDataURL"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,snapshot:format="rgba8unorm";snapshotType="toBlob"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,snapshot:format="rgba8unorm";snapshotType="imageBitmap"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,uploadToWebGL:format="rgba8unorm";webgl="webgl";upload="texImage2D"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,uploadToWebGL:format="rgba8unorm";webgl="webgl";upload="texSubImage2D"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,uploadToWebGL:format="rgba8unorm";webgl="webgl2";upload="texImage2D"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:onscreenCanvas,uploadToWebGL:format="rgba8unorm";webgl="webgl2";upload="texSubImage2D"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:offscreenCanvas,snapshot:format="rgba8unorm";snapshotType="convertToBlob"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:offscreenCanvas,snapshot:format="rgba8unorm";snapshotType="transferToImageBitmap"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:offscreenCanvas,snapshot:format="rgba8unorm";snapshotType="imageBitmap"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:drawTo2DCanvas:format="rgba8unorm";webgpuCanvasType="onscreen";canvas2DType="onscreen"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:drawTo2DCanvas:format="rgba8unorm";webgpuCanvasType="onscreen";canvas2DType="offscreen"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:drawTo2DCanvas:format="rgba8unorm";webgpuCanvasType="offscreen";canvas2DType="onscreen"',
      'webgpu:web_platform,canvas,readbackFromWebGPUCanvas:drawTo2DCanvas:format="rgba8unorm";webgpuCanvasType="offscreen";canvas2DType="offscreen"',
    ],
  },
  {
    // TODO: Remove and add an expected crash count?
    // Intentionally hits a CHECK.
    b: 'crbug.com/1243842',
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,operation,buffers,map_ArrayBuffer:postMessage:transfer=true;*',
    ],
  },
  {
    // CopyExternalImageToTexture test failures with CPU uploading path on
    // Windows.
    b: 'crbug.com/1269118',
    t: [win, intel_hd630],
    e: [Failure],
    q: [
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="offscreen";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="offscreen";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="onscreen";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="onscreen";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl2";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl2";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl2";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl2";dstColorFormat="rgba32float";*',
    ],
  },
  {
    // CopyExternalImageToTexture test failures with CPU uploading path on
    // Windows.
    b: 'crbug.com/1269118',
    t: [win, intel_uhd630],
    e: [Failure],
    q: [
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="offscreen";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="offscreen";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="onscreen";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="onscreen";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl2";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";contextName="webgl2";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl2";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";contextName="webgl2";dstColorFormat="rgba32float";*',
    ],
  },
  {
    // WebGPU allows copy from webgpu context in CopyExternalImageToTexture().
    // Disable related cts temporarily. This fails on Linux, Mac, and Win, but
    // Linux is already covered by a Skip expectation associated with
    // crbug.com/1234041.
    b: 'crbug.com/1282838',
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_canvas,contexts:*',
    ],
  },
  {
    // WebGPU allows copy from webgpu context in CopyExternalImageToTexture().
    // Disable related cts temporarily. This fails on Linux, Mac, and Win, but
    // Linux is already covered by a Skip expectation associated with
    // crbug.com/1234041.
    b: 'crbug.com/1282838',
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:source_canvas,contexts:*',
    ],
  },
  {
    b: 'crbug.com/1299319',
    t: [mac, amd],
    e: [Failure],
    q: [
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";orientation="flipY";srcDoFlipYDuringCopy=false;dstColorFormat="rgb10a2unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";orientation="flipY";srcDoFlipYDuringCopy=true;dstColorFormat="rgb10a2unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";orientation="none";srcDoFlipYDuringCopy=false;dstColorFormat="rgb10a2unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";orientation="none";srcDoFlipYDuringCopy=true;dstColorFormat="rgb10a2unorm";*',
      // Conflicts with Skip expectations from crbug.com/1197369,
      // 'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="flipY";srcDoFlipYDuringCopy=false;dstColorFormat="rgb10a2unorm";*',
      // 'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="flipY";srcDoFlipYDuringCopy=true;dstColorFormat="rgb10a2unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";srcDoFlipYDuringCopy=false;dstColorFormat="rgb10a2unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";srcDoFlipYDuringCopy=true;dstColorFormat="rgb10a2unorm";*',
    ],
  },
  {
    b: 'crbug.com/1301808',
    t: [linux, intel],
    e: [Failure],
    q: [
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="offscreen";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:*',
    ],
  },
  //
  // Unaffiliated bugs
  //
  {
    // Failures because stencil8 and depth16unorm aren't implemented.
    // All of these should be restricted to stencil8/depth16unorm once they are
    // no longer sub-cases.
    b: null,
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,createTexture:mipLevelCount,format:*',
      // These are handled by the more general suppression for
      // crbug.com/dawn/1244 above.
      // 'webgpu:api,validation,createTexture:sampleCount,various_sampleCount_with_all_formats:format="depth16unorm";*',
      // 'webgpu:api,validation,createTexture:sampleCount,various_sampleCount_with_all_formats:format="stencil8";*',
      'webgpu:api,validation,createTexture:texture_size,default_value_and_smallest_size,uncompressed_format:*',
      // Conflicts with a broader query from crbug.com/dawn/685.
      // 'webgpu:api,validation,createTexture:texture_size,2d_texture,uncompressed_format:format="stencil8";*',
      // 'webgpu:api,validation,createTexture:texture_size,2d_texture,uncompressed_format:format="depth16unorm";*',
      'webgpu:api,validation,createTexture:texture_usage:*',
      'webgpu:api,validation,createTexture:dimension_type_and_format_compatibility:*',
      'webgpu:api,validation,encoding,cmds,buffer_texture_copies:depth_stencil_format,copy_buffer_size:format="depth16unorm";*',
      'webgpu:api,validation,encoding,cmds,buffer_texture_copies:depth_stencil_format,copy_buffer_size:format="stencil8";*',
      // Conflicts with a broader query from crbug.com/dawn/666.
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:dimension="2d";readMethod="CopyToTexture";format="stencil8";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:readMethod="DepthTest";format="depth16unorm";*',
      // 'webgpu:api,operation,resource_init,texture_zero:uninitialized_texture_is_zero:readMethod="StencilTest";format="stencil8";*',
      'webgpu:api,validation,createTexture:sampleCount,valid_sampleCount_with_other_parameter_varies:*',
      'webgpu:api,operation,rendering,depth:depth_compare_func:format="depth16unorm";*',
      'webgpu:api,validation,resource_usages,texture,in_pass_encoder:subresources_and_binding_types_combination_for_aspect:format="depth16unorm";*',
      'webgpu:api,validation,resource_usages,texture,in_pass_encoder:subresources_and_binding_types_combination_for_aspect:format="stencil8";*',
      'webgpu:api,validation,resource_usages,texture,in_pass_encoder:subresources_and_binding_types_combination_for_aspect:format="depth24unorm-stencil8";*',
      'webgpu:api,validation,resource_usages,texture,in_pass_encoder:subresources_and_binding_types_combination_for_aspect:format="depth32float-stencil8";*',
      // TODO: Figure out why the broader query doesn't match anything.
      // 'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_upload_to_stencil_aspect:stencilFormat="stencil8";*',
      // 'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_upload_to_stencil_aspect:stencilFormat="stencil8";*',
      // 'webgpu:api,operation,command_buffer,image_copy:rowsPerImage_and_bytesPerRow_copy_with_stencil_aspect:stencilFormat="stencil8";*',
      'webgpu:api,validation,createView:format:*',
    ],
  },
  {
    // Originally part of the group above, but split out to resolve a conflict
    // with a query from crbug.com/1087130.
    b: null,
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,validation,createView:aspect:format="depth16unorm";*',
      'webgpu:api,validation,createView:aspect:format="stencil8";*',
    ],
  },
  {
    // Originally part of the group above, but split out to resolve a conflict
    // with queries from crbug.com/1087130 and crbug.com/1234041.
    b: null,
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:api,validation,createView:aspect:format="depth16unorm";*',
      'webgpu:api,validation,createView:aspect:format="stencil8";*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,format:format="depth16unorm";*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,format:format="stencil8";*',
    ],
  },
  {
    // Originally part of the group above, but split out to resolve a conflict
    // with a query from crbug.com/1234041.
    b: null,
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,format:format="depth16unorm";*',
      'webgpu:api,validation,queue,copyToTexture,CopyExternalImageToTexture:destination_texture,format:format="stencil8";*',
    ],
  },
  {
    b: null,
    t: [mac],
    e: [Slow],
    q: [
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba32sint";dstFormat="rgba32sint";*',
      'webgpu:api,operation,command_buffer,copyTextureToTexture:color_textures,non_compressed,array:srcFormat="rgba32uint";dstFormat="rgba32uint";*',
    ],
  },
  // These Slow expectations should apply to just 'mac', but conflicts with
  // AMD-specific expectations from crbug.com/1299319.
  {
    b: null,
    t: [mac, amd],
    e: [Slow],
    q: [
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:*',
      // Should be webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:*,
      // but conflicts with the Skip expectations associated with
      // crbug.com/1197369.
      // 'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";*',
      // 'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="bgra8unorm-srgb";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="bgra8unorm-srgb";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="bgra8unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="bgra8unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="r16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="r16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="r32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="r32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="r8unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="r8unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="rg16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="rg16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="rg32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="rg8unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="rg8unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="rgba16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="rgba16float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="rgba32float";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="rgba8unorm-srgb";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="rgba8unorm-srgb";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";dstColorFormat="rgba8unorm";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";dstColorFormat="rgba8unorm";*',
    ]
  },
  {
    b: null,
    t: [mac, intel],
    e: [Slow],
    q: [
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:*',
      // Should be webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:*,
      // but conflicts with the Skip expectations associated with
      // crbug.com/1197369.
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";*',
    ]
  },
  {
    b: null,
    t: [mac, nvidia],
    e: [Slow],
    q: [
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_canvas:*',
      // Should be webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:*,
      // but conflicts with the Skip expectations associated with
      // crbug.com/1197369.
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="none";*',
      'webgpu:web_platform,copyToTexture,ImageBitmap:from_ImageData:alpha="premultiply";orientation="none";*',
    ]
  },
  {
    b: null,
    t: [mac, amd],
    e: [Slow],
    q: [
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="offscreen";dstColorFormat="rgba16float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_gl_context_canvas:canvasType="onscreen";dstColorFormat="rgba16float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="offscreen";dstColorFormat="rgba16float";*',
      'webgpu:web_platform,copyToTexture,canvas:copy_contents_from_2d_context_canvas:canvasType="onscreen";dstColorFormat="rgba16float";*',
    ],
  },
  {
    // This and the below Intel/NVIDIA queries can be combined into a single
    // "mac" entry once the AMD-specific issues associated with
    // crbug.com/dawn/1116 are resolved.
    b: null,
    t: [mac, amd],
    e: [Slow],
    q: [
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=1;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=2;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=3;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=4;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=5;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=6;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=7;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=8;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=9;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=10;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=11;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=12;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=13;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=14;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=15;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="function";batch__=16;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=1;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=2;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=3;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=4;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=5;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=6;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=7;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=8;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=9;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=10;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=11;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=12;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=13;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=14;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=15;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="private";batch__=16;*',
      'webgpu:shader,execution,zero_init:compute,zero_init:storageClass="workgroup";*',
    ],
  },
  {
    b: null,
    t: [mac, intel],
    e: [Slow],
    q: [
      'webgpu:shader,execution,zero_init:compute,zero_init:*',
    ],
  },
  {
    b: null,
    t: [mac, nvidia],
    e: [Slow],
    q: [
      'webgpu:shader,execution,zero_init:compute,zero_init:*',
    ],
  },
  {
    // Spec was changed so BGLs should eagerly apply per-pipeline limits. Tests
    // need fixing, then Dawn needs to pass them.
    // https://github.com/gpuweb/cts/issues/230
    b: null,
    t: null,
    e: [Failure],
    q: [
      'webgpu:api,validation,createBindGroupLayout:max_resources_per_stage,in_bind_group_layout:*',
    ],
  },
  {
    // Deprecated values temporarily cause the wrong count.
    b: null,
    t: null,
    e: [Failure],
    q: [
      'webgpu:idl,constants,flags:TextureUsage,count:*',
    ],
  },
  // Started not matching any cases.
  /*{
    // Should only apply to 'format="stencil8"', but that is currently a
    // subcase within the test. Originally a single query that applied
    // everywhere, but partially conflicted with a Linux query for
    // crbug.com/dawn/1125 that specified
    // depth_test_input_clamped:clampDepth=false;*
    b: null,
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_test_input_clamped:clampDepth=true;*',
    ],
  },*/
  {
    // Should only apply to 'format="stencil8"', but that is currently a
    // subcase within the test. Originally a single query that applied
    // everywhere, but partially conflicted with a Linux query for
    // crbug.com/dawn/1125 that specified
    // depth_test_input_clamped:clampDepth=false;*
    b: null,
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_test_input_clamped:*',
    ],
  },
  {
    // Should only apply to 'format="stencil8"', but that is currently a
    // subcase within the test. Originally a single query that applied
    // everywhere, but partially conflicted with a Linux query for
    // crbug.com/dawn/1125 that specified
    // depth_test_input_clamped:clampDepth=false;*
    b: null,
    t: [win],
    e: [Failure],
    q: [
      'webgpu:api,operation,rendering,depth_clip_clamp:depth_test_input_clamped:*',
    ],
  },
  {
    // Our automated build does not support mp4 currently (fails on Linux, Mac,
    // and Win Intel). Linux failure is already handled by a broader query from
    // crbug.com/1237592, so specify Win and Mac.
    b: null,
    t: [mac],
    e: [Failure],
    q: [
      'webgpu:web_platform,external_texture,video:importExternalTexture,sample:videoSource="red-green.mp4"',
    ],
  },
  {
    // Our automated build does not support mp4 currently (fails on Linux, Mac,
    // and Win Intel). Linux failure is already handled by a broader query from
    // crbug.com/1237592, so specify Win and Mac.
    b: null,
    t: [win, intel],
    e: [Failure],
    q: [
      'webgpu:web_platform,external_texture,video:importExternalTexture,sample:videoSource="red-green.mp4"',
    ],
  },
  {
    b: null,
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:shader,execution,robust_access:linear_memory:storageClass="function";access="read";containerType="vector";*',
    ],
  },
  // TODO: Determine if this can be removed.
  /*{
    // Failures because stencil8 and depth16unorm aren't implemented.
    // Should only apply to 'stencilFormat="stencil8"', but that is currently a
    // subcase within the test.
    b: null,
    t: [linux],
    e: [Failure],
    q: [
      'webgpu:api,operation,command_buffer,image_copy:offsets_and_sizes_copy_with_stencil_aspect:*',
    ],
  },*/
];

var expectations = [];
// This is currently removed so that the blanket skip works properly.
/*
for (const group of expectation_groups) {
  for (const query of group.q) {
    expectations.push({
      b: group.b,
      t: group.t,
      e: group.e,
      q: query,
      w: group.w || false,
    });
  }
}*/

module.exports = { expectations };
