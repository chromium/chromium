// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
RWBuffer<half> input_buffer : register(u0);
RWBuffer<float> output_buffer : register(u1);

#define BLOCK_SIZE 512
cbuffer ConstantBufferCS
{
  uint num;
  uint channel;
  uint size;
};

[numthreads(BLOCK_SIZE, 1, 1)]
void format_half_output(uint3 block_id : SV_GroupID, uint3 thread_id : SV_GroupThreadID)
{
  uint index = block_id.x * BLOCK_SIZE + thread_id.x;
  uint chw_length = channel * size;
  if (index < num * chw_length)
  {
    uint num_stride = index / chw_length * chw_length;
    uint num_index = index >= chw_length ? (index - num_stride) : index;
    uint3 strides = {num_index / channel,                // width * height stride
                      index % channel * size,            // channel stride
                      num_stride};                       // num stride
    output_buffer[index] = input_buffer[strides.x + strides.y + strides.z];	
  }
}