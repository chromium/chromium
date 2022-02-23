// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that expecations below have lighter syntax without quotes.
const Mac = 'Mac';
const Win = 'Win';
const Failure = 'Failure';

const expectations = [
  // Example:
  // {
  //   b: 'crbug.com/dawn/1304812342',
  //   t: [Mac],
  //   q: 'webgpu:api,operation,command_buffer,copyTextureToTexture:*',
  //   e: [Failure]
  // },
  // {
  //   b: 'crbug.com/dawn/9839583',
  //   t: [Win],
  //   q: 'webgpu:api,validation,resource_usages,texture,in_pass_encoder:unused_bindings_in_pipeline:*',
  //   e: [Failure]
  // }
];

module.exports = { expectations };
