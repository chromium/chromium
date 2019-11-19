// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_AUTOGEN_H_

#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_SCANOUT_CHROMIUM 0x6000

struct ActiveTexture {
  typedef ActiveTexture ValueType;
  static const CommandId kCmdId = kActiveTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _texture) {
    SetHeader();
    texture = _texture;
  }

  void* Set(void* cmd, GLenum _texture) {
    static_cast<ValueType*>(cmd)->Init(_texture);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture;
};

static_assert(sizeof(ActiveTexture) == 8, "size of ActiveTexture should be 8");
static_assert(offsetof(ActiveTexture, header) == 0,
              "offset of ActiveTexture header should be 0");
static_assert(offsetof(ActiveTexture, texture) == 4,
              "offset of ActiveTexture texture should be 4");

struct AttachShader {
  typedef AttachShader ValueType;
  static const CommandId kCmdId = kAttachShader;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, GLuint _shader) {
    SetHeader();
    program = _program;
    shader = _shader;
  }

  void* Set(void* cmd, GLuint _program, GLuint _shader) {
    static_cast<ValueType*>(cmd)->Init(_program, _shader);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t shader;
};

static_assert(sizeof(AttachShader) == 12, "size of AttachShader should be 12");
static_assert(offsetof(AttachShader, header) == 0,
              "offset of AttachShader header should be 0");
static_assert(offsetof(AttachShader, program) == 4,
              "offset of AttachShader program should be 4");
static_assert(offsetof(AttachShader, shader) == 8,
              "offset of AttachShader shader should be 8");

struct BindAttribLocationBucket {
  typedef BindAttribLocationBucket ValueType;
  static const CommandId kCmdId = kBindAttribLocationBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, GLuint _index, uint32_t _name_bucket_id) {
    SetHeader();
    program = _program;
    index = _index;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _index, _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t index;
  uint32_t name_bucket_id;
};

static_assert(sizeof(BindAttribLocationBucket) == 16,
              "size of BindAttribLocationBucket should be 16");
static_assert(offsetof(BindAttribLocationBucket, header) == 0,
              "offset of BindAttribLocationBucket header should be 0");
static_assert(offsetof(BindAttribLocationBucket, program) == 4,
              "offset of BindAttribLocationBucket program should be 4");
static_assert(offsetof(BindAttribLocationBucket, index) == 8,
              "offset of BindAttribLocationBucket index should be 8");
static_assert(offsetof(BindAttribLocationBucket, name_bucket_id) == 12,
              "offset of BindAttribLocationBucket name_bucket_id should be 12");

struct BindBuffer {
  typedef BindBuffer ValueType;
  static const CommandId kCmdId = kBindBuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _buffer) {
    SetHeader();
    target = _target;
    buffer = _buffer;
  }

  void* Set(void* cmd, GLenum _target, GLuint _buffer) {
    static_cast<ValueType*>(cmd)->Init(_target, _buffer);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t buffer;
};

static_assert(sizeof(BindBuffer) == 12, "size of BindBuffer should be 12");
static_assert(offsetof(BindBuffer, header) == 0,
              "offset of BindBuffer header should be 0");
static_assert(offsetof(BindBuffer, target) == 4,
              "offset of BindBuffer target should be 4");
static_assert(offsetof(BindBuffer, buffer) == 8,
              "offset of BindBuffer buffer should be 8");

struct BindBufferBase {
  typedef BindBufferBase ValueType;
  static const CommandId kCmdId = kBindBufferBase;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _index, GLuint _buffer) {
    SetHeader();
    target = _target;
    index = _index;
    buffer = _buffer;
  }

  void* Set(void* cmd, GLenum _target, GLuint _index, GLuint _buffer) {
    static_cast<ValueType*>(cmd)->Init(_target, _index, _buffer);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t index;
  uint32_t buffer;
};

static_assert(sizeof(BindBufferBase) == 16,
              "size of BindBufferBase should be 16");
static_assert(offsetof(BindBufferBase, header) == 0,
              "offset of BindBufferBase header should be 0");
static_assert(offsetof(BindBufferBase, target) == 4,
              "offset of BindBufferBase target should be 4");
static_assert(offsetof(BindBufferBase, index) == 8,
              "offset of BindBufferBase index should be 8");
static_assert(offsetof(BindBufferBase, buffer) == 12,
              "offset of BindBufferBase buffer should be 12");

struct BindBufferRange {
  typedef BindBufferRange ValueType;
  static const CommandId kCmdId = kBindBufferRange;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLuint _index,
            GLuint _buffer,
            GLintptr _offset,
            GLsizeiptr _size) {
    SetHeader();
    target = _target;
    index = _index;
    buffer = _buffer;
    offset = _offset;
    size = _size;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLuint _index,
            GLuint _buffer,
            GLintptr _offset,
            GLsizeiptr _size) {
    static_cast<ValueType*>(cmd)->Init(_target, _index, _buffer, _offset,
                                       _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t index;
  uint32_t buffer;
  int32_t offset;
  int32_t size;
};

static_assert(sizeof(BindBufferRange) == 24,
              "size of BindBufferRange should be 24");
static_assert(offsetof(BindBufferRange, header) == 0,
              "offset of BindBufferRange header should be 0");
static_assert(offsetof(BindBufferRange, target) == 4,
              "offset of BindBufferRange target should be 4");
static_assert(offsetof(BindBufferRange, index) == 8,
              "offset of BindBufferRange index should be 8");
static_assert(offsetof(BindBufferRange, buffer) == 12,
              "offset of BindBufferRange buffer should be 12");
static_assert(offsetof(BindBufferRange, offset) == 16,
              "offset of BindBufferRange offset should be 16");
static_assert(offsetof(BindBufferRange, size) == 20,
              "offset of BindBufferRange size should be 20");

struct BindFramebuffer {
  typedef BindFramebuffer ValueType;
  static const CommandId kCmdId = kBindFramebuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _framebuffer) {
    SetHeader();
    target = _target;
    framebuffer = _framebuffer;
  }

  void* Set(void* cmd, GLenum _target, GLuint _framebuffer) {
    static_cast<ValueType*>(cmd)->Init(_target, _framebuffer);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t framebuffer;
};

static_assert(sizeof(BindFramebuffer) == 12,
              "size of BindFramebuffer should be 12");
static_assert(offsetof(BindFramebuffer, header) == 0,
              "offset of BindFramebuffer header should be 0");
static_assert(offsetof(BindFramebuffer, target) == 4,
              "offset of BindFramebuffer target should be 4");
static_assert(offsetof(BindFramebuffer, framebuffer) == 8,
              "offset of BindFramebuffer framebuffer should be 8");

struct BindRenderbuffer {
  typedef BindRenderbuffer ValueType;
  static const CommandId kCmdId = kBindRenderbuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _renderbuffer) {
    SetHeader();
    target = _target;
    renderbuffer = _renderbuffer;
  }

  void* Set(void* cmd, GLenum _target, GLuint _renderbuffer) {
    static_cast<ValueType*>(cmd)->Init(_target, _renderbuffer);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t renderbuffer;
};

static_assert(sizeof(BindRenderbuffer) == 12,
              "size of BindRenderbuffer should be 12");
static_assert(offsetof(BindRenderbuffer, header) == 0,
              "offset of BindRenderbuffer header should be 0");
static_assert(offsetof(BindRenderbuffer, target) == 4,
              "offset of BindRenderbuffer target should be 4");
static_assert(offsetof(BindRenderbuffer, renderbuffer) == 8,
              "offset of BindRenderbuffer renderbuffer should be 8");

struct BindSampler {
  typedef BindSampler ValueType;
  static const CommandId kCmdId = kBindSampler;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _unit, GLuint _sampler) {
    SetHeader();
    unit = _unit;
    sampler = _sampler;
  }

  void* Set(void* cmd, GLuint _unit, GLuint _sampler) {
    static_cast<ValueType*>(cmd)->Init(_unit, _sampler);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t unit;
  uint32_t sampler;
};

static_assert(sizeof(BindSampler) == 12, "size of BindSampler should be 12");
static_assert(offsetof(BindSampler, header) == 0,
              "offset of BindSampler header should be 0");
static_assert(offsetof(BindSampler, unit) == 4,
              "offset of BindSampler unit should be 4");
static_assert(offsetof(BindSampler, sampler) == 8,
              "offset of BindSampler sampler should be 8");

struct BindTexture {
  typedef BindTexture ValueType;
  static const CommandId kCmdId = kBindTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _texture) {
    SetHeader();
    target = _target;
    texture = _texture;
  }

  void* Set(void* cmd, GLenum _target, GLuint _texture) {
    static_cast<ValueType*>(cmd)->Init(_target, _texture);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t texture;
};

static_assert(sizeof(BindTexture) == 12, "size of BindTexture should be 12");
static_assert(offsetof(BindTexture, header) == 0,
              "offset of BindTexture header should be 0");
static_assert(offsetof(BindTexture, target) == 4,
              "offset of BindTexture target should be 4");
static_assert(offsetof(BindTexture, texture) == 8,
              "offset of BindTexture texture should be 8");

struct BindTransformFeedback {
  typedef BindTransformFeedback ValueType;
  static const CommandId kCmdId = kBindTransformFeedback;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _transformfeedback) {
    SetHeader();
    target = _target;
    transformfeedback = _transformfeedback;
  }

  void* Set(void* cmd, GLenum _target, GLuint _transformfeedback) {
    static_cast<ValueType*>(cmd)->Init(_target, _transformfeedback);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t transformfeedback;
};

static_assert(sizeof(BindTransformFeedback) == 12,
              "size of BindTransformFeedback should be 12");
static_assert(offsetof(BindTransformFeedback, header) == 0,
              "offset of BindTransformFeedback header should be 0");
static_assert(offsetof(BindTransformFeedback, target) == 4,
              "offset of BindTransformFeedback target should be 4");
static_assert(offsetof(BindTransformFeedback, transformfeedback) == 8,
              "offset of BindTransformFeedback transformfeedback should be 8");

struct BlendColor {
  typedef BlendColor ValueType;
  static const CommandId kCmdId = kBlendColor;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLclampf _red, GLclampf _green, GLclampf _blue, GLclampf _alpha) {
    SetHeader();
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
  }

  void* Set(void* cmd,
            GLclampf _red,
            GLclampf _green,
            GLclampf _blue,
            GLclampf _alpha) {
    static_cast<ValueType*>(cmd)->Init(_red, _green, _blue, _alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float red;
  float green;
  float blue;
  float alpha;
};

static_assert(sizeof(BlendColor) == 20, "size of BlendColor should be 20");
static_assert(offsetof(BlendColor, header) == 0,
              "offset of BlendColor header should be 0");
static_assert(offsetof(BlendColor, red) == 4,
              "offset of BlendColor red should be 4");
static_assert(offsetof(BlendColor, green) == 8,
              "offset of BlendColor green should be 8");
static_assert(offsetof(BlendColor, blue) == 12,
              "offset of BlendColor blue should be 12");
static_assert(offsetof(BlendColor, alpha) == 16,
              "offset of BlendColor alpha should be 16");

struct BlendEquation {
  typedef BlendEquation ValueType;
  static const CommandId kCmdId = kBlendEquation;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode) {
    SetHeader();
    mode = _mode;
  }

  void* Set(void* cmd, GLenum _mode) {
    static_cast<ValueType*>(cmd)->Init(_mode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
};

static_assert(sizeof(BlendEquation) == 8, "size of BlendEquation should be 8");
static_assert(offsetof(BlendEquation, header) == 0,
              "offset of BlendEquation header should be 0");
static_assert(offsetof(BlendEquation, mode) == 4,
              "offset of BlendEquation mode should be 4");

struct BlendEquationSeparate {
  typedef BlendEquationSeparate ValueType;
  static const CommandId kCmdId = kBlendEquationSeparate;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _modeRGB, GLenum _modeAlpha) {
    SetHeader();
    modeRGB = _modeRGB;
    modeAlpha = _modeAlpha;
  }

  void* Set(void* cmd, GLenum _modeRGB, GLenum _modeAlpha) {
    static_cast<ValueType*>(cmd)->Init(_modeRGB, _modeAlpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t modeRGB;
  uint32_t modeAlpha;
};

static_assert(sizeof(BlendEquationSeparate) == 12,
              "size of BlendEquationSeparate should be 12");
static_assert(offsetof(BlendEquationSeparate, header) == 0,
              "offset of BlendEquationSeparate header should be 0");
static_assert(offsetof(BlendEquationSeparate, modeRGB) == 4,
              "offset of BlendEquationSeparate modeRGB should be 4");
static_assert(offsetof(BlendEquationSeparate, modeAlpha) == 8,
              "offset of BlendEquationSeparate modeAlpha should be 8");

struct BlendFunc {
  typedef BlendFunc ValueType;
  static const CommandId kCmdId = kBlendFunc;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _sfactor, GLenum _dfactor) {
    SetHeader();
    sfactor = _sfactor;
    dfactor = _dfactor;
  }

  void* Set(void* cmd, GLenum _sfactor, GLenum _dfactor) {
    static_cast<ValueType*>(cmd)->Init(_sfactor, _dfactor);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sfactor;
  uint32_t dfactor;
};

static_assert(sizeof(BlendFunc) == 12, "size of BlendFunc should be 12");
static_assert(offsetof(BlendFunc, header) == 0,
              "offset of BlendFunc header should be 0");
static_assert(offsetof(BlendFunc, sfactor) == 4,
              "offset of BlendFunc sfactor should be 4");
static_assert(offsetof(BlendFunc, dfactor) == 8,
              "offset of BlendFunc dfactor should be 8");

struct BlendFuncSeparate {
  typedef BlendFuncSeparate ValueType;
  static const CommandId kCmdId = kBlendFuncSeparate;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _srcRGB,
            GLenum _dstRGB,
            GLenum _srcAlpha,
            GLenum _dstAlpha) {
    SetHeader();
    srcRGB = _srcRGB;
    dstRGB = _dstRGB;
    srcAlpha = _srcAlpha;
    dstAlpha = _dstAlpha;
  }

  void* Set(void* cmd,
            GLenum _srcRGB,
            GLenum _dstRGB,
            GLenum _srcAlpha,
            GLenum _dstAlpha) {
    static_cast<ValueType*>(cmd)->Init(_srcRGB, _dstRGB, _srcAlpha, _dstAlpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t srcRGB;
  uint32_t dstRGB;
  uint32_t srcAlpha;
  uint32_t dstAlpha;
};

static_assert(sizeof(BlendFuncSeparate) == 20,
              "size of BlendFuncSeparate should be 20");
static_assert(offsetof(BlendFuncSeparate, header) == 0,
              "offset of BlendFuncSeparate header should be 0");
static_assert(offsetof(BlendFuncSeparate, srcRGB) == 4,
              "offset of BlendFuncSeparate srcRGB should be 4");
static_assert(offsetof(BlendFuncSeparate, dstRGB) == 8,
              "offset of BlendFuncSeparate dstRGB should be 8");
static_assert(offsetof(BlendFuncSeparate, srcAlpha) == 12,
              "offset of BlendFuncSeparate srcAlpha should be 12");
static_assert(offsetof(BlendFuncSeparate, dstAlpha) == 16,
              "offset of BlendFuncSeparate dstAlpha should be 16");

struct BufferData {
  typedef BufferData ValueType;
  static const CommandId kCmdId = kBufferData;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLsizeiptr _size,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset,
            GLenum _usage) {
    SetHeader();
    target = _target;
    size = _size;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
    usage = _usage;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizeiptr _size,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset,
            GLenum _usage) {
    static_cast<ValueType*>(cmd)->Init(_target, _size, _data_shm_id,
                                       _data_shm_offset, _usage);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t size;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  uint32_t usage;
};

static_assert(sizeof(BufferData) == 24, "size of BufferData should be 24");
static_assert(offsetof(BufferData, header) == 0,
              "offset of BufferData header should be 0");
static_assert(offsetof(BufferData, target) == 4,
              "offset of BufferData target should be 4");
static_assert(offsetof(BufferData, size) == 8,
              "offset of BufferData size should be 8");
static_assert(offsetof(BufferData, data_shm_id) == 12,
              "offset of BufferData data_shm_id should be 12");
static_assert(offsetof(BufferData, data_shm_offset) == 16,
              "offset of BufferData data_shm_offset should be 16");
static_assert(offsetof(BufferData, usage) == 20,
              "offset of BufferData usage should be 20");

struct BufferSubData {
  typedef BufferSubData ValueType;
  static const CommandId kCmdId = kBufferSubData;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLintptr _offset,
            GLsizeiptr _size,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    target = _target;
    offset = _offset;
    size = _size;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLintptr _offset,
            GLsizeiptr _size,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _offset, _size, _data_shm_id,
                                       _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t offset;
  int32_t size;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
};

static_assert(sizeof(BufferSubData) == 24,
              "size of BufferSubData should be 24");
static_assert(offsetof(BufferSubData, header) == 0,
              "offset of BufferSubData header should be 0");
static_assert(offsetof(BufferSubData, target) == 4,
              "offset of BufferSubData target should be 4");
static_assert(offsetof(BufferSubData, offset) == 8,
              "offset of BufferSubData offset should be 8");
static_assert(offsetof(BufferSubData, size) == 12,
              "offset of BufferSubData size should be 12");
static_assert(offsetof(BufferSubData, data_shm_id) == 16,
              "offset of BufferSubData data_shm_id should be 16");
static_assert(offsetof(BufferSubData, data_shm_offset) == 20,
              "offset of BufferSubData data_shm_offset should be 20");

struct CheckFramebufferStatus {
  typedef CheckFramebufferStatus ValueType;
  static const CommandId kCmdId = kCheckFramebufferStatus;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLenum Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    target = _target;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(CheckFramebufferStatus) == 16,
              "size of CheckFramebufferStatus should be 16");
static_assert(offsetof(CheckFramebufferStatus, header) == 0,
              "offset of CheckFramebufferStatus header should be 0");
static_assert(offsetof(CheckFramebufferStatus, target) == 4,
              "offset of CheckFramebufferStatus target should be 4");
static_assert(offsetof(CheckFramebufferStatus, result_shm_id) == 8,
              "offset of CheckFramebufferStatus result_shm_id should be 8");
static_assert(
    offsetof(CheckFramebufferStatus, result_shm_offset) == 12,
    "offset of CheckFramebufferStatus result_shm_offset should be 12");

struct Clear {
  typedef Clear ValueType;
  static const CommandId kCmdId = kClear;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLbitfield _mask) {
    SetHeader();
    mask = _mask;
  }

  void* Set(void* cmd, GLbitfield _mask) {
    static_cast<ValueType*>(cmd)->Init(_mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mask;
};

static_assert(sizeof(Clear) == 8, "size of Clear should be 8");
static_assert(offsetof(Clear, header) == 0,
              "offset of Clear header should be 0");
static_assert(offsetof(Clear, mask) == 4, "offset of Clear mask should be 4");

struct ClearBufferfi {
  typedef ClearBufferfi ValueType;
  static const CommandId kCmdId = kClearBufferfi;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _buffer,
            GLint _drawbuffers,
            GLfloat _depth,
            GLint _stencil) {
    SetHeader();
    buffer = _buffer;
    drawbuffers = _drawbuffers;
    depth = _depth;
    stencil = _stencil;
  }

  void* Set(void* cmd,
            GLenum _buffer,
            GLint _drawbuffers,
            GLfloat _depth,
            GLint _stencil) {
    static_cast<ValueType*>(cmd)->Init(_buffer, _drawbuffers, _depth, _stencil);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t buffer;
  int32_t drawbuffers;
  float depth;
  int32_t stencil;
};

static_assert(sizeof(ClearBufferfi) == 20,
              "size of ClearBufferfi should be 20");
static_assert(offsetof(ClearBufferfi, header) == 0,
              "offset of ClearBufferfi header should be 0");
static_assert(offsetof(ClearBufferfi, buffer) == 4,
              "offset of ClearBufferfi buffer should be 4");
static_assert(offsetof(ClearBufferfi, drawbuffers) == 8,
              "offset of ClearBufferfi drawbuffers should be 8");
static_assert(offsetof(ClearBufferfi, depth) == 12,
              "offset of ClearBufferfi depth should be 12");
static_assert(offsetof(ClearBufferfi, stencil) == 16,
              "offset of ClearBufferfi stencil should be 16");

struct ClearBufferfvImmediate {
  typedef ClearBufferfvImmediate ValueType;
  static const CommandId kCmdId = kClearBufferfvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 4);
  }

  static uint32_t ComputeEffectiveDataSize(GLenum buffer) {
    return static_cast<uint32_t>(sizeof(GLfloat) *
                                 GLES2Util::CalcClearBufferfvDataCount(buffer));
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLenum _buffer, GLint _drawbuffers, const GLfloat* _value) {
    SetHeader();
    buffer = _buffer;
    drawbuffers = _drawbuffers;
    memcpy(ImmediateDataAddress(this), _value,
           ComputeEffectiveDataSize(buffer));
    DCHECK_GE(ComputeDataSize(), ComputeEffectiveDataSize(buffer));
    char* pointer = reinterpret_cast<char*>(ImmediateDataAddress(this)) +
                    ComputeEffectiveDataSize(buffer);
    memset(pointer, 0, ComputeDataSize() - ComputeEffectiveDataSize(buffer));
  }

  void* Set(void* cmd,
            GLenum _buffer,
            GLint _drawbuffers,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_buffer, _drawbuffers, _value);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t buffer;
  int32_t drawbuffers;
};

static_assert(sizeof(ClearBufferfvImmediate) == 12,
              "size of ClearBufferfvImmediate should be 12");
static_assert(offsetof(ClearBufferfvImmediate, header) == 0,
              "offset of ClearBufferfvImmediate header should be 0");
static_assert(offsetof(ClearBufferfvImmediate, buffer) == 4,
              "offset of ClearBufferfvImmediate buffer should be 4");
static_assert(offsetof(ClearBufferfvImmediate, drawbuffers) == 8,
              "offset of ClearBufferfvImmediate drawbuffers should be 8");

struct ClearBufferivImmediate {
  typedef ClearBufferivImmediate ValueType;
  static const CommandId kCmdId = kClearBufferivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLint) * 4);
  }

  static uint32_t ComputeEffectiveDataSize(GLenum buffer) {
    return static_cast<uint32_t>(sizeof(GLint) *
                                 GLES2Util::CalcClearBufferivDataCount(buffer));
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLenum _buffer, GLint _drawbuffers, const GLint* _value) {
    SetHeader();
    buffer = _buffer;
    drawbuffers = _drawbuffers;
    memcpy(ImmediateDataAddress(this), _value,
           ComputeEffectiveDataSize(buffer));
    DCHECK_GE(ComputeDataSize(), ComputeEffectiveDataSize(buffer));
    char* pointer = reinterpret_cast<char*>(ImmediateDataAddress(this)) +
                    ComputeEffectiveDataSize(buffer);
    memset(pointer, 0, ComputeDataSize() - ComputeEffectiveDataSize(buffer));
  }

  void* Set(void* cmd,
            GLenum _buffer,
            GLint _drawbuffers,
            const GLint* _value) {
    static_cast<ValueType*>(cmd)->Init(_buffer, _drawbuffers, _value);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t buffer;
  int32_t drawbuffers;
};

static_assert(sizeof(ClearBufferivImmediate) == 12,
              "size of ClearBufferivImmediate should be 12");
static_assert(offsetof(ClearBufferivImmediate, header) == 0,
              "offset of ClearBufferivImmediate header should be 0");
static_assert(offsetof(ClearBufferivImmediate, buffer) == 4,
              "offset of ClearBufferivImmediate buffer should be 4");
static_assert(offsetof(ClearBufferivImmediate, drawbuffers) == 8,
              "offset of ClearBufferivImmediate drawbuffers should be 8");

struct ClearBufferuivImmediate {
  typedef ClearBufferuivImmediate ValueType;
  static const CommandId kCmdId = kClearBufferuivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLuint) * 4);
  }

  static uint32_t ComputeEffectiveDataSize(GLenum buffer) {
    return static_cast<uint32_t>(
        sizeof(GLuint) * GLES2Util::CalcClearBufferuivDataCount(buffer));
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLenum _buffer, GLint _drawbuffers, const GLuint* _value) {
    SetHeader();
    buffer = _buffer;
    drawbuffers = _drawbuffers;
    memcpy(ImmediateDataAddress(this), _value,
           ComputeEffectiveDataSize(buffer));
    DCHECK_GE(ComputeDataSize(), ComputeEffectiveDataSize(buffer));
    char* pointer = reinterpret_cast<char*>(ImmediateDataAddress(this)) +
                    ComputeEffectiveDataSize(buffer);
    memset(pointer, 0, ComputeDataSize() - ComputeEffectiveDataSize(buffer));
  }

  void* Set(void* cmd,
            GLenum _buffer,
            GLint _drawbuffers,
            const GLuint* _value) {
    static_cast<ValueType*>(cmd)->Init(_buffer, _drawbuffers, _value);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t buffer;
  int32_t drawbuffers;
};

static_assert(sizeof(ClearBufferuivImmediate) == 12,
              "size of ClearBufferuivImmediate should be 12");
static_assert(offsetof(ClearBufferuivImmediate, header) == 0,
              "offset of ClearBufferuivImmediate header should be 0");
static_assert(offsetof(ClearBufferuivImmediate, buffer) == 4,
              "offset of ClearBufferuivImmediate buffer should be 4");
static_assert(offsetof(ClearBufferuivImmediate, drawbuffers) == 8,
              "offset of ClearBufferuivImmediate drawbuffers should be 8");

struct ClearColor {
  typedef ClearColor ValueType;
  static const CommandId kCmdId = kClearColor;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLclampf _red, GLclampf _green, GLclampf _blue, GLclampf _alpha) {
    SetHeader();
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
  }

  void* Set(void* cmd,
            GLclampf _red,
            GLclampf _green,
            GLclampf _blue,
            GLclampf _alpha) {
    static_cast<ValueType*>(cmd)->Init(_red, _green, _blue, _alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float red;
  float green;
  float blue;
  float alpha;
};

static_assert(sizeof(ClearColor) == 20, "size of ClearColor should be 20");
static_assert(offsetof(ClearColor, header) == 0,
              "offset of ClearColor header should be 0");
static_assert(offsetof(ClearColor, red) == 4,
              "offset of ClearColor red should be 4");
static_assert(offsetof(ClearColor, green) == 8,
              "offset of ClearColor green should be 8");
static_assert(offsetof(ClearColor, blue) == 12,
              "offset of ClearColor blue should be 12");
static_assert(offsetof(ClearColor, alpha) == 16,
              "offset of ClearColor alpha should be 16");

struct ClearDepthf {
  typedef ClearDepthf ValueType;
  static const CommandId kCmdId = kClearDepthf;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLclampf _depth) {
    SetHeader();
    depth = _depth;
  }

  void* Set(void* cmd, GLclampf _depth) {
    static_cast<ValueType*>(cmd)->Init(_depth);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float depth;
};

static_assert(sizeof(ClearDepthf) == 8, "size of ClearDepthf should be 8");
static_assert(offsetof(ClearDepthf, header) == 0,
              "offset of ClearDepthf header should be 0");
static_assert(offsetof(ClearDepthf, depth) == 4,
              "offset of ClearDepthf depth should be 4");

struct ClearStencil {
  typedef ClearStencil ValueType;
  static const CommandId kCmdId = kClearStencil;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _s) {
    SetHeader();
    s = _s;
  }

  void* Set(void* cmd, GLint _s) {
    static_cast<ValueType*>(cmd)->Init(_s);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t s;
};

static_assert(sizeof(ClearStencil) == 8, "size of ClearStencil should be 8");
static_assert(offsetof(ClearStencil, header) == 0,
              "offset of ClearStencil header should be 0");
static_assert(offsetof(ClearStencil, s) == 4,
              "offset of ClearStencil s should be 4");

struct ClientWaitSync {
  typedef ClientWaitSync ValueType;
  static const CommandId kCmdId = kClientWaitSync;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef GLenum Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sync,
            GLbitfield _flags,
            GLuint64 _timeout,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    sync = _sync;
    flags = _flags;
    GLES2Util::MapUint64ToTwoUint32(static_cast<uint64_t>(_timeout), &timeout_0,
                                    &timeout_1);
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _sync,
            GLbitfield _flags,
            GLuint64 _timeout,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_sync, _flags, _timeout, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 timeout() const volatile {
    return static_cast<GLuint64>(
        GLES2Util::MapTwoUint32ToUint64(timeout_0, timeout_1));
  }

  gpu::CommandHeader header;
  uint32_t sync;
  uint32_t flags;
  uint32_t timeout_0;
  uint32_t timeout_1;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(ClientWaitSync) == 28,
              "size of ClientWaitSync should be 28");
static_assert(offsetof(ClientWaitSync, header) == 0,
              "offset of ClientWaitSync header should be 0");
static_assert(offsetof(ClientWaitSync, sync) == 4,
              "offset of ClientWaitSync sync should be 4");
static_assert(offsetof(ClientWaitSync, flags) == 8,
              "offset of ClientWaitSync flags should be 8");
static_assert(offsetof(ClientWaitSync, timeout_0) == 12,
              "offset of ClientWaitSync timeout_0 should be 12");
static_assert(offsetof(ClientWaitSync, timeout_1) == 16,
              "offset of ClientWaitSync timeout_1 should be 16");
static_assert(offsetof(ClientWaitSync, result_shm_id) == 20,
              "offset of ClientWaitSync result_shm_id should be 20");
static_assert(offsetof(ClientWaitSync, result_shm_offset) == 24,
              "offset of ClientWaitSync result_shm_offset should be 24");

struct ColorMask {
  typedef ColorMask ValueType;
  static const CommandId kCmdId = kColorMask;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLboolean _red,
            GLboolean _green,
            GLboolean _blue,
            GLboolean _alpha) {
    SetHeader();
    red = _red;
    green = _green;
    blue = _blue;
    alpha = _alpha;
  }

  void* Set(void* cmd,
            GLboolean _red,
            GLboolean _green,
            GLboolean _blue,
            GLboolean _alpha) {
    static_cast<ValueType*>(cmd)->Init(_red, _green, _blue, _alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t red;
  uint32_t green;
  uint32_t blue;
  uint32_t alpha;
};

static_assert(sizeof(ColorMask) == 20, "size of ColorMask should be 20");
static_assert(offsetof(ColorMask, header) == 0,
              "offset of ColorMask header should be 0");
static_assert(offsetof(ColorMask, red) == 4,
              "offset of ColorMask red should be 4");
static_assert(offsetof(ColorMask, green) == 8,
              "offset of ColorMask green should be 8");
static_assert(offsetof(ColorMask, blue) == 12,
              "offset of ColorMask blue should be 12");
static_assert(offsetof(ColorMask, alpha) == 16,
              "offset of ColorMask alpha should be 16");

struct CompileShader {
  typedef CompileShader ValueType;
  static const CommandId kCmdId = kCompileShader;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader) {
    SetHeader();
    shader = _shader;
  }

  void* Set(void* cmd, GLuint _shader) {
    static_cast<ValueType*>(cmd)->Init(_shader);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
};

static_assert(sizeof(CompileShader) == 8, "size of CompileShader should be 8");
static_assert(offsetof(CompileShader, header) == 0,
              "offset of CompileShader header should be 0");
static_assert(offsetof(CompileShader, shader) == 4,
              "offset of CompileShader shader should be 4");

struct CompressedTexImage2DBucket {
  typedef CompressedTexImage2DBucket ValueType;
  static const CommandId kCmdId = kCompressedTexImage2DBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLuint _bucket_id) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLuint _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
  uint32_t bucket_id;
  static const int32_t border = 0;
};

static_assert(sizeof(CompressedTexImage2DBucket) == 28,
              "size of CompressedTexImage2DBucket should be 28");
static_assert(offsetof(CompressedTexImage2DBucket, header) == 0,
              "offset of CompressedTexImage2DBucket header should be 0");
static_assert(offsetof(CompressedTexImage2DBucket, target) == 4,
              "offset of CompressedTexImage2DBucket target should be 4");
static_assert(offsetof(CompressedTexImage2DBucket, level) == 8,
              "offset of CompressedTexImage2DBucket level should be 8");
static_assert(
    offsetof(CompressedTexImage2DBucket, internalformat) == 12,
    "offset of CompressedTexImage2DBucket internalformat should be 12");
static_assert(offsetof(CompressedTexImage2DBucket, width) == 16,
              "offset of CompressedTexImage2DBucket width should be 16");
static_assert(offsetof(CompressedTexImage2DBucket, height) == 20,
              "offset of CompressedTexImage2DBucket height should be 20");
static_assert(offsetof(CompressedTexImage2DBucket, bucket_id) == 24,
              "offset of CompressedTexImage2DBucket bucket_id should be 24");

struct CompressedTexImage2D {
  typedef CompressedTexImage2D ValueType;
  static const CommandId kCmdId = kCompressedTexImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    imageSize = _imageSize;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _imageSize, _data_shm_id,
                                       _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
  int32_t imageSize;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  static const int32_t border = 0;
};

static_assert(sizeof(CompressedTexImage2D) == 36,
              "size of CompressedTexImage2D should be 36");
static_assert(offsetof(CompressedTexImage2D, header) == 0,
              "offset of CompressedTexImage2D header should be 0");
static_assert(offsetof(CompressedTexImage2D, target) == 4,
              "offset of CompressedTexImage2D target should be 4");
static_assert(offsetof(CompressedTexImage2D, level) == 8,
              "offset of CompressedTexImage2D level should be 8");
static_assert(offsetof(CompressedTexImage2D, internalformat) == 12,
              "offset of CompressedTexImage2D internalformat should be 12");
static_assert(offsetof(CompressedTexImage2D, width) == 16,
              "offset of CompressedTexImage2D width should be 16");
static_assert(offsetof(CompressedTexImage2D, height) == 20,
              "offset of CompressedTexImage2D height should be 20");
static_assert(offsetof(CompressedTexImage2D, imageSize) == 24,
              "offset of CompressedTexImage2D imageSize should be 24");
static_assert(offsetof(CompressedTexImage2D, data_shm_id) == 28,
              "offset of CompressedTexImage2D data_shm_id should be 28");
static_assert(offsetof(CompressedTexImage2D, data_shm_offset) == 32,
              "offset of CompressedTexImage2D data_shm_offset should be 32");

struct CompressedTexSubImage2DBucket {
  typedef CompressedTexSubImage2DBucket ValueType;
  static const CommandId kCmdId = kCompressedTexSubImage2DBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLuint _bucket_id) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    width = _width;
    height = _height;
    format = _format;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLuint _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _xoffset, _yoffset,
                                       _width, _height, _format, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t width;
  int32_t height;
  uint32_t format;
  uint32_t bucket_id;
};

static_assert(sizeof(CompressedTexSubImage2DBucket) == 36,
              "size of CompressedTexSubImage2DBucket should be 36");
static_assert(offsetof(CompressedTexSubImage2DBucket, header) == 0,
              "offset of CompressedTexSubImage2DBucket header should be 0");
static_assert(offsetof(CompressedTexSubImage2DBucket, target) == 4,
              "offset of CompressedTexSubImage2DBucket target should be 4");
static_assert(offsetof(CompressedTexSubImage2DBucket, level) == 8,
              "offset of CompressedTexSubImage2DBucket level should be 8");
static_assert(offsetof(CompressedTexSubImage2DBucket, xoffset) == 12,
              "offset of CompressedTexSubImage2DBucket xoffset should be 12");
static_assert(offsetof(CompressedTexSubImage2DBucket, yoffset) == 16,
              "offset of CompressedTexSubImage2DBucket yoffset should be 16");
static_assert(offsetof(CompressedTexSubImage2DBucket, width) == 20,
              "offset of CompressedTexSubImage2DBucket width should be 20");
static_assert(offsetof(CompressedTexSubImage2DBucket, height) == 24,
              "offset of CompressedTexSubImage2DBucket height should be 24");
static_assert(offsetof(CompressedTexSubImage2DBucket, format) == 28,
              "offset of CompressedTexSubImage2DBucket format should be 28");
static_assert(offsetof(CompressedTexSubImage2DBucket, bucket_id) == 32,
              "offset of CompressedTexSubImage2DBucket bucket_id should be 32");

struct CompressedTexSubImage2D {
  typedef CompressedTexSubImage2D ValueType;
  static const CommandId kCmdId = kCompressedTexSubImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    width = _width;
    height = _height;
    format = _format;
    imageSize = _imageSize;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _xoffset, _yoffset,
                                       _width, _height, _format, _imageSize,
                                       _data_shm_id, _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t width;
  int32_t height;
  uint32_t format;
  int32_t imageSize;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
};

static_assert(sizeof(CompressedTexSubImage2D) == 44,
              "size of CompressedTexSubImage2D should be 44");
static_assert(offsetof(CompressedTexSubImage2D, header) == 0,
              "offset of CompressedTexSubImage2D header should be 0");
static_assert(offsetof(CompressedTexSubImage2D, target) == 4,
              "offset of CompressedTexSubImage2D target should be 4");
static_assert(offsetof(CompressedTexSubImage2D, level) == 8,
              "offset of CompressedTexSubImage2D level should be 8");
static_assert(offsetof(CompressedTexSubImage2D, xoffset) == 12,
              "offset of CompressedTexSubImage2D xoffset should be 12");
static_assert(offsetof(CompressedTexSubImage2D, yoffset) == 16,
              "offset of CompressedTexSubImage2D yoffset should be 16");
static_assert(offsetof(CompressedTexSubImage2D, width) == 20,
              "offset of CompressedTexSubImage2D width should be 20");
static_assert(offsetof(CompressedTexSubImage2D, height) == 24,
              "offset of CompressedTexSubImage2D height should be 24");
static_assert(offsetof(CompressedTexSubImage2D, format) == 28,
              "offset of CompressedTexSubImage2D format should be 28");
static_assert(offsetof(CompressedTexSubImage2D, imageSize) == 32,
              "offset of CompressedTexSubImage2D imageSize should be 32");
static_assert(offsetof(CompressedTexSubImage2D, data_shm_id) == 36,
              "offset of CompressedTexSubImage2D data_shm_id should be 36");
static_assert(offsetof(CompressedTexSubImage2D, data_shm_offset) == 40,
              "offset of CompressedTexSubImage2D data_shm_offset should be 40");

struct CompressedTexImage3DBucket {
  typedef CompressedTexImage3DBucket ValueType;
  static const CommandId kCmdId = kCompressedTexImage3DBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLuint _bucket_id) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    depth = _depth;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLuint _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _depth, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
  int32_t depth;
  uint32_t bucket_id;
  static const int32_t border = 0;
};

static_assert(sizeof(CompressedTexImage3DBucket) == 32,
              "size of CompressedTexImage3DBucket should be 32");
static_assert(offsetof(CompressedTexImage3DBucket, header) == 0,
              "offset of CompressedTexImage3DBucket header should be 0");
static_assert(offsetof(CompressedTexImage3DBucket, target) == 4,
              "offset of CompressedTexImage3DBucket target should be 4");
static_assert(offsetof(CompressedTexImage3DBucket, level) == 8,
              "offset of CompressedTexImage3DBucket level should be 8");
static_assert(
    offsetof(CompressedTexImage3DBucket, internalformat) == 12,
    "offset of CompressedTexImage3DBucket internalformat should be 12");
static_assert(offsetof(CompressedTexImage3DBucket, width) == 16,
              "offset of CompressedTexImage3DBucket width should be 16");
static_assert(offsetof(CompressedTexImage3DBucket, height) == 20,
              "offset of CompressedTexImage3DBucket height should be 20");
static_assert(offsetof(CompressedTexImage3DBucket, depth) == 24,
              "offset of CompressedTexImage3DBucket depth should be 24");
static_assert(offsetof(CompressedTexImage3DBucket, bucket_id) == 28,
              "offset of CompressedTexImage3DBucket bucket_id should be 28");

struct CompressedTexImage3D {
  typedef CompressedTexImage3D ValueType;
  static const CommandId kCmdId = kCompressedTexImage3D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    depth = _depth;
    imageSize = _imageSize;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _depth, _imageSize,
                                       _data_shm_id, _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
  int32_t depth;
  int32_t imageSize;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  static const int32_t border = 0;
};

static_assert(sizeof(CompressedTexImage3D) == 40,
              "size of CompressedTexImage3D should be 40");
static_assert(offsetof(CompressedTexImage3D, header) == 0,
              "offset of CompressedTexImage3D header should be 0");
static_assert(offsetof(CompressedTexImage3D, target) == 4,
              "offset of CompressedTexImage3D target should be 4");
static_assert(offsetof(CompressedTexImage3D, level) == 8,
              "offset of CompressedTexImage3D level should be 8");
static_assert(offsetof(CompressedTexImage3D, internalformat) == 12,
              "offset of CompressedTexImage3D internalformat should be 12");
static_assert(offsetof(CompressedTexImage3D, width) == 16,
              "offset of CompressedTexImage3D width should be 16");
static_assert(offsetof(CompressedTexImage3D, height) == 20,
              "offset of CompressedTexImage3D height should be 20");
static_assert(offsetof(CompressedTexImage3D, depth) == 24,
              "offset of CompressedTexImage3D depth should be 24");
static_assert(offsetof(CompressedTexImage3D, imageSize) == 28,
              "offset of CompressedTexImage3D imageSize should be 28");
static_assert(offsetof(CompressedTexImage3D, data_shm_id) == 32,
              "offset of CompressedTexImage3D data_shm_id should be 32");
static_assert(offsetof(CompressedTexImage3D, data_shm_offset) == 36,
              "offset of CompressedTexImage3D data_shm_offset should be 36");

struct CompressedTexSubImage3DBucket {
  typedef CompressedTexSubImage3DBucket ValueType;
  static const CommandId kCmdId = kCompressedTexSubImage3DBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLuint _bucket_id) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    zoffset = _zoffset;
    width = _width;
    height = _height;
    depth = _depth;
    format = _format;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLuint _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _xoffset, _yoffset,
                                       _zoffset, _width, _height, _depth,
                                       _format, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t zoffset;
  int32_t width;
  int32_t height;
  int32_t depth;
  uint32_t format;
  uint32_t bucket_id;
};

static_assert(sizeof(CompressedTexSubImage3DBucket) == 44,
              "size of CompressedTexSubImage3DBucket should be 44");
static_assert(offsetof(CompressedTexSubImage3DBucket, header) == 0,
              "offset of CompressedTexSubImage3DBucket header should be 0");
static_assert(offsetof(CompressedTexSubImage3DBucket, target) == 4,
              "offset of CompressedTexSubImage3DBucket target should be 4");
static_assert(offsetof(CompressedTexSubImage3DBucket, level) == 8,
              "offset of CompressedTexSubImage3DBucket level should be 8");
static_assert(offsetof(CompressedTexSubImage3DBucket, xoffset) == 12,
              "offset of CompressedTexSubImage3DBucket xoffset should be 12");
static_assert(offsetof(CompressedTexSubImage3DBucket, yoffset) == 16,
              "offset of CompressedTexSubImage3DBucket yoffset should be 16");
static_assert(offsetof(CompressedTexSubImage3DBucket, zoffset) == 20,
              "offset of CompressedTexSubImage3DBucket zoffset should be 20");
static_assert(offsetof(CompressedTexSubImage3DBucket, width) == 24,
              "offset of CompressedTexSubImage3DBucket width should be 24");
static_assert(offsetof(CompressedTexSubImage3DBucket, height) == 28,
              "offset of CompressedTexSubImage3DBucket height should be 28");
static_assert(offsetof(CompressedTexSubImage3DBucket, depth) == 32,
              "offset of CompressedTexSubImage3DBucket depth should be 32");
static_assert(offsetof(CompressedTexSubImage3DBucket, format) == 36,
              "offset of CompressedTexSubImage3DBucket format should be 36");
static_assert(offsetof(CompressedTexSubImage3DBucket, bucket_id) == 40,
              "offset of CompressedTexSubImage3DBucket bucket_id should be 40");

struct CompressedTexSubImage3D {
  typedef CompressedTexSubImage3D ValueType;
  static const CommandId kCmdId = kCompressedTexSubImage3D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    zoffset = _zoffset;
    width = _width;
    height = _height;
    depth = _depth;
    format = _format;
    imageSize = _imageSize;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth,
        _format, _imageSize, _data_shm_id, _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t zoffset;
  int32_t width;
  int32_t height;
  int32_t depth;
  uint32_t format;
  int32_t imageSize;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
};

static_assert(sizeof(CompressedTexSubImage3D) == 52,
              "size of CompressedTexSubImage3D should be 52");
static_assert(offsetof(CompressedTexSubImage3D, header) == 0,
              "offset of CompressedTexSubImage3D header should be 0");
static_assert(offsetof(CompressedTexSubImage3D, target) == 4,
              "offset of CompressedTexSubImage3D target should be 4");
static_assert(offsetof(CompressedTexSubImage3D, level) == 8,
              "offset of CompressedTexSubImage3D level should be 8");
static_assert(offsetof(CompressedTexSubImage3D, xoffset) == 12,
              "offset of CompressedTexSubImage3D xoffset should be 12");
static_assert(offsetof(CompressedTexSubImage3D, yoffset) == 16,
              "offset of CompressedTexSubImage3D yoffset should be 16");
static_assert(offsetof(CompressedTexSubImage3D, zoffset) == 20,
              "offset of CompressedTexSubImage3D zoffset should be 20");
static_assert(offsetof(CompressedTexSubImage3D, width) == 24,
              "offset of CompressedTexSubImage3D width should be 24");
static_assert(offsetof(CompressedTexSubImage3D, height) == 28,
              "offset of CompressedTexSubImage3D height should be 28");
static_assert(offsetof(CompressedTexSubImage3D, depth) == 32,
              "offset of CompressedTexSubImage3D depth should be 32");
static_assert(offsetof(CompressedTexSubImage3D, format) == 36,
              "offset of CompressedTexSubImage3D format should be 36");
static_assert(offsetof(CompressedTexSubImage3D, imageSize) == 40,
              "offset of CompressedTexSubImage3D imageSize should be 40");
static_assert(offsetof(CompressedTexSubImage3D, data_shm_id) == 44,
              "offset of CompressedTexSubImage3D data_shm_id should be 44");
static_assert(offsetof(CompressedTexSubImage3D, data_shm_offset) == 48,
              "offset of CompressedTexSubImage3D data_shm_offset should be 48");

struct CopyBufferSubData {
  typedef CopyBufferSubData ValueType;
  static const CommandId kCmdId = kCopyBufferSubData;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _readtarget,
            GLenum _writetarget,
            GLintptr _readoffset,
            GLintptr _writeoffset,
            GLsizeiptr _size) {
    SetHeader();
    readtarget = _readtarget;
    writetarget = _writetarget;
    readoffset = _readoffset;
    writeoffset = _writeoffset;
    size = _size;
  }

  void* Set(void* cmd,
            GLenum _readtarget,
            GLenum _writetarget,
            GLintptr _readoffset,
            GLintptr _writeoffset,
            GLsizeiptr _size) {
    static_cast<ValueType*>(cmd)->Init(_readtarget, _writetarget, _readoffset,
                                       _writeoffset, _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t readtarget;
  uint32_t writetarget;
  int32_t readoffset;
  int32_t writeoffset;
  int32_t size;
};

static_assert(sizeof(CopyBufferSubData) == 24,
              "size of CopyBufferSubData should be 24");
static_assert(offsetof(CopyBufferSubData, header) == 0,
              "offset of CopyBufferSubData header should be 0");
static_assert(offsetof(CopyBufferSubData, readtarget) == 4,
              "offset of CopyBufferSubData readtarget should be 4");
static_assert(offsetof(CopyBufferSubData, writetarget) == 8,
              "offset of CopyBufferSubData writetarget should be 8");
static_assert(offsetof(CopyBufferSubData, readoffset) == 12,
              "offset of CopyBufferSubData readoffset should be 12");
static_assert(offsetof(CopyBufferSubData, writeoffset) == 16,
              "offset of CopyBufferSubData writeoffset should be 16");
static_assert(offsetof(CopyBufferSubData, size) == 20,
              "offset of CopyBufferSubData size should be 20");

struct CopyTexImage2D {
  typedef CopyTexImage2D ValueType;
  static const CommandId kCmdId = kCopyTexImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _x, _y,
                                       _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  uint32_t internalformat;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  static const int32_t border = 0;
};

static_assert(sizeof(CopyTexImage2D) == 32,
              "size of CopyTexImage2D should be 32");
static_assert(offsetof(CopyTexImage2D, header) == 0,
              "offset of CopyTexImage2D header should be 0");
static_assert(offsetof(CopyTexImage2D, target) == 4,
              "offset of CopyTexImage2D target should be 4");
static_assert(offsetof(CopyTexImage2D, level) == 8,
              "offset of CopyTexImage2D level should be 8");
static_assert(offsetof(CopyTexImage2D, internalformat) == 12,
              "offset of CopyTexImage2D internalformat should be 12");
static_assert(offsetof(CopyTexImage2D, x) == 16,
              "offset of CopyTexImage2D x should be 16");
static_assert(offsetof(CopyTexImage2D, y) == 20,
              "offset of CopyTexImage2D y should be 20");
static_assert(offsetof(CopyTexImage2D, width) == 24,
              "offset of CopyTexImage2D width should be 24");
static_assert(offsetof(CopyTexImage2D, height) == 28,
              "offset of CopyTexImage2D height should be 28");

struct CopyTexSubImage2D {
  typedef CopyTexSubImage2D ValueType;
  static const CommandId kCmdId = kCopyTexSubImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _xoffset, _yoffset, _x,
                                       _y, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(CopyTexSubImage2D) == 36,
              "size of CopyTexSubImage2D should be 36");
static_assert(offsetof(CopyTexSubImage2D, header) == 0,
              "offset of CopyTexSubImage2D header should be 0");
static_assert(offsetof(CopyTexSubImage2D, target) == 4,
              "offset of CopyTexSubImage2D target should be 4");
static_assert(offsetof(CopyTexSubImage2D, level) == 8,
              "offset of CopyTexSubImage2D level should be 8");
static_assert(offsetof(CopyTexSubImage2D, xoffset) == 12,
              "offset of CopyTexSubImage2D xoffset should be 12");
static_assert(offsetof(CopyTexSubImage2D, yoffset) == 16,
              "offset of CopyTexSubImage2D yoffset should be 16");
static_assert(offsetof(CopyTexSubImage2D, x) == 20,
              "offset of CopyTexSubImage2D x should be 20");
static_assert(offsetof(CopyTexSubImage2D, y) == 24,
              "offset of CopyTexSubImage2D y should be 24");
static_assert(offsetof(CopyTexSubImage2D, width) == 28,
              "offset of CopyTexSubImage2D width should be 28");
static_assert(offsetof(CopyTexSubImage2D, height) == 32,
              "offset of CopyTexSubImage2D height should be 32");

struct CopyTexSubImage3D {
  typedef CopyTexSubImage3D ValueType;
  static const CommandId kCmdId = kCopyTexSubImage3D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    zoffset = _zoffset;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _xoffset, _yoffset,
                                       _zoffset, _x, _y, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t zoffset;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(CopyTexSubImage3D) == 40,
              "size of CopyTexSubImage3D should be 40");
static_assert(offsetof(CopyTexSubImage3D, header) == 0,
              "offset of CopyTexSubImage3D header should be 0");
static_assert(offsetof(CopyTexSubImage3D, target) == 4,
              "offset of CopyTexSubImage3D target should be 4");
static_assert(offsetof(CopyTexSubImage3D, level) == 8,
              "offset of CopyTexSubImage3D level should be 8");
static_assert(offsetof(CopyTexSubImage3D, xoffset) == 12,
              "offset of CopyTexSubImage3D xoffset should be 12");
static_assert(offsetof(CopyTexSubImage3D, yoffset) == 16,
              "offset of CopyTexSubImage3D yoffset should be 16");
static_assert(offsetof(CopyTexSubImage3D, zoffset) == 20,
              "offset of CopyTexSubImage3D zoffset should be 20");
static_assert(offsetof(CopyTexSubImage3D, x) == 24,
              "offset of CopyTexSubImage3D x should be 24");
static_assert(offsetof(CopyTexSubImage3D, y) == 28,
              "offset of CopyTexSubImage3D y should be 28");
static_assert(offsetof(CopyTexSubImage3D, width) == 32,
              "offset of CopyTexSubImage3D width should be 32");
static_assert(offsetof(CopyTexSubImage3D, height) == 36,
              "offset of CopyTexSubImage3D height should be 36");

struct CreateProgram {
  typedef CreateProgram ValueType;
  static const CommandId kCmdId = kCreateProgram;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _client_id) {
    SetHeader();
    client_id = _client_id;
  }

  void* Set(void* cmd, uint32_t _client_id) {
    static_cast<ValueType*>(cmd)->Init(_client_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t client_id;
};

static_assert(sizeof(CreateProgram) == 8, "size of CreateProgram should be 8");
static_assert(offsetof(CreateProgram, header) == 0,
              "offset of CreateProgram header should be 0");
static_assert(offsetof(CreateProgram, client_id) == 4,
              "offset of CreateProgram client_id should be 4");

struct CreateShader {
  typedef CreateShader ValueType;
  static const CommandId kCmdId = kCreateShader;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _type, uint32_t _client_id) {
    SetHeader();
    type = _type;
    client_id = _client_id;
  }

  void* Set(void* cmd, GLenum _type, uint32_t _client_id) {
    static_cast<ValueType*>(cmd)->Init(_type, _client_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t type;
  uint32_t client_id;
};

static_assert(sizeof(CreateShader) == 12, "size of CreateShader should be 12");
static_assert(offsetof(CreateShader, header) == 0,
              "offset of CreateShader header should be 0");
static_assert(offsetof(CreateShader, type) == 4,
              "offset of CreateShader type should be 4");
static_assert(offsetof(CreateShader, client_id) == 8,
              "offset of CreateShader client_id should be 8");

struct CullFace {
  typedef CullFace ValueType;
  static const CommandId kCmdId = kCullFace;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode) {
    SetHeader();
    mode = _mode;
  }

  void* Set(void* cmd, GLenum _mode) {
    static_cast<ValueType*>(cmd)->Init(_mode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
};

static_assert(sizeof(CullFace) == 8, "size of CullFace should be 8");
static_assert(offsetof(CullFace, header) == 0,
              "offset of CullFace header should be 0");
static_assert(offsetof(CullFace, mode) == 4,
              "offset of CullFace mode should be 4");

struct DeleteBuffersImmediate {
  typedef DeleteBuffersImmediate ValueType;
  static const CommandId kCmdId = kDeleteBuffersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _buffers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _buffers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _buffers) {
    static_cast<ValueType*>(cmd)->Init(_n, _buffers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteBuffersImmediate) == 8,
              "size of DeleteBuffersImmediate should be 8");
static_assert(offsetof(DeleteBuffersImmediate, header) == 0,
              "offset of DeleteBuffersImmediate header should be 0");
static_assert(offsetof(DeleteBuffersImmediate, n) == 4,
              "offset of DeleteBuffersImmediate n should be 4");

struct DeleteFramebuffersImmediate {
  typedef DeleteFramebuffersImmediate ValueType;
  static const CommandId kCmdId = kDeleteFramebuffersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _framebuffers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _framebuffers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _framebuffers) {
    static_cast<ValueType*>(cmd)->Init(_n, _framebuffers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteFramebuffersImmediate) == 8,
              "size of DeleteFramebuffersImmediate should be 8");
static_assert(offsetof(DeleteFramebuffersImmediate, header) == 0,
              "offset of DeleteFramebuffersImmediate header should be 0");
static_assert(offsetof(DeleteFramebuffersImmediate, n) == 4,
              "offset of DeleteFramebuffersImmediate n should be 4");

struct DeleteProgram {
  typedef DeleteProgram ValueType;
  static const CommandId kCmdId = kDeleteProgram;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program) {
    SetHeader();
    program = _program;
  }

  void* Set(void* cmd, GLuint _program) {
    static_cast<ValueType*>(cmd)->Init(_program);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
};

static_assert(sizeof(DeleteProgram) == 8, "size of DeleteProgram should be 8");
static_assert(offsetof(DeleteProgram, header) == 0,
              "offset of DeleteProgram header should be 0");
static_assert(offsetof(DeleteProgram, program) == 4,
              "offset of DeleteProgram program should be 4");

struct DeleteRenderbuffersImmediate {
  typedef DeleteRenderbuffersImmediate ValueType;
  static const CommandId kCmdId = kDeleteRenderbuffersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _renderbuffers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _renderbuffers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _renderbuffers) {
    static_cast<ValueType*>(cmd)->Init(_n, _renderbuffers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteRenderbuffersImmediate) == 8,
              "size of DeleteRenderbuffersImmediate should be 8");
static_assert(offsetof(DeleteRenderbuffersImmediate, header) == 0,
              "offset of DeleteRenderbuffersImmediate header should be 0");
static_assert(offsetof(DeleteRenderbuffersImmediate, n) == 4,
              "offset of DeleteRenderbuffersImmediate n should be 4");

struct DeleteSamplersImmediate {
  typedef DeleteSamplersImmediate ValueType;
  static const CommandId kCmdId = kDeleteSamplersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _samplers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _samplers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _samplers) {
    static_cast<ValueType*>(cmd)->Init(_n, _samplers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteSamplersImmediate) == 8,
              "size of DeleteSamplersImmediate should be 8");
static_assert(offsetof(DeleteSamplersImmediate, header) == 0,
              "offset of DeleteSamplersImmediate header should be 0");
static_assert(offsetof(DeleteSamplersImmediate, n) == 4,
              "offset of DeleteSamplersImmediate n should be 4");

struct DeleteSync {
  typedef DeleteSync ValueType;
  static const CommandId kCmdId = kDeleteSync;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sync) {
    SetHeader();
    sync = _sync;
  }

  void* Set(void* cmd, GLuint _sync) {
    static_cast<ValueType*>(cmd)->Init(_sync);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sync;
};

static_assert(sizeof(DeleteSync) == 8, "size of DeleteSync should be 8");
static_assert(offsetof(DeleteSync, header) == 0,
              "offset of DeleteSync header should be 0");
static_assert(offsetof(DeleteSync, sync) == 4,
              "offset of DeleteSync sync should be 4");

struct DeleteShader {
  typedef DeleteShader ValueType;
  static const CommandId kCmdId = kDeleteShader;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader) {
    SetHeader();
    shader = _shader;
  }

  void* Set(void* cmd, GLuint _shader) {
    static_cast<ValueType*>(cmd)->Init(_shader);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
};

static_assert(sizeof(DeleteShader) == 8, "size of DeleteShader should be 8");
static_assert(offsetof(DeleteShader, header) == 0,
              "offset of DeleteShader header should be 0");
static_assert(offsetof(DeleteShader, shader) == 4,
              "offset of DeleteShader shader should be 4");

struct DeleteTexturesImmediate {
  typedef DeleteTexturesImmediate ValueType;
  static const CommandId kCmdId = kDeleteTexturesImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _textures) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _textures, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _textures) {
    static_cast<ValueType*>(cmd)->Init(_n, _textures);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteTexturesImmediate) == 8,
              "size of DeleteTexturesImmediate should be 8");
static_assert(offsetof(DeleteTexturesImmediate, header) == 0,
              "offset of DeleteTexturesImmediate header should be 0");
static_assert(offsetof(DeleteTexturesImmediate, n) == 4,
              "offset of DeleteTexturesImmediate n should be 4");

struct DeleteTransformFeedbacksImmediate {
  typedef DeleteTransformFeedbacksImmediate ValueType;
  static const CommandId kCmdId = kDeleteTransformFeedbacksImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _ids) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _ids, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _ids) {
    static_cast<ValueType*>(cmd)->Init(_n, _ids);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteTransformFeedbacksImmediate) == 8,
              "size of DeleteTransformFeedbacksImmediate should be 8");
static_assert(offsetof(DeleteTransformFeedbacksImmediate, header) == 0,
              "offset of DeleteTransformFeedbacksImmediate header should be 0");
static_assert(offsetof(DeleteTransformFeedbacksImmediate, n) == 4,
              "offset of DeleteTransformFeedbacksImmediate n should be 4");

struct DepthFunc {
  typedef DepthFunc ValueType;
  static const CommandId kCmdId = kDepthFunc;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _func) {
    SetHeader();
    func = _func;
  }

  void* Set(void* cmd, GLenum _func) {
    static_cast<ValueType*>(cmd)->Init(_func);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t func;
};

static_assert(sizeof(DepthFunc) == 8, "size of DepthFunc should be 8");
static_assert(offsetof(DepthFunc, header) == 0,
              "offset of DepthFunc header should be 0");
static_assert(offsetof(DepthFunc, func) == 4,
              "offset of DepthFunc func should be 4");

struct DepthMask {
  typedef DepthMask ValueType;
  static const CommandId kCmdId = kDepthMask;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLboolean _flag) {
    SetHeader();
    flag = _flag;
  }

  void* Set(void* cmd, GLboolean _flag) {
    static_cast<ValueType*>(cmd)->Init(_flag);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t flag;
};

static_assert(sizeof(DepthMask) == 8, "size of DepthMask should be 8");
static_assert(offsetof(DepthMask, header) == 0,
              "offset of DepthMask header should be 0");
static_assert(offsetof(DepthMask, flag) == 4,
              "offset of DepthMask flag should be 4");

struct DepthRangef {
  typedef DepthRangef ValueType;
  static const CommandId kCmdId = kDepthRangef;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLclampf _zNear, GLclampf _zFar) {
    SetHeader();
    zNear = _zNear;
    zFar = _zFar;
  }

  void* Set(void* cmd, GLclampf _zNear, GLclampf _zFar) {
    static_cast<ValueType*>(cmd)->Init(_zNear, _zFar);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float zNear;
  float zFar;
};

static_assert(sizeof(DepthRangef) == 12, "size of DepthRangef should be 12");
static_assert(offsetof(DepthRangef, header) == 0,
              "offset of DepthRangef header should be 0");
static_assert(offsetof(DepthRangef, zNear) == 4,
              "offset of DepthRangef zNear should be 4");
static_assert(offsetof(DepthRangef, zFar) == 8,
              "offset of DepthRangef zFar should be 8");

struct DetachShader {
  typedef DetachShader ValueType;
  static const CommandId kCmdId = kDetachShader;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, GLuint _shader) {
    SetHeader();
    program = _program;
    shader = _shader;
  }

  void* Set(void* cmd, GLuint _program, GLuint _shader) {
    static_cast<ValueType*>(cmd)->Init(_program, _shader);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t shader;
};

static_assert(sizeof(DetachShader) == 12, "size of DetachShader should be 12");
static_assert(offsetof(DetachShader, header) == 0,
              "offset of DetachShader header should be 0");
static_assert(offsetof(DetachShader, program) == 4,
              "offset of DetachShader program should be 4");
static_assert(offsetof(DetachShader, shader) == 8,
              "offset of DetachShader shader should be 8");

struct Disable {
  typedef Disable ValueType;
  static const CommandId kCmdId = kDisable;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _cap) {
    SetHeader();
    cap = _cap;
  }

  void* Set(void* cmd, GLenum _cap) {
    static_cast<ValueType*>(cmd)->Init(_cap);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t cap;
};

static_assert(sizeof(Disable) == 8, "size of Disable should be 8");
static_assert(offsetof(Disable, header) == 0,
              "offset of Disable header should be 0");
static_assert(offsetof(Disable, cap) == 4, "offset of Disable cap should be 4");

struct DisableVertexAttribArray {
  typedef DisableVertexAttribArray ValueType;
  static const CommandId kCmdId = kDisableVertexAttribArray;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index) {
    SetHeader();
    index = _index;
  }

  void* Set(void* cmd, GLuint _index) {
    static_cast<ValueType*>(cmd)->Init(_index);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
};

static_assert(sizeof(DisableVertexAttribArray) == 8,
              "size of DisableVertexAttribArray should be 8");
static_assert(offsetof(DisableVertexAttribArray, header) == 0,
              "offset of DisableVertexAttribArray header should be 0");
static_assert(offsetof(DisableVertexAttribArray, index) == 4,
              "offset of DisableVertexAttribArray index should be 4");

struct DrawArrays {
  typedef DrawArrays ValueType;
  static const CommandId kCmdId = kDrawArrays;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode, GLint _first, GLsizei _count) {
    SetHeader();
    mode = _mode;
    first = _first;
    count = _count;
  }

  void* Set(void* cmd, GLenum _mode, GLint _first, GLsizei _count) {
    static_cast<ValueType*>(cmd)->Init(_mode, _first, _count);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  int32_t first;
  int32_t count;
};

static_assert(sizeof(DrawArrays) == 16, "size of DrawArrays should be 16");
static_assert(offsetof(DrawArrays, header) == 0,
              "offset of DrawArrays header should be 0");
static_assert(offsetof(DrawArrays, mode) == 4,
              "offset of DrawArrays mode should be 4");
static_assert(offsetof(DrawArrays, first) == 8,
              "offset of DrawArrays first should be 8");
static_assert(offsetof(DrawArrays, count) == 12,
              "offset of DrawArrays count should be 12");

struct DrawElements {
  typedef DrawElements ValueType;
  static const CommandId kCmdId = kDrawElements;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode, GLsizei _count, GLenum _type, GLuint _index_offset) {
    SetHeader();
    mode = _mode;
    count = _count;
    type = _type;
    index_offset = _index_offset;
  }

  void* Set(void* cmd,
            GLenum _mode,
            GLsizei _count,
            GLenum _type,
            GLuint _index_offset) {
    static_cast<ValueType*>(cmd)->Init(_mode, _count, _type, _index_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  int32_t count;
  uint32_t type;
  uint32_t index_offset;
};

static_assert(sizeof(DrawElements) == 20, "size of DrawElements should be 20");
static_assert(offsetof(DrawElements, header) == 0,
              "offset of DrawElements header should be 0");
static_assert(offsetof(DrawElements, mode) == 4,
              "offset of DrawElements mode should be 4");
static_assert(offsetof(DrawElements, count) == 8,
              "offset of DrawElements count should be 8");
static_assert(offsetof(DrawElements, type) == 12,
              "offset of DrawElements type should be 12");
static_assert(offsetof(DrawElements, index_offset) == 16,
              "offset of DrawElements index_offset should be 16");

struct Enable {
  typedef Enable ValueType;
  static const CommandId kCmdId = kEnable;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _cap) {
    SetHeader();
    cap = _cap;
  }

  void* Set(void* cmd, GLenum _cap) {
    static_cast<ValueType*>(cmd)->Init(_cap);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t cap;
};

static_assert(sizeof(Enable) == 8, "size of Enable should be 8");
static_assert(offsetof(Enable, header) == 0,
              "offset of Enable header should be 0");
static_assert(offsetof(Enable, cap) == 4, "offset of Enable cap should be 4");

struct EnableVertexAttribArray {
  typedef EnableVertexAttribArray ValueType;
  static const CommandId kCmdId = kEnableVertexAttribArray;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index) {
    SetHeader();
    index = _index;
  }

  void* Set(void* cmd, GLuint _index) {
    static_cast<ValueType*>(cmd)->Init(_index);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
};

static_assert(sizeof(EnableVertexAttribArray) == 8,
              "size of EnableVertexAttribArray should be 8");
static_assert(offsetof(EnableVertexAttribArray, header) == 0,
              "offset of EnableVertexAttribArray header should be 0");
static_assert(offsetof(EnableVertexAttribArray, index) == 4,
              "offset of EnableVertexAttribArray index should be 4");

struct FenceSync {
  typedef FenceSync ValueType;
  static const CommandId kCmdId = kFenceSync;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _client_id) {
    SetHeader();
    client_id = _client_id;
  }

  void* Set(void* cmd, uint32_t _client_id) {
    static_cast<ValueType*>(cmd)->Init(_client_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t client_id;
  static const uint32_t condition = GL_SYNC_GPU_COMMANDS_COMPLETE;
  static const uint32_t flags = 0;
};

static_assert(sizeof(FenceSync) == 8, "size of FenceSync should be 8");
static_assert(offsetof(FenceSync, header) == 0,
              "offset of FenceSync header should be 0");
static_assert(offsetof(FenceSync, client_id) == 4,
              "offset of FenceSync client_id should be 4");

struct Finish {
  typedef Finish ValueType;
  static const CommandId kCmdId = kFinish;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(Finish) == 4, "size of Finish should be 4");
static_assert(offsetof(Finish, header) == 0,
              "offset of Finish header should be 0");

struct Flush {
  typedef Flush ValueType;
  static const CommandId kCmdId = kFlush;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(Flush) == 4, "size of Flush should be 4");
static_assert(offsetof(Flush, header) == 0,
              "offset of Flush header should be 0");

struct FramebufferRenderbuffer {
  typedef FramebufferRenderbuffer ValueType;
  static const CommandId kCmdId = kFramebufferRenderbuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _attachment,
            GLenum _renderbuffertarget,
            GLuint _renderbuffer) {
    SetHeader();
    target = _target;
    attachment = _attachment;
    renderbuffertarget = _renderbuffertarget;
    renderbuffer = _renderbuffer;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _attachment,
            GLenum _renderbuffertarget,
            GLuint _renderbuffer) {
    static_cast<ValueType*>(cmd)->Init(_target, _attachment,
                                       _renderbuffertarget, _renderbuffer);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t attachment;
  uint32_t renderbuffertarget;
  uint32_t renderbuffer;
};

static_assert(sizeof(FramebufferRenderbuffer) == 20,
              "size of FramebufferRenderbuffer should be 20");
static_assert(offsetof(FramebufferRenderbuffer, header) == 0,
              "offset of FramebufferRenderbuffer header should be 0");
static_assert(offsetof(FramebufferRenderbuffer, target) == 4,
              "offset of FramebufferRenderbuffer target should be 4");
static_assert(offsetof(FramebufferRenderbuffer, attachment) == 8,
              "offset of FramebufferRenderbuffer attachment should be 8");
static_assert(
    offsetof(FramebufferRenderbuffer, renderbuffertarget) == 12,
    "offset of FramebufferRenderbuffer renderbuffertarget should be 12");
static_assert(offsetof(FramebufferRenderbuffer, renderbuffer) == 16,
              "offset of FramebufferRenderbuffer renderbuffer should be 16");

struct FramebufferTexture2D {
  typedef FramebufferTexture2D ValueType;
  static const CommandId kCmdId = kFramebufferTexture2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _attachment,
            GLenum _textarget,
            GLuint _texture,
            GLint _level) {
    SetHeader();
    target = _target;
    attachment = _attachment;
    textarget = _textarget;
    texture = _texture;
    level = _level;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _attachment,
            GLenum _textarget,
            GLuint _texture,
            GLint _level) {
    static_cast<ValueType*>(cmd)->Init(_target, _attachment, _textarget,
                                       _texture, _level);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t attachment;
  uint32_t textarget;
  uint32_t texture;
  int32_t level;
};

static_assert(sizeof(FramebufferTexture2D) == 24,
              "size of FramebufferTexture2D should be 24");
static_assert(offsetof(FramebufferTexture2D, header) == 0,
              "offset of FramebufferTexture2D header should be 0");
static_assert(offsetof(FramebufferTexture2D, target) == 4,
              "offset of FramebufferTexture2D target should be 4");
static_assert(offsetof(FramebufferTexture2D, attachment) == 8,
              "offset of FramebufferTexture2D attachment should be 8");
static_assert(offsetof(FramebufferTexture2D, textarget) == 12,
              "offset of FramebufferTexture2D textarget should be 12");
static_assert(offsetof(FramebufferTexture2D, texture) == 16,
              "offset of FramebufferTexture2D texture should be 16");
static_assert(offsetof(FramebufferTexture2D, level) == 20,
              "offset of FramebufferTexture2D level should be 20");

struct FramebufferTextureLayer {
  typedef FramebufferTextureLayer ValueType;
  static const CommandId kCmdId = kFramebufferTextureLayer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _attachment,
            GLuint _texture,
            GLint _level,
            GLint _layer) {
    SetHeader();
    target = _target;
    attachment = _attachment;
    texture = _texture;
    level = _level;
    layer = _layer;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _attachment,
            GLuint _texture,
            GLint _level,
            GLint _layer) {
    static_cast<ValueType*>(cmd)->Init(_target, _attachment, _texture, _level,
                                       _layer);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t attachment;
  uint32_t texture;
  int32_t level;
  int32_t layer;
};

static_assert(sizeof(FramebufferTextureLayer) == 24,
              "size of FramebufferTextureLayer should be 24");
static_assert(offsetof(FramebufferTextureLayer, header) == 0,
              "offset of FramebufferTextureLayer header should be 0");
static_assert(offsetof(FramebufferTextureLayer, target) == 4,
              "offset of FramebufferTextureLayer target should be 4");
static_assert(offsetof(FramebufferTextureLayer, attachment) == 8,
              "offset of FramebufferTextureLayer attachment should be 8");
static_assert(offsetof(FramebufferTextureLayer, texture) == 12,
              "offset of FramebufferTextureLayer texture should be 12");
static_assert(offsetof(FramebufferTextureLayer, level) == 16,
              "offset of FramebufferTextureLayer level should be 16");
static_assert(offsetof(FramebufferTextureLayer, layer) == 20,
              "offset of FramebufferTextureLayer layer should be 20");

struct FrontFace {
  typedef FrontFace ValueType;
  static const CommandId kCmdId = kFrontFace;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode) {
    SetHeader();
    mode = _mode;
  }

  void* Set(void* cmd, GLenum _mode) {
    static_cast<ValueType*>(cmd)->Init(_mode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
};

static_assert(sizeof(FrontFace) == 8, "size of FrontFace should be 8");
static_assert(offsetof(FrontFace, header) == 0,
              "offset of FrontFace header should be 0");
static_assert(offsetof(FrontFace, mode) == 4,
              "offset of FrontFace mode should be 4");

struct GenBuffersImmediate {
  typedef GenBuffersImmediate ValueType;
  static const CommandId kCmdId = kGenBuffersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _buffers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _buffers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _buffers) {
    static_cast<ValueType*>(cmd)->Init(_n, _buffers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenBuffersImmediate) == 8,
              "size of GenBuffersImmediate should be 8");
static_assert(offsetof(GenBuffersImmediate, header) == 0,
              "offset of GenBuffersImmediate header should be 0");
static_assert(offsetof(GenBuffersImmediate, n) == 4,
              "offset of GenBuffersImmediate n should be 4");

struct GenerateMipmap {
  typedef GenerateMipmap ValueType;
  static const CommandId kCmdId = kGenerateMipmap;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target) {
    SetHeader();
    target = _target;
  }

  void* Set(void* cmd, GLenum _target) {
    static_cast<ValueType*>(cmd)->Init(_target);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
};

static_assert(sizeof(GenerateMipmap) == 8,
              "size of GenerateMipmap should be 8");
static_assert(offsetof(GenerateMipmap, header) == 0,
              "offset of GenerateMipmap header should be 0");
static_assert(offsetof(GenerateMipmap, target) == 4,
              "offset of GenerateMipmap target should be 4");

struct GenFramebuffersImmediate {
  typedef GenFramebuffersImmediate ValueType;
  static const CommandId kCmdId = kGenFramebuffersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _framebuffers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _framebuffers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _framebuffers) {
    static_cast<ValueType*>(cmd)->Init(_n, _framebuffers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenFramebuffersImmediate) == 8,
              "size of GenFramebuffersImmediate should be 8");
static_assert(offsetof(GenFramebuffersImmediate, header) == 0,
              "offset of GenFramebuffersImmediate header should be 0");
static_assert(offsetof(GenFramebuffersImmediate, n) == 4,
              "offset of GenFramebuffersImmediate n should be 4");

struct GenRenderbuffersImmediate {
  typedef GenRenderbuffersImmediate ValueType;
  static const CommandId kCmdId = kGenRenderbuffersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _renderbuffers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _renderbuffers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _renderbuffers) {
    static_cast<ValueType*>(cmd)->Init(_n, _renderbuffers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenRenderbuffersImmediate) == 8,
              "size of GenRenderbuffersImmediate should be 8");
static_assert(offsetof(GenRenderbuffersImmediate, header) == 0,
              "offset of GenRenderbuffersImmediate header should be 0");
static_assert(offsetof(GenRenderbuffersImmediate, n) == 4,
              "offset of GenRenderbuffersImmediate n should be 4");

struct GenSamplersImmediate {
  typedef GenSamplersImmediate ValueType;
  static const CommandId kCmdId = kGenSamplersImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _samplers) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _samplers, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _samplers) {
    static_cast<ValueType*>(cmd)->Init(_n, _samplers);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenSamplersImmediate) == 8,
              "size of GenSamplersImmediate should be 8");
static_assert(offsetof(GenSamplersImmediate, header) == 0,
              "offset of GenSamplersImmediate header should be 0");
static_assert(offsetof(GenSamplersImmediate, n) == 4,
              "offset of GenSamplersImmediate n should be 4");

struct GenTexturesImmediate {
  typedef GenTexturesImmediate ValueType;
  static const CommandId kCmdId = kGenTexturesImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _textures) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _textures, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _textures) {
    static_cast<ValueType*>(cmd)->Init(_n, _textures);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenTexturesImmediate) == 8,
              "size of GenTexturesImmediate should be 8");
static_assert(offsetof(GenTexturesImmediate, header) == 0,
              "offset of GenTexturesImmediate header should be 0");
static_assert(offsetof(GenTexturesImmediate, n) == 4,
              "offset of GenTexturesImmediate n should be 4");

struct GenTransformFeedbacksImmediate {
  typedef GenTransformFeedbacksImmediate ValueType;
  static const CommandId kCmdId = kGenTransformFeedbacksImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _ids) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _ids, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _ids) {
    static_cast<ValueType*>(cmd)->Init(_n, _ids);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenTransformFeedbacksImmediate) == 8,
              "size of GenTransformFeedbacksImmediate should be 8");
static_assert(offsetof(GenTransformFeedbacksImmediate, header) == 0,
              "offset of GenTransformFeedbacksImmediate header should be 0");
static_assert(offsetof(GenTransformFeedbacksImmediate, n) == 4,
              "offset of GenTransformFeedbacksImmediate n should be 4");

struct GetActiveAttrib {
  typedef GetActiveAttrib ValueType;
  static const CommandId kCmdId = kGetActiveAttrib;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  struct Result {
    int32_t success;
    int32_t size;
    uint32_t type;
  };

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    program = _program;
    index = _index;
    name_bucket_id = _name_bucket_id;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _index, _name_bucket_id,
                                       _result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t index;
  uint32_t name_bucket_id;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetActiveAttrib) == 24,
              "size of GetActiveAttrib should be 24");
static_assert(offsetof(GetActiveAttrib, header) == 0,
              "offset of GetActiveAttrib header should be 0");
static_assert(offsetof(GetActiveAttrib, program) == 4,
              "offset of GetActiveAttrib program should be 4");
static_assert(offsetof(GetActiveAttrib, index) == 8,
              "offset of GetActiveAttrib index should be 8");
static_assert(offsetof(GetActiveAttrib, name_bucket_id) == 12,
              "offset of GetActiveAttrib name_bucket_id should be 12");
static_assert(offsetof(GetActiveAttrib, result_shm_id) == 16,
              "offset of GetActiveAttrib result_shm_id should be 16");
static_assert(offsetof(GetActiveAttrib, result_shm_offset) == 20,
              "offset of GetActiveAttrib result_shm_offset should be 20");
static_assert(offsetof(GetActiveAttrib::Result, success) == 0,
              "offset of GetActiveAttrib Result success should be "
              "0");
static_assert(offsetof(GetActiveAttrib::Result, size) == 4,
              "offset of GetActiveAttrib Result size should be "
              "4");
static_assert(offsetof(GetActiveAttrib::Result, type) == 8,
              "offset of GetActiveAttrib Result type should be "
              "8");

struct GetActiveUniform {
  typedef GetActiveUniform ValueType;
  static const CommandId kCmdId = kGetActiveUniform;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  struct Result {
    int32_t success;
    int32_t size;
    uint32_t type;
  };

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    program = _program;
    index = _index;
    name_bucket_id = _name_bucket_id;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _index, _name_bucket_id,
                                       _result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t index;
  uint32_t name_bucket_id;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetActiveUniform) == 24,
              "size of GetActiveUniform should be 24");
static_assert(offsetof(GetActiveUniform, header) == 0,
              "offset of GetActiveUniform header should be 0");
static_assert(offsetof(GetActiveUniform, program) == 4,
              "offset of GetActiveUniform program should be 4");
static_assert(offsetof(GetActiveUniform, index) == 8,
              "offset of GetActiveUniform index should be 8");
static_assert(offsetof(GetActiveUniform, name_bucket_id) == 12,
              "offset of GetActiveUniform name_bucket_id should be 12");
static_assert(offsetof(GetActiveUniform, result_shm_id) == 16,
              "offset of GetActiveUniform result_shm_id should be 16");
static_assert(offsetof(GetActiveUniform, result_shm_offset) == 20,
              "offset of GetActiveUniform result_shm_offset should be 20");
static_assert(offsetof(GetActiveUniform::Result, success) == 0,
              "offset of GetActiveUniform Result success should be "
              "0");
static_assert(offsetof(GetActiveUniform::Result, size) == 4,
              "offset of GetActiveUniform Result size should be "
              "4");
static_assert(offsetof(GetActiveUniform::Result, type) == 8,
              "offset of GetActiveUniform Result type should be "
              "8");

struct GetActiveUniformBlockiv {
  typedef GetActiveUniformBlockiv ValueType;
  static const CommandId kCmdId = kGetActiveUniformBlockiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    index = _index;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _index, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t index;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetActiveUniformBlockiv) == 24,
              "size of GetActiveUniformBlockiv should be 24");
static_assert(offsetof(GetActiveUniformBlockiv, header) == 0,
              "offset of GetActiveUniformBlockiv header should be 0");
static_assert(offsetof(GetActiveUniformBlockiv, program) == 4,
              "offset of GetActiveUniformBlockiv program should be 4");
static_assert(offsetof(GetActiveUniformBlockiv, index) == 8,
              "offset of GetActiveUniformBlockiv index should be 8");
static_assert(offsetof(GetActiveUniformBlockiv, pname) == 12,
              "offset of GetActiveUniformBlockiv pname should be 12");
static_assert(offsetof(GetActiveUniformBlockiv, params_shm_id) == 16,
              "offset of GetActiveUniformBlockiv params_shm_id should be 16");
static_assert(
    offsetof(GetActiveUniformBlockiv, params_shm_offset) == 20,
    "offset of GetActiveUniformBlockiv params_shm_offset should be 20");

struct GetActiveUniformBlockName {
  typedef GetActiveUniformBlockName ValueType;
  static const CommandId kCmdId = kGetActiveUniformBlockName;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef int32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    program = _program;
    index = _index;
    name_bucket_id = _name_bucket_id;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _index, _name_bucket_id,
                                       _result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t index;
  uint32_t name_bucket_id;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetActiveUniformBlockName) == 24,
              "size of GetActiveUniformBlockName should be 24");
static_assert(offsetof(GetActiveUniformBlockName, header) == 0,
              "offset of GetActiveUniformBlockName header should be 0");
static_assert(offsetof(GetActiveUniformBlockName, program) == 4,
              "offset of GetActiveUniformBlockName program should be 4");
static_assert(offsetof(GetActiveUniformBlockName, index) == 8,
              "offset of GetActiveUniformBlockName index should be 8");
static_assert(
    offsetof(GetActiveUniformBlockName, name_bucket_id) == 12,
    "offset of GetActiveUniformBlockName name_bucket_id should be 12");
static_assert(offsetof(GetActiveUniformBlockName, result_shm_id) == 16,
              "offset of GetActiveUniformBlockName result_shm_id should be 16");
static_assert(
    offsetof(GetActiveUniformBlockName, result_shm_offset) == 20,
    "offset of GetActiveUniformBlockName result_shm_offset should be 20");

struct GetActiveUniformsiv {
  typedef GetActiveUniformsiv ValueType;
  static const CommandId kCmdId = kGetActiveUniformsiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _indices_bucket_id,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    indices_bucket_id = _indices_bucket_id;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _indices_bucket_id,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _indices_bucket_id, _pname,
                                       _params_shm_id, _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t indices_bucket_id;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetActiveUniformsiv) == 24,
              "size of GetActiveUniformsiv should be 24");
static_assert(offsetof(GetActiveUniformsiv, header) == 0,
              "offset of GetActiveUniformsiv header should be 0");
static_assert(offsetof(GetActiveUniformsiv, program) == 4,
              "offset of GetActiveUniformsiv program should be 4");
static_assert(offsetof(GetActiveUniformsiv, indices_bucket_id) == 8,
              "offset of GetActiveUniformsiv indices_bucket_id should be 8");
static_assert(offsetof(GetActiveUniformsiv, pname) == 12,
              "offset of GetActiveUniformsiv pname should be 12");
static_assert(offsetof(GetActiveUniformsiv, params_shm_id) == 16,
              "offset of GetActiveUniformsiv params_shm_id should be 16");
static_assert(offsetof(GetActiveUniformsiv, params_shm_offset) == 20,
              "offset of GetActiveUniformsiv params_shm_offset should be 20");

struct GetAttachedShaders {
  typedef GetAttachedShaders ValueType;
  static const CommandId kCmdId = kGetAttachedShaders;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLuint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset,
            uint32_t _result_size) {
    SetHeader();
    program = _program;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
    result_size = _result_size;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset,
            uint32_t _result_size) {
    static_cast<ValueType*>(cmd)->Init(_program, _result_shm_id,
                                       _result_shm_offset, _result_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
  uint32_t result_size;
};

static_assert(sizeof(GetAttachedShaders) == 20,
              "size of GetAttachedShaders should be 20");
static_assert(offsetof(GetAttachedShaders, header) == 0,
              "offset of GetAttachedShaders header should be 0");
static_assert(offsetof(GetAttachedShaders, program) == 4,
              "offset of GetAttachedShaders program should be 4");
static_assert(offsetof(GetAttachedShaders, result_shm_id) == 8,
              "offset of GetAttachedShaders result_shm_id should be 8");
static_assert(offsetof(GetAttachedShaders, result_shm_offset) == 12,
              "offset of GetAttachedShaders result_shm_offset should be 12");
static_assert(offsetof(GetAttachedShaders, result_size) == 16,
              "offset of GetAttachedShaders result_size should be 16");

struct GetAttribLocation {
  typedef GetAttribLocation ValueType;
  static const CommandId kCmdId = kGetAttribLocation;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    SetHeader();
    program = _program;
    name_bucket_id = _name_bucket_id;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _name_bucket_id,
                                       _location_shm_id, _location_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t name_bucket_id;
  uint32_t location_shm_id;
  uint32_t location_shm_offset;
};

static_assert(sizeof(GetAttribLocation) == 20,
              "size of GetAttribLocation should be 20");
static_assert(offsetof(GetAttribLocation, header) == 0,
              "offset of GetAttribLocation header should be 0");
static_assert(offsetof(GetAttribLocation, program) == 4,
              "offset of GetAttribLocation program should be 4");
static_assert(offsetof(GetAttribLocation, name_bucket_id) == 8,
              "offset of GetAttribLocation name_bucket_id should be 8");
static_assert(offsetof(GetAttribLocation, location_shm_id) == 12,
              "offset of GetAttribLocation location_shm_id should be 12");
static_assert(offsetof(GetAttribLocation, location_shm_offset) == 16,
              "offset of GetAttribLocation location_shm_offset should be 16");

struct GetBooleanv {
  typedef GetBooleanv ValueType;
  static const CommandId kCmdId = kGetBooleanv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLboolean> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetBooleanv) == 16, "size of GetBooleanv should be 16");
static_assert(offsetof(GetBooleanv, header) == 0,
              "offset of GetBooleanv header should be 0");
static_assert(offsetof(GetBooleanv, pname) == 4,
              "offset of GetBooleanv pname should be 4");
static_assert(offsetof(GetBooleanv, params_shm_id) == 8,
              "offset of GetBooleanv params_shm_id should be 8");
static_assert(offsetof(GetBooleanv, params_shm_offset) == 12,
              "offset of GetBooleanv params_shm_offset should be 12");

struct GetBufferParameteri64v {
  typedef GetBufferParameteri64v ValueType;
  static const CommandId kCmdId = kGetBufferParameteri64v;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint64> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    target = _target;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetBufferParameteri64v) == 20,
              "size of GetBufferParameteri64v should be 20");
static_assert(offsetof(GetBufferParameteri64v, header) == 0,
              "offset of GetBufferParameteri64v header should be 0");
static_assert(offsetof(GetBufferParameteri64v, target) == 4,
              "offset of GetBufferParameteri64v target should be 4");
static_assert(offsetof(GetBufferParameteri64v, pname) == 8,
              "offset of GetBufferParameteri64v pname should be 8");
static_assert(offsetof(GetBufferParameteri64v, params_shm_id) == 12,
              "offset of GetBufferParameteri64v params_shm_id should be 12");
static_assert(
    offsetof(GetBufferParameteri64v, params_shm_offset) == 16,
    "offset of GetBufferParameteri64v params_shm_offset should be 16");

struct GetBufferParameteriv {
  typedef GetBufferParameteriv ValueType;
  static const CommandId kCmdId = kGetBufferParameteriv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    target = _target;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetBufferParameteriv) == 20,
              "size of GetBufferParameteriv should be 20");
static_assert(offsetof(GetBufferParameteriv, header) == 0,
              "offset of GetBufferParameteriv header should be 0");
static_assert(offsetof(GetBufferParameteriv, target) == 4,
              "offset of GetBufferParameteriv target should be 4");
static_assert(offsetof(GetBufferParameteriv, pname) == 8,
              "offset of GetBufferParameteriv pname should be 8");
static_assert(offsetof(GetBufferParameteriv, params_shm_id) == 12,
              "offset of GetBufferParameteriv params_shm_id should be 12");
static_assert(offsetof(GetBufferParameteriv, params_shm_offset) == 16,
              "offset of GetBufferParameteriv params_shm_offset should be 16");

struct GetError {
  typedef GetError ValueType;
  static const CommandId kCmdId = kGetError;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLenum Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _result_shm_id, uint32_t _result_shm_offset) {
    SetHeader();
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd, uint32_t _result_shm_id, uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetError) == 12, "size of GetError should be 12");
static_assert(offsetof(GetError, header) == 0,
              "offset of GetError header should be 0");
static_assert(offsetof(GetError, result_shm_id) == 4,
              "offset of GetError result_shm_id should be 4");
static_assert(offsetof(GetError, result_shm_offset) == 8,
              "offset of GetError result_shm_offset should be 8");

struct GetFloatv {
  typedef GetFloatv ValueType;
  static const CommandId kCmdId = kGetFloatv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLfloat> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetFloatv) == 16, "size of GetFloatv should be 16");
static_assert(offsetof(GetFloatv, header) == 0,
              "offset of GetFloatv header should be 0");
static_assert(offsetof(GetFloatv, pname) == 4,
              "offset of GetFloatv pname should be 4");
static_assert(offsetof(GetFloatv, params_shm_id) == 8,
              "offset of GetFloatv params_shm_id should be 8");
static_assert(offsetof(GetFloatv, params_shm_offset) == 12,
              "offset of GetFloatv params_shm_offset should be 12");

struct GetFragDataLocation {
  typedef GetFragDataLocation ValueType;
  static const CommandId kCmdId = kGetFragDataLocation;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    SetHeader();
    program = _program;
    name_bucket_id = _name_bucket_id;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _name_bucket_id,
                                       _location_shm_id, _location_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t name_bucket_id;
  uint32_t location_shm_id;
  uint32_t location_shm_offset;
};

static_assert(sizeof(GetFragDataLocation) == 20,
              "size of GetFragDataLocation should be 20");
static_assert(offsetof(GetFragDataLocation, header) == 0,
              "offset of GetFragDataLocation header should be 0");
static_assert(offsetof(GetFragDataLocation, program) == 4,
              "offset of GetFragDataLocation program should be 4");
static_assert(offsetof(GetFragDataLocation, name_bucket_id) == 8,
              "offset of GetFragDataLocation name_bucket_id should be 8");
static_assert(offsetof(GetFragDataLocation, location_shm_id) == 12,
              "offset of GetFragDataLocation location_shm_id should be 12");
static_assert(offsetof(GetFragDataLocation, location_shm_offset) == 16,
              "offset of GetFragDataLocation location_shm_offset should be 16");

struct GetFramebufferAttachmentParameteriv {
  typedef GetFramebufferAttachmentParameteriv ValueType;
  static const CommandId kCmdId = kGetFramebufferAttachmentParameteriv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _attachment,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    target = _target;
    attachment = _attachment;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _attachment,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _attachment, _pname,
                                       _params_shm_id, _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t attachment;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetFramebufferAttachmentParameteriv) == 24,
              "size of GetFramebufferAttachmentParameteriv should be 24");
static_assert(
    offsetof(GetFramebufferAttachmentParameteriv, header) == 0,
    "offset of GetFramebufferAttachmentParameteriv header should be 0");
static_assert(
    offsetof(GetFramebufferAttachmentParameteriv, target) == 4,
    "offset of GetFramebufferAttachmentParameteriv target should be 4");
static_assert(
    offsetof(GetFramebufferAttachmentParameteriv, attachment) == 8,
    "offset of GetFramebufferAttachmentParameteriv attachment should be 8");
static_assert(
    offsetof(GetFramebufferAttachmentParameteriv, pname) == 12,
    "offset of GetFramebufferAttachmentParameteriv pname should be 12");
static_assert(
    offsetof(GetFramebufferAttachmentParameteriv, params_shm_id) == 16,
    "offset of GetFramebufferAttachmentParameteriv params_shm_id should be 16");
static_assert(offsetof(GetFramebufferAttachmentParameteriv,
                       params_shm_offset) == 20,
              "offset of GetFramebufferAttachmentParameteriv params_shm_offset "
              "should be 20");

struct GetInteger64v {
  typedef GetInteger64v ValueType;
  static const CommandId kCmdId = kGetInteger64v;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint64> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetInteger64v) == 16,
              "size of GetInteger64v should be 16");
static_assert(offsetof(GetInteger64v, header) == 0,
              "offset of GetInteger64v header should be 0");
static_assert(offsetof(GetInteger64v, pname) == 4,
              "offset of GetInteger64v pname should be 4");
static_assert(offsetof(GetInteger64v, params_shm_id) == 8,
              "offset of GetInteger64v params_shm_id should be 8");
static_assert(offsetof(GetInteger64v, params_shm_offset) == 12,
              "offset of GetInteger64v params_shm_offset should be 12");

struct GetIntegeri_v {
  typedef GetIntegeri_v ValueType;
  static const CommandId kCmdId = kGetIntegeri_v;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname,
            GLuint _index,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    pname = _pname;
    index = _index;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _pname,
            GLuint _index,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_pname, _index, _data_shm_id,
                                       _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  uint32_t index;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
};

static_assert(sizeof(GetIntegeri_v) == 20,
              "size of GetIntegeri_v should be 20");
static_assert(offsetof(GetIntegeri_v, header) == 0,
              "offset of GetIntegeri_v header should be 0");
static_assert(offsetof(GetIntegeri_v, pname) == 4,
              "offset of GetIntegeri_v pname should be 4");
static_assert(offsetof(GetIntegeri_v, index) == 8,
              "offset of GetIntegeri_v index should be 8");
static_assert(offsetof(GetIntegeri_v, data_shm_id) == 12,
              "offset of GetIntegeri_v data_shm_id should be 12");
static_assert(offsetof(GetIntegeri_v, data_shm_offset) == 16,
              "offset of GetIntegeri_v data_shm_offset should be 16");

struct GetInteger64i_v {
  typedef GetInteger64i_v ValueType;
  static const CommandId kCmdId = kGetInteger64i_v;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint64> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname,
            GLuint _index,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    pname = _pname;
    index = _index;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _pname,
            GLuint _index,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_pname, _index, _data_shm_id,
                                       _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  uint32_t index;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
};

static_assert(sizeof(GetInteger64i_v) == 20,
              "size of GetInteger64i_v should be 20");
static_assert(offsetof(GetInteger64i_v, header) == 0,
              "offset of GetInteger64i_v header should be 0");
static_assert(offsetof(GetInteger64i_v, pname) == 4,
              "offset of GetInteger64i_v pname should be 4");
static_assert(offsetof(GetInteger64i_v, index) == 8,
              "offset of GetInteger64i_v index should be 8");
static_assert(offsetof(GetInteger64i_v, data_shm_id) == 12,
              "offset of GetInteger64i_v data_shm_id should be 12");
static_assert(offsetof(GetInteger64i_v, data_shm_offset) == 16,
              "offset of GetInteger64i_v data_shm_offset should be 16");

struct GetIntegerv {
  typedef GetIntegerv ValueType;
  static const CommandId kCmdId = kGetIntegerv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetIntegerv) == 16, "size of GetIntegerv should be 16");
static_assert(offsetof(GetIntegerv, header) == 0,
              "offset of GetIntegerv header should be 0");
static_assert(offsetof(GetIntegerv, pname) == 4,
              "offset of GetIntegerv pname should be 4");
static_assert(offsetof(GetIntegerv, params_shm_id) == 8,
              "offset of GetIntegerv params_shm_id should be 8");
static_assert(offsetof(GetIntegerv, params_shm_offset) == 12,
              "offset of GetIntegerv params_shm_offset should be 12");

struct GetInternalformativ {
  typedef GetInternalformativ ValueType;
  static const CommandId kCmdId = kGetInternalformativ;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _format,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    target = _target;
    format = _format;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _format,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _format, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t format;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetInternalformativ) == 24,
              "size of GetInternalformativ should be 24");
static_assert(offsetof(GetInternalformativ, header) == 0,
              "offset of GetInternalformativ header should be 0");
static_assert(offsetof(GetInternalformativ, target) == 4,
              "offset of GetInternalformativ target should be 4");
static_assert(offsetof(GetInternalformativ, format) == 8,
              "offset of GetInternalformativ format should be 8");
static_assert(offsetof(GetInternalformativ, pname) == 12,
              "offset of GetInternalformativ pname should be 12");
static_assert(offsetof(GetInternalformativ, params_shm_id) == 16,
              "offset of GetInternalformativ params_shm_id should be 16");
static_assert(offsetof(GetInternalformativ, params_shm_offset) == 20,
              "offset of GetInternalformativ params_shm_offset should be 20");

struct GetProgramiv {
  typedef GetProgramiv ValueType;
  static const CommandId kCmdId = kGetProgramiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetProgramiv) == 20, "size of GetProgramiv should be 20");
static_assert(offsetof(GetProgramiv, header) == 0,
              "offset of GetProgramiv header should be 0");
static_assert(offsetof(GetProgramiv, program) == 4,
              "offset of GetProgramiv program should be 4");
static_assert(offsetof(GetProgramiv, pname) == 8,
              "offset of GetProgramiv pname should be 8");
static_assert(offsetof(GetProgramiv, params_shm_id) == 12,
              "offset of GetProgramiv params_shm_id should be 12");
static_assert(offsetof(GetProgramiv, params_shm_offset) == 16,
              "offset of GetProgramiv params_shm_offset should be 16");

struct GetProgramInfoLog {
  typedef GetProgramInfoLog ValueType;
  static const CommandId kCmdId = kGetProgramInfoLog;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, uint32_t _bucket_id) {
    SetHeader();
    program = _program;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _program, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t bucket_id;
};

static_assert(sizeof(GetProgramInfoLog) == 12,
              "size of GetProgramInfoLog should be 12");
static_assert(offsetof(GetProgramInfoLog, header) == 0,
              "offset of GetProgramInfoLog header should be 0");
static_assert(offsetof(GetProgramInfoLog, program) == 4,
              "offset of GetProgramInfoLog program should be 4");
static_assert(offsetof(GetProgramInfoLog, bucket_id) == 8,
              "offset of GetProgramInfoLog bucket_id should be 8");

struct GetRenderbufferParameteriv {
  typedef GetRenderbufferParameteriv ValueType;
  static const CommandId kCmdId = kGetRenderbufferParameteriv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    target = _target;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetRenderbufferParameteriv) == 20,
              "size of GetRenderbufferParameteriv should be 20");
static_assert(offsetof(GetRenderbufferParameteriv, header) == 0,
              "offset of GetRenderbufferParameteriv header should be 0");
static_assert(offsetof(GetRenderbufferParameteriv, target) == 4,
              "offset of GetRenderbufferParameteriv target should be 4");
static_assert(offsetof(GetRenderbufferParameteriv, pname) == 8,
              "offset of GetRenderbufferParameteriv pname should be 8");
static_assert(
    offsetof(GetRenderbufferParameteriv, params_shm_id) == 12,
    "offset of GetRenderbufferParameteriv params_shm_id should be 12");
static_assert(
    offsetof(GetRenderbufferParameteriv, params_shm_offset) == 16,
    "offset of GetRenderbufferParameteriv params_shm_offset should be 16");

struct GetSamplerParameterfv {
  typedef GetSamplerParameterfv ValueType;
  static const CommandId kCmdId = kGetSamplerParameterfv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLfloat> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sampler,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    sampler = _sampler;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _sampler,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_sampler, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sampler;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetSamplerParameterfv) == 20,
              "size of GetSamplerParameterfv should be 20");
static_assert(offsetof(GetSamplerParameterfv, header) == 0,
              "offset of GetSamplerParameterfv header should be 0");
static_assert(offsetof(GetSamplerParameterfv, sampler) == 4,
              "offset of GetSamplerParameterfv sampler should be 4");
static_assert(offsetof(GetSamplerParameterfv, pname) == 8,
              "offset of GetSamplerParameterfv pname should be 8");
static_assert(offsetof(GetSamplerParameterfv, params_shm_id) == 12,
              "offset of GetSamplerParameterfv params_shm_id should be 12");
static_assert(offsetof(GetSamplerParameterfv, params_shm_offset) == 16,
              "offset of GetSamplerParameterfv params_shm_offset should be 16");

struct GetSamplerParameteriv {
  typedef GetSamplerParameteriv ValueType;
  static const CommandId kCmdId = kGetSamplerParameteriv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sampler,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    sampler = _sampler;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _sampler,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_sampler, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sampler;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetSamplerParameteriv) == 20,
              "size of GetSamplerParameteriv should be 20");
static_assert(offsetof(GetSamplerParameteriv, header) == 0,
              "offset of GetSamplerParameteriv header should be 0");
static_assert(offsetof(GetSamplerParameteriv, sampler) == 4,
              "offset of GetSamplerParameteriv sampler should be 4");
static_assert(offsetof(GetSamplerParameteriv, pname) == 8,
              "offset of GetSamplerParameteriv pname should be 8");
static_assert(offsetof(GetSamplerParameteriv, params_shm_id) == 12,
              "offset of GetSamplerParameteriv params_shm_id should be 12");
static_assert(offsetof(GetSamplerParameteriv, params_shm_offset) == 16,
              "offset of GetSamplerParameteriv params_shm_offset should be 16");

struct GetShaderiv {
  typedef GetShaderiv ValueType;
  static const CommandId kCmdId = kGetShaderiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    shader = _shader;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _shader,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_shader, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetShaderiv) == 20, "size of GetShaderiv should be 20");
static_assert(offsetof(GetShaderiv, header) == 0,
              "offset of GetShaderiv header should be 0");
static_assert(offsetof(GetShaderiv, shader) == 4,
              "offset of GetShaderiv shader should be 4");
static_assert(offsetof(GetShaderiv, pname) == 8,
              "offset of GetShaderiv pname should be 8");
static_assert(offsetof(GetShaderiv, params_shm_id) == 12,
              "offset of GetShaderiv params_shm_id should be 12");
static_assert(offsetof(GetShaderiv, params_shm_offset) == 16,
              "offset of GetShaderiv params_shm_offset should be 16");

struct GetShaderInfoLog {
  typedef GetShaderInfoLog ValueType;
  static const CommandId kCmdId = kGetShaderInfoLog;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader, uint32_t _bucket_id) {
    SetHeader();
    shader = _shader;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _shader, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_shader, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
  uint32_t bucket_id;
};

static_assert(sizeof(GetShaderInfoLog) == 12,
              "size of GetShaderInfoLog should be 12");
static_assert(offsetof(GetShaderInfoLog, header) == 0,
              "offset of GetShaderInfoLog header should be 0");
static_assert(offsetof(GetShaderInfoLog, shader) == 4,
              "offset of GetShaderInfoLog shader should be 4");
static_assert(offsetof(GetShaderInfoLog, bucket_id) == 8,
              "offset of GetShaderInfoLog bucket_id should be 8");

struct GetShaderPrecisionFormat {
  typedef GetShaderPrecisionFormat ValueType;
  static const CommandId kCmdId = kGetShaderPrecisionFormat;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  struct Result {
    int32_t success;
    int32_t min_range;
    int32_t max_range;
    int32_t precision;
  };

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _shadertype,
            GLenum _precisiontype,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    shadertype = _shadertype;
    precisiontype = _precisiontype;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _shadertype,
            GLenum _precisiontype,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_shadertype, _precisiontype,
                                       _result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shadertype;
  uint32_t precisiontype;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetShaderPrecisionFormat) == 20,
              "size of GetShaderPrecisionFormat should be 20");
static_assert(offsetof(GetShaderPrecisionFormat, header) == 0,
              "offset of GetShaderPrecisionFormat header should be 0");
static_assert(offsetof(GetShaderPrecisionFormat, shadertype) == 4,
              "offset of GetShaderPrecisionFormat shadertype should be 4");
static_assert(offsetof(GetShaderPrecisionFormat, precisiontype) == 8,
              "offset of GetShaderPrecisionFormat precisiontype should be 8");
static_assert(offsetof(GetShaderPrecisionFormat, result_shm_id) == 12,
              "offset of GetShaderPrecisionFormat result_shm_id should be 12");
static_assert(
    offsetof(GetShaderPrecisionFormat, result_shm_offset) == 16,
    "offset of GetShaderPrecisionFormat result_shm_offset should be 16");
static_assert(offsetof(GetShaderPrecisionFormat::Result, success) == 0,
              "offset of GetShaderPrecisionFormat Result success should be "
              "0");
static_assert(offsetof(GetShaderPrecisionFormat::Result, min_range) == 4,
              "offset of GetShaderPrecisionFormat Result min_range should be "
              "4");
static_assert(offsetof(GetShaderPrecisionFormat::Result, max_range) == 8,
              "offset of GetShaderPrecisionFormat Result max_range should be "
              "8");
static_assert(offsetof(GetShaderPrecisionFormat::Result, precision) == 12,
              "offset of GetShaderPrecisionFormat Result precision should be "
              "12");

struct GetShaderSource {
  typedef GetShaderSource ValueType;
  static const CommandId kCmdId = kGetShaderSource;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader, uint32_t _bucket_id) {
    SetHeader();
    shader = _shader;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _shader, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_shader, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
  uint32_t bucket_id;
};

static_assert(sizeof(GetShaderSource) == 12,
              "size of GetShaderSource should be 12");
static_assert(offsetof(GetShaderSource, header) == 0,
              "offset of GetShaderSource header should be 0");
static_assert(offsetof(GetShaderSource, shader) == 4,
              "offset of GetShaderSource shader should be 4");
static_assert(offsetof(GetShaderSource, bucket_id) == 8,
              "offset of GetShaderSource bucket_id should be 8");

struct GetString {
  typedef GetString ValueType;
  static const CommandId kCmdId = kGetString;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _name, uint32_t _bucket_id) {
    SetHeader();
    name = _name;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLenum _name, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_name, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t name;
  uint32_t bucket_id;
};

static_assert(sizeof(GetString) == 12, "size of GetString should be 12");
static_assert(offsetof(GetString, header) == 0,
              "offset of GetString header should be 0");
static_assert(offsetof(GetString, name) == 4,
              "offset of GetString name should be 4");
static_assert(offsetof(GetString, bucket_id) == 8,
              "offset of GetString bucket_id should be 8");

struct GetSynciv {
  typedef GetSynciv ValueType;
  static const CommandId kCmdId = kGetSynciv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sync,
            GLenum _pname,
            uint32_t _values_shm_id,
            uint32_t _values_shm_offset) {
    SetHeader();
    sync = _sync;
    pname = _pname;
    values_shm_id = _values_shm_id;
    values_shm_offset = _values_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _sync,
            GLenum _pname,
            uint32_t _values_shm_id,
            uint32_t _values_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_sync, _pname, _values_shm_id,
                                       _values_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sync;
  uint32_t pname;
  uint32_t values_shm_id;
  uint32_t values_shm_offset;
};

static_assert(sizeof(GetSynciv) == 20, "size of GetSynciv should be 20");
static_assert(offsetof(GetSynciv, header) == 0,
              "offset of GetSynciv header should be 0");
static_assert(offsetof(GetSynciv, sync) == 4,
              "offset of GetSynciv sync should be 4");
static_assert(offsetof(GetSynciv, pname) == 8,
              "offset of GetSynciv pname should be 8");
static_assert(offsetof(GetSynciv, values_shm_id) == 12,
              "offset of GetSynciv values_shm_id should be 12");
static_assert(offsetof(GetSynciv, values_shm_offset) == 16,
              "offset of GetSynciv values_shm_offset should be 16");

struct GetTexParameterfv {
  typedef GetTexParameterfv ValueType;
  static const CommandId kCmdId = kGetTexParameterfv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLfloat> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    target = _target;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetTexParameterfv) == 20,
              "size of GetTexParameterfv should be 20");
static_assert(offsetof(GetTexParameterfv, header) == 0,
              "offset of GetTexParameterfv header should be 0");
static_assert(offsetof(GetTexParameterfv, target) == 4,
              "offset of GetTexParameterfv target should be 4");
static_assert(offsetof(GetTexParameterfv, pname) == 8,
              "offset of GetTexParameterfv pname should be 8");
static_assert(offsetof(GetTexParameterfv, params_shm_id) == 12,
              "offset of GetTexParameterfv params_shm_id should be 12");
static_assert(offsetof(GetTexParameterfv, params_shm_offset) == 16,
              "offset of GetTexParameterfv params_shm_offset should be 16");

struct GetTexParameteriv {
  typedef GetTexParameteriv ValueType;
  static const CommandId kCmdId = kGetTexParameteriv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    target = _target;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetTexParameteriv) == 20,
              "size of GetTexParameteriv should be 20");
static_assert(offsetof(GetTexParameteriv, header) == 0,
              "offset of GetTexParameteriv header should be 0");
static_assert(offsetof(GetTexParameteriv, target) == 4,
              "offset of GetTexParameteriv target should be 4");
static_assert(offsetof(GetTexParameteriv, pname) == 8,
              "offset of GetTexParameteriv pname should be 8");
static_assert(offsetof(GetTexParameteriv, params_shm_id) == 12,
              "offset of GetTexParameteriv params_shm_id should be 12");
static_assert(offsetof(GetTexParameteriv, params_shm_offset) == 16,
              "offset of GetTexParameteriv params_shm_offset should be 16");

struct GetTransformFeedbackVarying {
  typedef GetTransformFeedbackVarying ValueType;
  static const CommandId kCmdId = kGetTransformFeedbackVarying;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  struct Result {
    int32_t success;
    int32_t size;
    uint32_t type;
  };

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    program = _program;
    index = _index;
    name_bucket_id = _name_bucket_id;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _index, _name_bucket_id,
                                       _result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t index;
  uint32_t name_bucket_id;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetTransformFeedbackVarying) == 24,
              "size of GetTransformFeedbackVarying should be 24");
static_assert(offsetof(GetTransformFeedbackVarying, header) == 0,
              "offset of GetTransformFeedbackVarying header should be 0");
static_assert(offsetof(GetTransformFeedbackVarying, program) == 4,
              "offset of GetTransformFeedbackVarying program should be 4");
static_assert(offsetof(GetTransformFeedbackVarying, index) == 8,
              "offset of GetTransformFeedbackVarying index should be 8");
static_assert(
    offsetof(GetTransformFeedbackVarying, name_bucket_id) == 12,
    "offset of GetTransformFeedbackVarying name_bucket_id should be 12");
static_assert(
    offsetof(GetTransformFeedbackVarying, result_shm_id) == 16,
    "offset of GetTransformFeedbackVarying result_shm_id should be 16");
static_assert(
    offsetof(GetTransformFeedbackVarying, result_shm_offset) == 20,
    "offset of GetTransformFeedbackVarying result_shm_offset should be 20");
static_assert(offsetof(GetTransformFeedbackVarying::Result, success) == 0,
              "offset of GetTransformFeedbackVarying Result success should be "
              "0");
static_assert(offsetof(GetTransformFeedbackVarying::Result, size) == 4,
              "offset of GetTransformFeedbackVarying Result size should be "
              "4");
static_assert(offsetof(GetTransformFeedbackVarying::Result, type) == 8,
              "offset of GetTransformFeedbackVarying Result type should be "
              "8");

struct GetUniformBlockIndex {
  typedef GetUniformBlockIndex ValueType;
  static const CommandId kCmdId = kGetUniformBlockIndex;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLuint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _index_shm_id,
            uint32_t _index_shm_offset) {
    SetHeader();
    program = _program;
    name_bucket_id = _name_bucket_id;
    index_shm_id = _index_shm_id;
    index_shm_offset = _index_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _index_shm_id,
            uint32_t _index_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _name_bucket_id, _index_shm_id,
                                       _index_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t name_bucket_id;
  uint32_t index_shm_id;
  uint32_t index_shm_offset;
};

static_assert(sizeof(GetUniformBlockIndex) == 20,
              "size of GetUniformBlockIndex should be 20");
static_assert(offsetof(GetUniformBlockIndex, header) == 0,
              "offset of GetUniformBlockIndex header should be 0");
static_assert(offsetof(GetUniformBlockIndex, program) == 4,
              "offset of GetUniformBlockIndex program should be 4");
static_assert(offsetof(GetUniformBlockIndex, name_bucket_id) == 8,
              "offset of GetUniformBlockIndex name_bucket_id should be 8");
static_assert(offsetof(GetUniformBlockIndex, index_shm_id) == 12,
              "offset of GetUniformBlockIndex index_shm_id should be 12");
static_assert(offsetof(GetUniformBlockIndex, index_shm_offset) == 16,
              "offset of GetUniformBlockIndex index_shm_offset should be 16");

struct GetUniformfv {
  typedef GetUniformfv ValueType;
  static const CommandId kCmdId = kGetUniformfv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLfloat> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLint _location,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    location = _location;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLint _location,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _location, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  int32_t location;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetUniformfv) == 20, "size of GetUniformfv should be 20");
static_assert(offsetof(GetUniformfv, header) == 0,
              "offset of GetUniformfv header should be 0");
static_assert(offsetof(GetUniformfv, program) == 4,
              "offset of GetUniformfv program should be 4");
static_assert(offsetof(GetUniformfv, location) == 8,
              "offset of GetUniformfv location should be 8");
static_assert(offsetof(GetUniformfv, params_shm_id) == 12,
              "offset of GetUniformfv params_shm_id should be 12");
static_assert(offsetof(GetUniformfv, params_shm_offset) == 16,
              "offset of GetUniformfv params_shm_offset should be 16");

struct GetUniformiv {
  typedef GetUniformiv ValueType;
  static const CommandId kCmdId = kGetUniformiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLint _location,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    location = _location;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLint _location,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _location, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  int32_t location;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetUniformiv) == 20, "size of GetUniformiv should be 20");
static_assert(offsetof(GetUniformiv, header) == 0,
              "offset of GetUniformiv header should be 0");
static_assert(offsetof(GetUniformiv, program) == 4,
              "offset of GetUniformiv program should be 4");
static_assert(offsetof(GetUniformiv, location) == 8,
              "offset of GetUniformiv location should be 8");
static_assert(offsetof(GetUniformiv, params_shm_id) == 12,
              "offset of GetUniformiv params_shm_id should be 12");
static_assert(offsetof(GetUniformiv, params_shm_offset) == 16,
              "offset of GetUniformiv params_shm_offset should be 16");

struct GetUniformuiv {
  typedef GetUniformuiv ValueType;
  static const CommandId kCmdId = kGetUniformuiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLuint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLint _location,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    location = _location;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLint _location,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _location, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  int32_t location;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetUniformuiv) == 20,
              "size of GetUniformuiv should be 20");
static_assert(offsetof(GetUniformuiv, header) == 0,
              "offset of GetUniformuiv header should be 0");
static_assert(offsetof(GetUniformuiv, program) == 4,
              "offset of GetUniformuiv program should be 4");
static_assert(offsetof(GetUniformuiv, location) == 8,
              "offset of GetUniformuiv location should be 8");
static_assert(offsetof(GetUniformuiv, params_shm_id) == 12,
              "offset of GetUniformuiv params_shm_id should be 12");
static_assert(offsetof(GetUniformuiv, params_shm_offset) == 16,
              "offset of GetUniformuiv params_shm_offset should be 16");

struct GetUniformIndices {
  typedef GetUniformIndices ValueType;
  static const CommandId kCmdId = kGetUniformIndices;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLuint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _names_bucket_id,
            uint32_t _indices_shm_id,
            uint32_t _indices_shm_offset) {
    SetHeader();
    program = _program;
    names_bucket_id = _names_bucket_id;
    indices_shm_id = _indices_shm_id;
    indices_shm_offset = _indices_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _names_bucket_id,
            uint32_t _indices_shm_id,
            uint32_t _indices_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _names_bucket_id,
                                       _indices_shm_id, _indices_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t names_bucket_id;
  uint32_t indices_shm_id;
  uint32_t indices_shm_offset;
};

static_assert(sizeof(GetUniformIndices) == 20,
              "size of GetUniformIndices should be 20");
static_assert(offsetof(GetUniformIndices, header) == 0,
              "offset of GetUniformIndices header should be 0");
static_assert(offsetof(GetUniformIndices, program) == 4,
              "offset of GetUniformIndices program should be 4");
static_assert(offsetof(GetUniformIndices, names_bucket_id) == 8,
              "offset of GetUniformIndices names_bucket_id should be 8");
static_assert(offsetof(GetUniformIndices, indices_shm_id) == 12,
              "offset of GetUniformIndices indices_shm_id should be 12");
static_assert(offsetof(GetUniformIndices, indices_shm_offset) == 16,
              "offset of GetUniformIndices indices_shm_offset should be 16");

struct GetUniformLocation {
  typedef GetUniformLocation ValueType;
  static const CommandId kCmdId = kGetUniformLocation;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    SetHeader();
    program = _program;
    name_bucket_id = _name_bucket_id;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _name_bucket_id,
                                       _location_shm_id, _location_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t name_bucket_id;
  uint32_t location_shm_id;
  uint32_t location_shm_offset;
};

static_assert(sizeof(GetUniformLocation) == 20,
              "size of GetUniformLocation should be 20");
static_assert(offsetof(GetUniformLocation, header) == 0,
              "offset of GetUniformLocation header should be 0");
static_assert(offsetof(GetUniformLocation, program) == 4,
              "offset of GetUniformLocation program should be 4");
static_assert(offsetof(GetUniformLocation, name_bucket_id) == 8,
              "offset of GetUniformLocation name_bucket_id should be 8");
static_assert(offsetof(GetUniformLocation, location_shm_id) == 12,
              "offset of GetUniformLocation location_shm_id should be 12");
static_assert(offsetof(GetUniformLocation, location_shm_offset) == 16,
              "offset of GetUniformLocation location_shm_offset should be 16");

struct GetVertexAttribfv {
  typedef GetVertexAttribfv ValueType;
  static const CommandId kCmdId = kGetVertexAttribfv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLfloat> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    index = _index;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_index, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetVertexAttribfv) == 20,
              "size of GetVertexAttribfv should be 20");
static_assert(offsetof(GetVertexAttribfv, header) == 0,
              "offset of GetVertexAttribfv header should be 0");
static_assert(offsetof(GetVertexAttribfv, index) == 4,
              "offset of GetVertexAttribfv index should be 4");
static_assert(offsetof(GetVertexAttribfv, pname) == 8,
              "offset of GetVertexAttribfv pname should be 8");
static_assert(offsetof(GetVertexAttribfv, params_shm_id) == 12,
              "offset of GetVertexAttribfv params_shm_id should be 12");
static_assert(offsetof(GetVertexAttribfv, params_shm_offset) == 16,
              "offset of GetVertexAttribfv params_shm_offset should be 16");

struct GetVertexAttribiv {
  typedef GetVertexAttribiv ValueType;
  static const CommandId kCmdId = kGetVertexAttribiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    index = _index;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_index, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetVertexAttribiv) == 20,
              "size of GetVertexAttribiv should be 20");
static_assert(offsetof(GetVertexAttribiv, header) == 0,
              "offset of GetVertexAttribiv header should be 0");
static_assert(offsetof(GetVertexAttribiv, index) == 4,
              "offset of GetVertexAttribiv index should be 4");
static_assert(offsetof(GetVertexAttribiv, pname) == 8,
              "offset of GetVertexAttribiv pname should be 8");
static_assert(offsetof(GetVertexAttribiv, params_shm_id) == 12,
              "offset of GetVertexAttribiv params_shm_id should be 12");
static_assert(offsetof(GetVertexAttribiv, params_shm_offset) == 16,
              "offset of GetVertexAttribiv params_shm_offset should be 16");

struct GetVertexAttribIiv {
  typedef GetVertexAttribIiv ValueType;
  static const CommandId kCmdId = kGetVertexAttribIiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    index = _index;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_index, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetVertexAttribIiv) == 20,
              "size of GetVertexAttribIiv should be 20");
static_assert(offsetof(GetVertexAttribIiv, header) == 0,
              "offset of GetVertexAttribIiv header should be 0");
static_assert(offsetof(GetVertexAttribIiv, index) == 4,
              "offset of GetVertexAttribIiv index should be 4");
static_assert(offsetof(GetVertexAttribIiv, pname) == 8,
              "offset of GetVertexAttribIiv pname should be 8");
static_assert(offsetof(GetVertexAttribIiv, params_shm_id) == 12,
              "offset of GetVertexAttribIiv params_shm_id should be 12");
static_assert(offsetof(GetVertexAttribIiv, params_shm_offset) == 16,
              "offset of GetVertexAttribIiv params_shm_offset should be 16");

struct GetVertexAttribIuiv {
  typedef GetVertexAttribIuiv ValueType;
  static const CommandId kCmdId = kGetVertexAttribIuiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLuint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    index = _index;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _index,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_index, _pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetVertexAttribIuiv) == 20,
              "size of GetVertexAttribIuiv should be 20");
static_assert(offsetof(GetVertexAttribIuiv, header) == 0,
              "offset of GetVertexAttribIuiv header should be 0");
static_assert(offsetof(GetVertexAttribIuiv, index) == 4,
              "offset of GetVertexAttribIuiv index should be 4");
static_assert(offsetof(GetVertexAttribIuiv, pname) == 8,
              "offset of GetVertexAttribIuiv pname should be 8");
static_assert(offsetof(GetVertexAttribIuiv, params_shm_id) == 12,
              "offset of GetVertexAttribIuiv params_shm_id should be 12");
static_assert(offsetof(GetVertexAttribIuiv, params_shm_offset) == 16,
              "offset of GetVertexAttribIuiv params_shm_offset should be 16");

struct GetVertexAttribPointerv {
  typedef GetVertexAttribPointerv ValueType;
  static const CommandId kCmdId = kGetVertexAttribPointerv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLuint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index,
            GLenum _pname,
            uint32_t _pointer_shm_id,
            uint32_t _pointer_shm_offset) {
    SetHeader();
    index = _index;
    pname = _pname;
    pointer_shm_id = _pointer_shm_id;
    pointer_shm_offset = _pointer_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _index,
            GLenum _pname,
            uint32_t _pointer_shm_id,
            uint32_t _pointer_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_index, _pname, _pointer_shm_id,
                                       _pointer_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
  uint32_t pname;
  uint32_t pointer_shm_id;
  uint32_t pointer_shm_offset;
};

static_assert(sizeof(GetVertexAttribPointerv) == 20,
              "size of GetVertexAttribPointerv should be 20");
static_assert(offsetof(GetVertexAttribPointerv, header) == 0,
              "offset of GetVertexAttribPointerv header should be 0");
static_assert(offsetof(GetVertexAttribPointerv, index) == 4,
              "offset of GetVertexAttribPointerv index should be 4");
static_assert(offsetof(GetVertexAttribPointerv, pname) == 8,
              "offset of GetVertexAttribPointerv pname should be 8");
static_assert(offsetof(GetVertexAttribPointerv, pointer_shm_id) == 12,
              "offset of GetVertexAttribPointerv pointer_shm_id should be 12");
static_assert(
    offsetof(GetVertexAttribPointerv, pointer_shm_offset) == 16,
    "offset of GetVertexAttribPointerv pointer_shm_offset should be 16");

struct Hint {
  typedef Hint ValueType;
  static const CommandId kCmdId = kHint;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLenum _mode) {
    SetHeader();
    target = _target;
    mode = _mode;
  }

  void* Set(void* cmd, GLenum _target, GLenum _mode) {
    static_cast<ValueType*>(cmd)->Init(_target, _mode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t mode;
};

static_assert(sizeof(Hint) == 12, "size of Hint should be 12");
static_assert(offsetof(Hint, header) == 0, "offset of Hint header should be 0");
static_assert(offsetof(Hint, target) == 4, "offset of Hint target should be 4");
static_assert(offsetof(Hint, mode) == 8, "offset of Hint mode should be 8");

struct InvalidateFramebufferImmediate {
  typedef InvalidateFramebufferImmediate ValueType;
  static const CommandId kCmdId = kInvalidateFramebufferImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLenum) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLenum _target, GLsizei _count, const GLenum* _attachments) {
    SetHeader(_count);
    target = _target;
    count = _count;
    memcpy(ImmediateDataAddress(this), _attachments, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _count,
            const GLenum* _attachments) {
    static_cast<ValueType*>(cmd)->Init(_target, _count, _attachments);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t count;
};

static_assert(sizeof(InvalidateFramebufferImmediate) == 12,
              "size of InvalidateFramebufferImmediate should be 12");
static_assert(offsetof(InvalidateFramebufferImmediate, header) == 0,
              "offset of InvalidateFramebufferImmediate header should be 0");
static_assert(offsetof(InvalidateFramebufferImmediate, target) == 4,
              "offset of InvalidateFramebufferImmediate target should be 4");
static_assert(offsetof(InvalidateFramebufferImmediate, count) == 8,
              "offset of InvalidateFramebufferImmediate count should be 8");

struct InvalidateSubFramebufferImmediate {
  typedef InvalidateSubFramebufferImmediate ValueType;
  static const CommandId kCmdId = kInvalidateSubFramebufferImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLenum) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLenum _target,
            GLsizei _count,
            const GLenum* _attachments,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    SetHeader(_count);
    target = _target;
    count = _count;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    memcpy(ImmediateDataAddress(this), _attachments, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _count,
            const GLenum* _attachments,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _count, _attachments, _x, _y,
                                       _width, _height);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t count;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(InvalidateSubFramebufferImmediate) == 28,
              "size of InvalidateSubFramebufferImmediate should be 28");
static_assert(offsetof(InvalidateSubFramebufferImmediate, header) == 0,
              "offset of InvalidateSubFramebufferImmediate header should be 0");
static_assert(offsetof(InvalidateSubFramebufferImmediate, target) == 4,
              "offset of InvalidateSubFramebufferImmediate target should be 4");
static_assert(offsetof(InvalidateSubFramebufferImmediate, count) == 8,
              "offset of InvalidateSubFramebufferImmediate count should be 8");
static_assert(offsetof(InvalidateSubFramebufferImmediate, x) == 12,
              "offset of InvalidateSubFramebufferImmediate x should be 12");
static_assert(offsetof(InvalidateSubFramebufferImmediate, y) == 16,
              "offset of InvalidateSubFramebufferImmediate y should be 16");
static_assert(offsetof(InvalidateSubFramebufferImmediate, width) == 20,
              "offset of InvalidateSubFramebufferImmediate width should be 20");
static_assert(
    offsetof(InvalidateSubFramebufferImmediate, height) == 24,
    "offset of InvalidateSubFramebufferImmediate height should be 24");

struct IsBuffer {
  typedef IsBuffer ValueType;
  static const CommandId kCmdId = kIsBuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _buffer,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    buffer = _buffer;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _buffer,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_buffer, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t buffer;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsBuffer) == 16, "size of IsBuffer should be 16");
static_assert(offsetof(IsBuffer, header) == 0,
              "offset of IsBuffer header should be 0");
static_assert(offsetof(IsBuffer, buffer) == 4,
              "offset of IsBuffer buffer should be 4");
static_assert(offsetof(IsBuffer, result_shm_id) == 8,
              "offset of IsBuffer result_shm_id should be 8");
static_assert(offsetof(IsBuffer, result_shm_offset) == 12,
              "offset of IsBuffer result_shm_offset should be 12");

struct IsEnabled {
  typedef IsEnabled ValueType;
  static const CommandId kCmdId = kIsEnabled;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _cap, uint32_t _result_shm_id, uint32_t _result_shm_offset) {
    SetHeader();
    cap = _cap;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _cap,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_cap, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t cap;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsEnabled) == 16, "size of IsEnabled should be 16");
static_assert(offsetof(IsEnabled, header) == 0,
              "offset of IsEnabled header should be 0");
static_assert(offsetof(IsEnabled, cap) == 4,
              "offset of IsEnabled cap should be 4");
static_assert(offsetof(IsEnabled, result_shm_id) == 8,
              "offset of IsEnabled result_shm_id should be 8");
static_assert(offsetof(IsEnabled, result_shm_offset) == 12,
              "offset of IsEnabled result_shm_offset should be 12");

struct IsFramebuffer {
  typedef IsFramebuffer ValueType;
  static const CommandId kCmdId = kIsFramebuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _framebuffer,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    framebuffer = _framebuffer;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _framebuffer,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_framebuffer, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t framebuffer;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsFramebuffer) == 16,
              "size of IsFramebuffer should be 16");
static_assert(offsetof(IsFramebuffer, header) == 0,
              "offset of IsFramebuffer header should be 0");
static_assert(offsetof(IsFramebuffer, framebuffer) == 4,
              "offset of IsFramebuffer framebuffer should be 4");
static_assert(offsetof(IsFramebuffer, result_shm_id) == 8,
              "offset of IsFramebuffer result_shm_id should be 8");
static_assert(offsetof(IsFramebuffer, result_shm_offset) == 12,
              "offset of IsFramebuffer result_shm_offset should be 12");

struct IsProgram {
  typedef IsProgram ValueType;
  static const CommandId kCmdId = kIsProgram;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    program = _program;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsProgram) == 16, "size of IsProgram should be 16");
static_assert(offsetof(IsProgram, header) == 0,
              "offset of IsProgram header should be 0");
static_assert(offsetof(IsProgram, program) == 4,
              "offset of IsProgram program should be 4");
static_assert(offsetof(IsProgram, result_shm_id) == 8,
              "offset of IsProgram result_shm_id should be 8");
static_assert(offsetof(IsProgram, result_shm_offset) == 12,
              "offset of IsProgram result_shm_offset should be 12");

struct IsRenderbuffer {
  typedef IsRenderbuffer ValueType;
  static const CommandId kCmdId = kIsRenderbuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _renderbuffer,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    renderbuffer = _renderbuffer;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _renderbuffer,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_renderbuffer, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t renderbuffer;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsRenderbuffer) == 16,
              "size of IsRenderbuffer should be 16");
static_assert(offsetof(IsRenderbuffer, header) == 0,
              "offset of IsRenderbuffer header should be 0");
static_assert(offsetof(IsRenderbuffer, renderbuffer) == 4,
              "offset of IsRenderbuffer renderbuffer should be 4");
static_assert(offsetof(IsRenderbuffer, result_shm_id) == 8,
              "offset of IsRenderbuffer result_shm_id should be 8");
static_assert(offsetof(IsRenderbuffer, result_shm_offset) == 12,
              "offset of IsRenderbuffer result_shm_offset should be 12");

struct IsSampler {
  typedef IsSampler ValueType;
  static const CommandId kCmdId = kIsSampler;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sampler,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    sampler = _sampler;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _sampler,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_sampler, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sampler;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsSampler) == 16, "size of IsSampler should be 16");
static_assert(offsetof(IsSampler, header) == 0,
              "offset of IsSampler header should be 0");
static_assert(offsetof(IsSampler, sampler) == 4,
              "offset of IsSampler sampler should be 4");
static_assert(offsetof(IsSampler, result_shm_id) == 8,
              "offset of IsSampler result_shm_id should be 8");
static_assert(offsetof(IsSampler, result_shm_offset) == 12,
              "offset of IsSampler result_shm_offset should be 12");

struct IsShader {
  typedef IsShader ValueType;
  static const CommandId kCmdId = kIsShader;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    shader = _shader;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _shader,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_shader, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsShader) == 16, "size of IsShader should be 16");
static_assert(offsetof(IsShader, header) == 0,
              "offset of IsShader header should be 0");
static_assert(offsetof(IsShader, shader) == 4,
              "offset of IsShader shader should be 4");
static_assert(offsetof(IsShader, result_shm_id) == 8,
              "offset of IsShader result_shm_id should be 8");
static_assert(offsetof(IsShader, result_shm_offset) == 12,
              "offset of IsShader result_shm_offset should be 12");

struct IsSync {
  typedef IsSync ValueType;
  static const CommandId kCmdId = kIsSync;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sync,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    sync = _sync;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _sync,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_sync, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sync;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsSync) == 16, "size of IsSync should be 16");
static_assert(offsetof(IsSync, header) == 0,
              "offset of IsSync header should be 0");
static_assert(offsetof(IsSync, sync) == 4, "offset of IsSync sync should be 4");
static_assert(offsetof(IsSync, result_shm_id) == 8,
              "offset of IsSync result_shm_id should be 8");
static_assert(offsetof(IsSync, result_shm_offset) == 12,
              "offset of IsSync result_shm_offset should be 12");

struct IsTexture {
  typedef IsTexture ValueType;
  static const CommandId kCmdId = kIsTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    texture = _texture;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _texture,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_texture, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsTexture) == 16, "size of IsTexture should be 16");
static_assert(offsetof(IsTexture, header) == 0,
              "offset of IsTexture header should be 0");
static_assert(offsetof(IsTexture, texture) == 4,
              "offset of IsTexture texture should be 4");
static_assert(offsetof(IsTexture, result_shm_id) == 8,
              "offset of IsTexture result_shm_id should be 8");
static_assert(offsetof(IsTexture, result_shm_offset) == 12,
              "offset of IsTexture result_shm_offset should be 12");

struct IsTransformFeedback {
  typedef IsTransformFeedback ValueType;
  static const CommandId kCmdId = kIsTransformFeedback;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _transformfeedback,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    transformfeedback = _transformfeedback;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _transformfeedback,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_transformfeedback, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t transformfeedback;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsTransformFeedback) == 16,
              "size of IsTransformFeedback should be 16");
static_assert(offsetof(IsTransformFeedback, header) == 0,
              "offset of IsTransformFeedback header should be 0");
static_assert(offsetof(IsTransformFeedback, transformfeedback) == 4,
              "offset of IsTransformFeedback transformfeedback should be 4");
static_assert(offsetof(IsTransformFeedback, result_shm_id) == 8,
              "offset of IsTransformFeedback result_shm_id should be 8");
static_assert(offsetof(IsTransformFeedback, result_shm_offset) == 12,
              "offset of IsTransformFeedback result_shm_offset should be 12");

struct LineWidth {
  typedef LineWidth ValueType;
  static const CommandId kCmdId = kLineWidth;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLfloat _width) {
    SetHeader();
    width = _width;
  }

  void* Set(void* cmd, GLfloat _width) {
    static_cast<ValueType*>(cmd)->Init(_width);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float width;
};

static_assert(sizeof(LineWidth) == 8, "size of LineWidth should be 8");
static_assert(offsetof(LineWidth, header) == 0,
              "offset of LineWidth header should be 0");
static_assert(offsetof(LineWidth, width) == 4,
              "offset of LineWidth width should be 4");

struct LinkProgram {
  typedef LinkProgram ValueType;
  static const CommandId kCmdId = kLinkProgram;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program) {
    SetHeader();
    program = _program;
  }

  void* Set(void* cmd, GLuint _program) {
    static_cast<ValueType*>(cmd)->Init(_program);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
};

static_assert(sizeof(LinkProgram) == 8, "size of LinkProgram should be 8");
static_assert(offsetof(LinkProgram, header) == 0,
              "offset of LinkProgram header should be 0");
static_assert(offsetof(LinkProgram, program) == 4,
              "offset of LinkProgram program should be 4");

struct PauseTransformFeedback {
  typedef PauseTransformFeedback ValueType;
  static const CommandId kCmdId = kPauseTransformFeedback;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(PauseTransformFeedback) == 4,
              "size of PauseTransformFeedback should be 4");
static_assert(offsetof(PauseTransformFeedback, header) == 0,
              "offset of PauseTransformFeedback header should be 0");

struct PixelStorei {
  typedef PixelStorei ValueType;
  static const CommandId kCmdId = kPixelStorei;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname, GLint _param) {
    SetHeader();
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLenum _pname, GLint _param) {
    static_cast<ValueType*>(cmd)->Init(_pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  int32_t param;
};

static_assert(sizeof(PixelStorei) == 12, "size of PixelStorei should be 12");
static_assert(offsetof(PixelStorei, header) == 0,
              "offset of PixelStorei header should be 0");
static_assert(offsetof(PixelStorei, pname) == 4,
              "offset of PixelStorei pname should be 4");
static_assert(offsetof(PixelStorei, param) == 8,
              "offset of PixelStorei param should be 8");

struct PolygonOffset {
  typedef PolygonOffset ValueType;
  static const CommandId kCmdId = kPolygonOffset;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLfloat _factor, GLfloat _units) {
    SetHeader();
    factor = _factor;
    units = _units;
  }

  void* Set(void* cmd, GLfloat _factor, GLfloat _units) {
    static_cast<ValueType*>(cmd)->Init(_factor, _units);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float factor;
  float units;
};

static_assert(sizeof(PolygonOffset) == 12,
              "size of PolygonOffset should be 12");
static_assert(offsetof(PolygonOffset, header) == 0,
              "offset of PolygonOffset header should be 0");
static_assert(offsetof(PolygonOffset, factor) == 4,
              "offset of PolygonOffset factor should be 4");
static_assert(offsetof(PolygonOffset, units) == 8,
              "offset of PolygonOffset units should be 8");

struct ReadBuffer {
  typedef ReadBuffer ValueType;
  static const CommandId kCmdId = kReadBuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _src) {
    SetHeader();
    src = _src;
  }

  void* Set(void* cmd, GLenum _src) {
    static_cast<ValueType*>(cmd)->Init(_src);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t src;
};

static_assert(sizeof(ReadBuffer) == 8, "size of ReadBuffer should be 8");
static_assert(offsetof(ReadBuffer, header) == 0,
              "offset of ReadBuffer header should be 0");
static_assert(offsetof(ReadBuffer, src) == 4,
              "offset of ReadBuffer src should be 4");

// ReadPixels has the result separated from the pixel buffer so that
// it is easier to specify the result going to some specific place
// that exactly fits the rectangle of pixels.
struct ReadPixels {
  typedef ReadPixels ValueType;
  static const CommandId kCmdId = kReadPixels;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  struct Result {
    uint32_t success;
    int32_t row_length;
    int32_t num_rows;
  };

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset,
            GLboolean _async) {
    SetHeader();
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    format = _format;
    type = _type;
    pixels_shm_id = _pixels_shm_id;
    pixels_shm_offset = _pixels_shm_offset;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
    async = _async;
  }

  void* Set(void* cmd,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset,
            GLboolean _async) {
    static_cast<ValueType*>(cmd)->Init(
        _x, _y, _width, _height, _format, _type, _pixels_shm_id,
        _pixels_shm_offset, _result_shm_id, _result_shm_offset, _async);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  uint32_t format;
  uint32_t type;
  uint32_t pixels_shm_id;
  uint32_t pixels_shm_offset;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
  uint32_t async;
};

static_assert(sizeof(ReadPixels) == 48, "size of ReadPixels should be 48");
static_assert(offsetof(ReadPixels, header) == 0,
              "offset of ReadPixels header should be 0");
static_assert(offsetof(ReadPixels, x) == 4,
              "offset of ReadPixels x should be 4");
static_assert(offsetof(ReadPixels, y) == 8,
              "offset of ReadPixels y should be 8");
static_assert(offsetof(ReadPixels, width) == 12,
              "offset of ReadPixels width should be 12");
static_assert(offsetof(ReadPixels, height) == 16,
              "offset of ReadPixels height should be 16");
static_assert(offsetof(ReadPixels, format) == 20,
              "offset of ReadPixels format should be 20");
static_assert(offsetof(ReadPixels, type) == 24,
              "offset of ReadPixels type should be 24");
static_assert(offsetof(ReadPixels, pixels_shm_id) == 28,
              "offset of ReadPixels pixels_shm_id should be 28");
static_assert(offsetof(ReadPixels, pixels_shm_offset) == 32,
              "offset of ReadPixels pixels_shm_offset should be 32");
static_assert(offsetof(ReadPixels, result_shm_id) == 36,
              "offset of ReadPixels result_shm_id should be 36");
static_assert(offsetof(ReadPixels, result_shm_offset) == 40,
              "offset of ReadPixels result_shm_offset should be 40");
static_assert(offsetof(ReadPixels, async) == 44,
              "offset of ReadPixels async should be 44");
static_assert(offsetof(ReadPixels::Result, success) == 0,
              "offset of ReadPixels Result success should be "
              "0");
static_assert(offsetof(ReadPixels::Result, row_length) == 4,
              "offset of ReadPixels Result row_length should be "
              "4");
static_assert(offsetof(ReadPixels::Result, num_rows) == 8,
              "offset of ReadPixels Result num_rows should be "
              "8");

struct ReleaseShaderCompiler {
  typedef ReleaseShaderCompiler ValueType;
  static const CommandId kCmdId = kReleaseShaderCompiler;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(ReleaseShaderCompiler) == 4,
              "size of ReleaseShaderCompiler should be 4");
static_assert(offsetof(ReleaseShaderCompiler, header) == 0,
              "offset of ReleaseShaderCompiler header should be 0");

struct RenderbufferStorage {
  typedef RenderbufferStorage ValueType;
  static const CommandId kCmdId = kRenderbufferStorage;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    internalformat = _internalformat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _internalformat, _width,
                                       _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(RenderbufferStorage) == 20,
              "size of RenderbufferStorage should be 20");
static_assert(offsetof(RenderbufferStorage, header) == 0,
              "offset of RenderbufferStorage header should be 0");
static_assert(offsetof(RenderbufferStorage, target) == 4,
              "offset of RenderbufferStorage target should be 4");
static_assert(offsetof(RenderbufferStorage, internalformat) == 8,
              "offset of RenderbufferStorage internalformat should be 8");
static_assert(offsetof(RenderbufferStorage, width) == 12,
              "offset of RenderbufferStorage width should be 12");
static_assert(offsetof(RenderbufferStorage, height) == 16,
              "offset of RenderbufferStorage height should be 16");

struct ResumeTransformFeedback {
  typedef ResumeTransformFeedback ValueType;
  static const CommandId kCmdId = kResumeTransformFeedback;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(ResumeTransformFeedback) == 4,
              "size of ResumeTransformFeedback should be 4");
static_assert(offsetof(ResumeTransformFeedback, header) == 0,
              "offset of ResumeTransformFeedback header should be 0");

struct SampleCoverage {
  typedef SampleCoverage ValueType;
  static const CommandId kCmdId = kSampleCoverage;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLclampf _value, GLboolean _invert) {
    SetHeader();
    value = _value;
    invert = _invert;
  }

  void* Set(void* cmd, GLclampf _value, GLboolean _invert) {
    static_cast<ValueType*>(cmd)->Init(_value, _invert);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float value;
  uint32_t invert;
};

static_assert(sizeof(SampleCoverage) == 12,
              "size of SampleCoverage should be 12");
static_assert(offsetof(SampleCoverage, header) == 0,
              "offset of SampleCoverage header should be 0");
static_assert(offsetof(SampleCoverage, value) == 4,
              "offset of SampleCoverage value should be 4");
static_assert(offsetof(SampleCoverage, invert) == 8,
              "offset of SampleCoverage invert should be 8");

struct SamplerParameterf {
  typedef SamplerParameterf ValueType;
  static const CommandId kCmdId = kSamplerParameterf;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sampler, GLenum _pname, GLfloat _param) {
    SetHeader();
    sampler = _sampler;
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLuint _sampler, GLenum _pname, GLfloat _param) {
    static_cast<ValueType*>(cmd)->Init(_sampler, _pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sampler;
  uint32_t pname;
  float param;
};

static_assert(sizeof(SamplerParameterf) == 16,
              "size of SamplerParameterf should be 16");
static_assert(offsetof(SamplerParameterf, header) == 0,
              "offset of SamplerParameterf header should be 0");
static_assert(offsetof(SamplerParameterf, sampler) == 4,
              "offset of SamplerParameterf sampler should be 4");
static_assert(offsetof(SamplerParameterf, pname) == 8,
              "offset of SamplerParameterf pname should be 8");
static_assert(offsetof(SamplerParameterf, param) == 12,
              "offset of SamplerParameterf param should be 12");

struct SamplerParameterfvImmediate {
  typedef SamplerParameterfvImmediate ValueType;
  static const CommandId kCmdId = kSamplerParameterfvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 1);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _sampler, GLenum _pname, const GLfloat* _params) {
    SetHeader();
    sampler = _sampler;
    pname = _pname;
    memcpy(ImmediateDataAddress(this), _params, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _sampler, GLenum _pname, const GLfloat* _params) {
    static_cast<ValueType*>(cmd)->Init(_sampler, _pname, _params);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t sampler;
  uint32_t pname;
};

static_assert(sizeof(SamplerParameterfvImmediate) == 12,
              "size of SamplerParameterfvImmediate should be 12");
static_assert(offsetof(SamplerParameterfvImmediate, header) == 0,
              "offset of SamplerParameterfvImmediate header should be 0");
static_assert(offsetof(SamplerParameterfvImmediate, sampler) == 4,
              "offset of SamplerParameterfvImmediate sampler should be 4");
static_assert(offsetof(SamplerParameterfvImmediate, pname) == 8,
              "offset of SamplerParameterfvImmediate pname should be 8");

struct SamplerParameteri {
  typedef SamplerParameteri ValueType;
  static const CommandId kCmdId = kSamplerParameteri;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sampler, GLenum _pname, GLint _param) {
    SetHeader();
    sampler = _sampler;
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLuint _sampler, GLenum _pname, GLint _param) {
    static_cast<ValueType*>(cmd)->Init(_sampler, _pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sampler;
  uint32_t pname;
  int32_t param;
};

static_assert(sizeof(SamplerParameteri) == 16,
              "size of SamplerParameteri should be 16");
static_assert(offsetof(SamplerParameteri, header) == 0,
              "offset of SamplerParameteri header should be 0");
static_assert(offsetof(SamplerParameteri, sampler) == 4,
              "offset of SamplerParameteri sampler should be 4");
static_assert(offsetof(SamplerParameteri, pname) == 8,
              "offset of SamplerParameteri pname should be 8");
static_assert(offsetof(SamplerParameteri, param) == 12,
              "offset of SamplerParameteri param should be 12");

struct SamplerParameterivImmediate {
  typedef SamplerParameterivImmediate ValueType;
  static const CommandId kCmdId = kSamplerParameterivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLint) * 1);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _sampler, GLenum _pname, const GLint* _params) {
    SetHeader();
    sampler = _sampler;
    pname = _pname;
    memcpy(ImmediateDataAddress(this), _params, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _sampler, GLenum _pname, const GLint* _params) {
    static_cast<ValueType*>(cmd)->Init(_sampler, _pname, _params);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t sampler;
  uint32_t pname;
};

static_assert(sizeof(SamplerParameterivImmediate) == 12,
              "size of SamplerParameterivImmediate should be 12");
static_assert(offsetof(SamplerParameterivImmediate, header) == 0,
              "offset of SamplerParameterivImmediate header should be 0");
static_assert(offsetof(SamplerParameterivImmediate, sampler) == 4,
              "offset of SamplerParameterivImmediate sampler should be 4");
static_assert(offsetof(SamplerParameterivImmediate, pname) == 8,
              "offset of SamplerParameterivImmediate pname should be 8");

struct Scissor {
  typedef Scissor ValueType;
  static const CommandId kCmdId = kScissor;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _x, GLint _y, GLsizei _width, GLsizei _height) {
    SetHeader();
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd, GLint _x, GLint _y, GLsizei _width, GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_x, _y, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(Scissor) == 20, "size of Scissor should be 20");
static_assert(offsetof(Scissor, header) == 0,
              "offset of Scissor header should be 0");
static_assert(offsetof(Scissor, x) == 4, "offset of Scissor x should be 4");
static_assert(offsetof(Scissor, y) == 8, "offset of Scissor y should be 8");
static_assert(offsetof(Scissor, width) == 12,
              "offset of Scissor width should be 12");
static_assert(offsetof(Scissor, height) == 16,
              "offset of Scissor height should be 16");

struct ShaderBinary {
  typedef ShaderBinary ValueType;
  static const CommandId kCmdId = kShaderBinary;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _n,
            uint32_t _shaders_shm_id,
            uint32_t _shaders_shm_offset,
            GLenum _binaryformat,
            uint32_t _binary_shm_id,
            uint32_t _binary_shm_offset,
            GLsizei _length) {
    SetHeader();
    n = _n;
    shaders_shm_id = _shaders_shm_id;
    shaders_shm_offset = _shaders_shm_offset;
    binaryformat = _binaryformat;
    binary_shm_id = _binary_shm_id;
    binary_shm_offset = _binary_shm_offset;
    length = _length;
  }

  void* Set(void* cmd,
            GLsizei _n,
            uint32_t _shaders_shm_id,
            uint32_t _shaders_shm_offset,
            GLenum _binaryformat,
            uint32_t _binary_shm_id,
            uint32_t _binary_shm_offset,
            GLsizei _length) {
    static_cast<ValueType*>(cmd)->Init(_n, _shaders_shm_id, _shaders_shm_offset,
                                       _binaryformat, _binary_shm_id,
                                       _binary_shm_offset, _length);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t n;
  uint32_t shaders_shm_id;
  uint32_t shaders_shm_offset;
  uint32_t binaryformat;
  uint32_t binary_shm_id;
  uint32_t binary_shm_offset;
  int32_t length;
};

static_assert(sizeof(ShaderBinary) == 32, "size of ShaderBinary should be 32");
static_assert(offsetof(ShaderBinary, header) == 0,
              "offset of ShaderBinary header should be 0");
static_assert(offsetof(ShaderBinary, n) == 4,
              "offset of ShaderBinary n should be 4");
static_assert(offsetof(ShaderBinary, shaders_shm_id) == 8,
              "offset of ShaderBinary shaders_shm_id should be 8");
static_assert(offsetof(ShaderBinary, shaders_shm_offset) == 12,
              "offset of ShaderBinary shaders_shm_offset should be 12");
static_assert(offsetof(ShaderBinary, binaryformat) == 16,
              "offset of ShaderBinary binaryformat should be 16");
static_assert(offsetof(ShaderBinary, binary_shm_id) == 20,
              "offset of ShaderBinary binary_shm_id should be 20");
static_assert(offsetof(ShaderBinary, binary_shm_offset) == 24,
              "offset of ShaderBinary binary_shm_offset should be 24");
static_assert(offsetof(ShaderBinary, length) == 28,
              "offset of ShaderBinary length should be 28");

struct ShaderSourceBucket {
  typedef ShaderSourceBucket ValueType;
  static const CommandId kCmdId = kShaderSourceBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader, uint32_t _str_bucket_id) {
    SetHeader();
    shader = _shader;
    str_bucket_id = _str_bucket_id;
  }

  void* Set(void* cmd, GLuint _shader, uint32_t _str_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_shader, _str_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
  uint32_t str_bucket_id;
};

static_assert(sizeof(ShaderSourceBucket) == 12,
              "size of ShaderSourceBucket should be 12");
static_assert(offsetof(ShaderSourceBucket, header) == 0,
              "offset of ShaderSourceBucket header should be 0");
static_assert(offsetof(ShaderSourceBucket, shader) == 4,
              "offset of ShaderSourceBucket shader should be 4");
static_assert(offsetof(ShaderSourceBucket, str_bucket_id) == 8,
              "offset of ShaderSourceBucket str_bucket_id should be 8");

struct MultiDrawBeginCHROMIUM {
  typedef MultiDrawBeginCHROMIUM ValueType;
  static const CommandId kCmdId = kMultiDrawBeginCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _drawcount) {
    SetHeader();
    drawcount = _drawcount;
  }

  void* Set(void* cmd, GLsizei _drawcount) {
    static_cast<ValueType*>(cmd)->Init(_drawcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t drawcount;
};

static_assert(sizeof(MultiDrawBeginCHROMIUM) == 8,
              "size of MultiDrawBeginCHROMIUM should be 8");
static_assert(offsetof(MultiDrawBeginCHROMIUM, header) == 0,
              "offset of MultiDrawBeginCHROMIUM header should be 0");
static_assert(offsetof(MultiDrawBeginCHROMIUM, drawcount) == 4,
              "offset of MultiDrawBeginCHROMIUM drawcount should be 4");

struct MultiDrawEndCHROMIUM {
  typedef MultiDrawEndCHROMIUM ValueType;
  static const CommandId kCmdId = kMultiDrawEndCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(MultiDrawEndCHROMIUM) == 4,
              "size of MultiDrawEndCHROMIUM should be 4");
static_assert(offsetof(MultiDrawEndCHROMIUM, header) == 0,
              "offset of MultiDrawEndCHROMIUM header should be 0");

struct MultiDrawArraysCHROMIUM {
  typedef MultiDrawArraysCHROMIUM ValueType;
  static const CommandId kCmdId = kMultiDrawArraysCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            uint32_t _firsts_shm_id,
            uint32_t _firsts_shm_offset,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLsizei _drawcount) {
    SetHeader();
    mode = _mode;
    firsts_shm_id = _firsts_shm_id;
    firsts_shm_offset = _firsts_shm_offset;
    counts_shm_id = _counts_shm_id;
    counts_shm_offset = _counts_shm_offset;
    drawcount = _drawcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            uint32_t _firsts_shm_id,
            uint32_t _firsts_shm_offset,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLsizei _drawcount) {
    static_cast<ValueType*>(cmd)->Init(_mode, _firsts_shm_id,
                                       _firsts_shm_offset, _counts_shm_id,
                                       _counts_shm_offset, _drawcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t firsts_shm_id;
  uint32_t firsts_shm_offset;
  uint32_t counts_shm_id;
  uint32_t counts_shm_offset;
  int32_t drawcount;
};

static_assert(sizeof(MultiDrawArraysCHROMIUM) == 28,
              "size of MultiDrawArraysCHROMIUM should be 28");
static_assert(offsetof(MultiDrawArraysCHROMIUM, header) == 0,
              "offset of MultiDrawArraysCHROMIUM header should be 0");
static_assert(offsetof(MultiDrawArraysCHROMIUM, mode) == 4,
              "offset of MultiDrawArraysCHROMIUM mode should be 4");
static_assert(offsetof(MultiDrawArraysCHROMIUM, firsts_shm_id) == 8,
              "offset of MultiDrawArraysCHROMIUM firsts_shm_id should be 8");
static_assert(
    offsetof(MultiDrawArraysCHROMIUM, firsts_shm_offset) == 12,
    "offset of MultiDrawArraysCHROMIUM firsts_shm_offset should be 12");
static_assert(offsetof(MultiDrawArraysCHROMIUM, counts_shm_id) == 16,
              "offset of MultiDrawArraysCHROMIUM counts_shm_id should be 16");
static_assert(
    offsetof(MultiDrawArraysCHROMIUM, counts_shm_offset) == 20,
    "offset of MultiDrawArraysCHROMIUM counts_shm_offset should be 20");
static_assert(offsetof(MultiDrawArraysCHROMIUM, drawcount) == 24,
              "offset of MultiDrawArraysCHROMIUM drawcount should be 24");

struct MultiDrawArraysInstancedCHROMIUM {
  typedef MultiDrawArraysInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kMultiDrawArraysInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            uint32_t _firsts_shm_id,
            uint32_t _firsts_shm_offset,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            GLsizei _drawcount) {
    SetHeader();
    mode = _mode;
    firsts_shm_id = _firsts_shm_id;
    firsts_shm_offset = _firsts_shm_offset;
    counts_shm_id = _counts_shm_id;
    counts_shm_offset = _counts_shm_offset;
    instance_counts_shm_id = _instance_counts_shm_id;
    instance_counts_shm_offset = _instance_counts_shm_offset;
    drawcount = _drawcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            uint32_t _firsts_shm_id,
            uint32_t _firsts_shm_offset,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            GLsizei _drawcount) {
    static_cast<ValueType*>(cmd)->Init(
        _mode, _firsts_shm_id, _firsts_shm_offset, _counts_shm_id,
        _counts_shm_offset, _instance_counts_shm_id,
        _instance_counts_shm_offset, _drawcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t firsts_shm_id;
  uint32_t firsts_shm_offset;
  uint32_t counts_shm_id;
  uint32_t counts_shm_offset;
  uint32_t instance_counts_shm_id;
  uint32_t instance_counts_shm_offset;
  int32_t drawcount;
};

static_assert(sizeof(MultiDrawArraysInstancedCHROMIUM) == 36,
              "size of MultiDrawArraysInstancedCHROMIUM should be 36");
static_assert(offsetof(MultiDrawArraysInstancedCHROMIUM, header) == 0,
              "offset of MultiDrawArraysInstancedCHROMIUM header should be 0");
static_assert(offsetof(MultiDrawArraysInstancedCHROMIUM, mode) == 4,
              "offset of MultiDrawArraysInstancedCHROMIUM mode should be 4");
static_assert(
    offsetof(MultiDrawArraysInstancedCHROMIUM, firsts_shm_id) == 8,
    "offset of MultiDrawArraysInstancedCHROMIUM firsts_shm_id should be 8");
static_assert(offsetof(MultiDrawArraysInstancedCHROMIUM, firsts_shm_offset) ==
                  12,
              "offset of MultiDrawArraysInstancedCHROMIUM firsts_shm_offset "
              "should be 12");
static_assert(
    offsetof(MultiDrawArraysInstancedCHROMIUM, counts_shm_id) == 16,
    "offset of MultiDrawArraysInstancedCHROMIUM counts_shm_id should be 16");
static_assert(offsetof(MultiDrawArraysInstancedCHROMIUM, counts_shm_offset) ==
                  20,
              "offset of MultiDrawArraysInstancedCHROMIUM counts_shm_offset "
              "should be 20");
static_assert(offsetof(MultiDrawArraysInstancedCHROMIUM,
                       instance_counts_shm_id) == 24,
              "offset of MultiDrawArraysInstancedCHROMIUM "
              "instance_counts_shm_id should be 24");
static_assert(offsetof(MultiDrawArraysInstancedCHROMIUM,
                       instance_counts_shm_offset) == 28,
              "offset of MultiDrawArraysInstancedCHROMIUM "
              "instance_counts_shm_offset should be 28");
static_assert(
    offsetof(MultiDrawArraysInstancedCHROMIUM, drawcount) == 32,
    "offset of MultiDrawArraysInstancedCHROMIUM drawcount should be 32");

struct MultiDrawArraysInstancedBaseInstanceCHROMIUM {
  typedef MultiDrawArraysInstancedBaseInstanceCHROMIUM ValueType;
  static const CommandId kCmdId = kMultiDrawArraysInstancedBaseInstanceCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            uint32_t _firsts_shm_id,
            uint32_t _firsts_shm_offset,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            uint32_t _baseinstances_shm_id,
            uint32_t _baseinstances_shm_offset,
            GLsizei _drawcount) {
    SetHeader();
    mode = _mode;
    firsts_shm_id = _firsts_shm_id;
    firsts_shm_offset = _firsts_shm_offset;
    counts_shm_id = _counts_shm_id;
    counts_shm_offset = _counts_shm_offset;
    instance_counts_shm_id = _instance_counts_shm_id;
    instance_counts_shm_offset = _instance_counts_shm_offset;
    baseinstances_shm_id = _baseinstances_shm_id;
    baseinstances_shm_offset = _baseinstances_shm_offset;
    drawcount = _drawcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            uint32_t _firsts_shm_id,
            uint32_t _firsts_shm_offset,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            uint32_t _baseinstances_shm_id,
            uint32_t _baseinstances_shm_offset,
            GLsizei _drawcount) {
    static_cast<ValueType*>(cmd)->Init(
        _mode, _firsts_shm_id, _firsts_shm_offset, _counts_shm_id,
        _counts_shm_offset, _instance_counts_shm_id,
        _instance_counts_shm_offset, _baseinstances_shm_id,
        _baseinstances_shm_offset, _drawcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t firsts_shm_id;
  uint32_t firsts_shm_offset;
  uint32_t counts_shm_id;
  uint32_t counts_shm_offset;
  uint32_t instance_counts_shm_id;
  uint32_t instance_counts_shm_offset;
  uint32_t baseinstances_shm_id;
  uint32_t baseinstances_shm_offset;
  int32_t drawcount;
};

static_assert(
    sizeof(MultiDrawArraysInstancedBaseInstanceCHROMIUM) == 44,
    "size of MultiDrawArraysInstancedBaseInstanceCHROMIUM should be 44");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM, header) ==
                  0,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM header "
              "should be 0");
static_assert(
    offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM, mode) == 4,
    "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM mode should be 4");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       firsts_shm_id) == 8,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "firsts_shm_id should be 8");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       firsts_shm_offset) == 12,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "firsts_shm_offset should be 12");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       counts_shm_id) == 16,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "counts_shm_id should be 16");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       counts_shm_offset) == 20,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "counts_shm_offset should be 20");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       instance_counts_shm_id) == 24,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "instance_counts_shm_id should be 24");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       instance_counts_shm_offset) == 28,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "instance_counts_shm_offset should be 28");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       baseinstances_shm_id) == 32,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "baseinstances_shm_id should be 32");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       baseinstances_shm_offset) == 36,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "baseinstances_shm_offset should be 36");
static_assert(offsetof(MultiDrawArraysInstancedBaseInstanceCHROMIUM,
                       drawcount) == 40,
              "offset of MultiDrawArraysInstancedBaseInstanceCHROMIUM "
              "drawcount should be 40");

struct MultiDrawElementsCHROMIUM {
  typedef MultiDrawElementsCHROMIUM ValueType;
  static const CommandId kCmdId = kMultiDrawElementsCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLenum _type,
            uint32_t _offsets_shm_id,
            uint32_t _offsets_shm_offset,
            GLsizei _drawcount) {
    SetHeader();
    mode = _mode;
    counts_shm_id = _counts_shm_id;
    counts_shm_offset = _counts_shm_offset;
    type = _type;
    offsets_shm_id = _offsets_shm_id;
    offsets_shm_offset = _offsets_shm_offset;
    drawcount = _drawcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLenum _type,
            uint32_t _offsets_shm_id,
            uint32_t _offsets_shm_offset,
            GLsizei _drawcount) {
    static_cast<ValueType*>(cmd)->Init(
        _mode, _counts_shm_id, _counts_shm_offset, _type, _offsets_shm_id,
        _offsets_shm_offset, _drawcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t counts_shm_id;
  uint32_t counts_shm_offset;
  uint32_t type;
  uint32_t offsets_shm_id;
  uint32_t offsets_shm_offset;
  int32_t drawcount;
};

static_assert(sizeof(MultiDrawElementsCHROMIUM) == 32,
              "size of MultiDrawElementsCHROMIUM should be 32");
static_assert(offsetof(MultiDrawElementsCHROMIUM, header) == 0,
              "offset of MultiDrawElementsCHROMIUM header should be 0");
static_assert(offsetof(MultiDrawElementsCHROMIUM, mode) == 4,
              "offset of MultiDrawElementsCHROMIUM mode should be 4");
static_assert(offsetof(MultiDrawElementsCHROMIUM, counts_shm_id) == 8,
              "offset of MultiDrawElementsCHROMIUM counts_shm_id should be 8");
static_assert(
    offsetof(MultiDrawElementsCHROMIUM, counts_shm_offset) == 12,
    "offset of MultiDrawElementsCHROMIUM counts_shm_offset should be 12");
static_assert(offsetof(MultiDrawElementsCHROMIUM, type) == 16,
              "offset of MultiDrawElementsCHROMIUM type should be 16");
static_assert(
    offsetof(MultiDrawElementsCHROMIUM, offsets_shm_id) == 20,
    "offset of MultiDrawElementsCHROMIUM offsets_shm_id should be 20");
static_assert(
    offsetof(MultiDrawElementsCHROMIUM, offsets_shm_offset) == 24,
    "offset of MultiDrawElementsCHROMIUM offsets_shm_offset should be 24");
static_assert(offsetof(MultiDrawElementsCHROMIUM, drawcount) == 28,
              "offset of MultiDrawElementsCHROMIUM drawcount should be 28");

struct MultiDrawElementsInstancedCHROMIUM {
  typedef MultiDrawElementsInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kMultiDrawElementsInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLenum _type,
            uint32_t _offsets_shm_id,
            uint32_t _offsets_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            GLsizei _drawcount) {
    SetHeader();
    mode = _mode;
    counts_shm_id = _counts_shm_id;
    counts_shm_offset = _counts_shm_offset;
    type = _type;
    offsets_shm_id = _offsets_shm_id;
    offsets_shm_offset = _offsets_shm_offset;
    instance_counts_shm_id = _instance_counts_shm_id;
    instance_counts_shm_offset = _instance_counts_shm_offset;
    drawcount = _drawcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLenum _type,
            uint32_t _offsets_shm_id,
            uint32_t _offsets_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            GLsizei _drawcount) {
    static_cast<ValueType*>(cmd)->Init(
        _mode, _counts_shm_id, _counts_shm_offset, _type, _offsets_shm_id,
        _offsets_shm_offset, _instance_counts_shm_id,
        _instance_counts_shm_offset, _drawcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t counts_shm_id;
  uint32_t counts_shm_offset;
  uint32_t type;
  uint32_t offsets_shm_id;
  uint32_t offsets_shm_offset;
  uint32_t instance_counts_shm_id;
  uint32_t instance_counts_shm_offset;
  int32_t drawcount;
};

static_assert(sizeof(MultiDrawElementsInstancedCHROMIUM) == 40,
              "size of MultiDrawElementsInstancedCHROMIUM should be 40");
static_assert(
    offsetof(MultiDrawElementsInstancedCHROMIUM, header) == 0,
    "offset of MultiDrawElementsInstancedCHROMIUM header should be 0");
static_assert(offsetof(MultiDrawElementsInstancedCHROMIUM, mode) == 4,
              "offset of MultiDrawElementsInstancedCHROMIUM mode should be 4");
static_assert(
    offsetof(MultiDrawElementsInstancedCHROMIUM, counts_shm_id) == 8,
    "offset of MultiDrawElementsInstancedCHROMIUM counts_shm_id should be 8");
static_assert(offsetof(MultiDrawElementsInstancedCHROMIUM, counts_shm_offset) ==
                  12,
              "offset of MultiDrawElementsInstancedCHROMIUM counts_shm_offset "
              "should be 12");
static_assert(offsetof(MultiDrawElementsInstancedCHROMIUM, type) == 16,
              "offset of MultiDrawElementsInstancedCHROMIUM type should be 16");
static_assert(
    offsetof(MultiDrawElementsInstancedCHROMIUM, offsets_shm_id) == 20,
    "offset of MultiDrawElementsInstancedCHROMIUM offsets_shm_id should be 20");
static_assert(offsetof(MultiDrawElementsInstancedCHROMIUM,
                       offsets_shm_offset) == 24,
              "offset of MultiDrawElementsInstancedCHROMIUM offsets_shm_offset "
              "should be 24");
static_assert(offsetof(MultiDrawElementsInstancedCHROMIUM,
                       instance_counts_shm_id) == 28,
              "offset of MultiDrawElementsInstancedCHROMIUM "
              "instance_counts_shm_id should be 28");
static_assert(offsetof(MultiDrawElementsInstancedCHROMIUM,
                       instance_counts_shm_offset) == 32,
              "offset of MultiDrawElementsInstancedCHROMIUM "
              "instance_counts_shm_offset should be 32");
static_assert(
    offsetof(MultiDrawElementsInstancedCHROMIUM, drawcount) == 36,
    "offset of MultiDrawElementsInstancedCHROMIUM drawcount should be 36");

struct MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM {
  typedef MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM ValueType;
  static const CommandId kCmdId =
      kMultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLenum _type,
            uint32_t _offsets_shm_id,
            uint32_t _offsets_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            uint32_t _basevertices_shm_id,
            uint32_t _basevertices_shm_offset,
            uint32_t _baseinstances_shm_id,
            uint32_t _baseinstances_shm_offset,
            GLsizei _drawcount) {
    SetHeader();
    mode = _mode;
    counts_shm_id = _counts_shm_id;
    counts_shm_offset = _counts_shm_offset;
    type = _type;
    offsets_shm_id = _offsets_shm_id;
    offsets_shm_offset = _offsets_shm_offset;
    instance_counts_shm_id = _instance_counts_shm_id;
    instance_counts_shm_offset = _instance_counts_shm_offset;
    basevertices_shm_id = _basevertices_shm_id;
    basevertices_shm_offset = _basevertices_shm_offset;
    baseinstances_shm_id = _baseinstances_shm_id;
    baseinstances_shm_offset = _baseinstances_shm_offset;
    drawcount = _drawcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            uint32_t _counts_shm_id,
            uint32_t _counts_shm_offset,
            GLenum _type,
            uint32_t _offsets_shm_id,
            uint32_t _offsets_shm_offset,
            uint32_t _instance_counts_shm_id,
            uint32_t _instance_counts_shm_offset,
            uint32_t _basevertices_shm_id,
            uint32_t _basevertices_shm_offset,
            uint32_t _baseinstances_shm_id,
            uint32_t _baseinstances_shm_offset,
            GLsizei _drawcount) {
    static_cast<ValueType*>(cmd)->Init(
        _mode, _counts_shm_id, _counts_shm_offset, _type, _offsets_shm_id,
        _offsets_shm_offset, _instance_counts_shm_id,
        _instance_counts_shm_offset, _basevertices_shm_id,
        _basevertices_shm_offset, _baseinstances_shm_id,
        _baseinstances_shm_offset, _drawcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t counts_shm_id;
  uint32_t counts_shm_offset;
  uint32_t type;
  uint32_t offsets_shm_id;
  uint32_t offsets_shm_offset;
  uint32_t instance_counts_shm_id;
  uint32_t instance_counts_shm_offset;
  uint32_t basevertices_shm_id;
  uint32_t basevertices_shm_offset;
  uint32_t baseinstances_shm_id;
  uint32_t baseinstances_shm_offset;
  int32_t drawcount;
};

static_assert(
    sizeof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM) == 56,
    "size of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM should "
    "be 56");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             header) == 0,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM header "
    "should be 0");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM, mode) ==
        4,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM mode "
    "should be 4");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             counts_shm_id) == 8,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "counts_shm_id should be 8");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             counts_shm_offset) == 12,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "counts_shm_offset should be 12");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM, type) ==
        16,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM type "
    "should be 16");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             offsets_shm_id) == 20,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "offsets_shm_id should be 20");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             offsets_shm_offset) == 24,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "offsets_shm_offset should be 24");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             instance_counts_shm_id) == 28,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "instance_counts_shm_id should be 28");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             instance_counts_shm_offset) == 32,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "instance_counts_shm_offset should be 32");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             basevertices_shm_id) == 36,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "basevertices_shm_id should be 36");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             basevertices_shm_offset) == 40,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "basevertices_shm_offset should be 40");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             baseinstances_shm_id) == 44,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "baseinstances_shm_id should be 44");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             baseinstances_shm_offset) == 48,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "baseinstances_shm_offset should be 48");
static_assert(
    offsetof(MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM,
             drawcount) == 52,
    "offset of MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM "
    "drawcount should be 52");

struct StencilFunc {
  typedef StencilFunc ValueType;
  static const CommandId kCmdId = kStencilFunc;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _func, GLint _ref, GLuint _mask) {
    SetHeader();
    func = _func;
    ref = _ref;
    mask = _mask;
  }

  void* Set(void* cmd, GLenum _func, GLint _ref, GLuint _mask) {
    static_cast<ValueType*>(cmd)->Init(_func, _ref, _mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t func;
  int32_t ref;
  uint32_t mask;
};

static_assert(sizeof(StencilFunc) == 16, "size of StencilFunc should be 16");
static_assert(offsetof(StencilFunc, header) == 0,
              "offset of StencilFunc header should be 0");
static_assert(offsetof(StencilFunc, func) == 4,
              "offset of StencilFunc func should be 4");
static_assert(offsetof(StencilFunc, ref) == 8,
              "offset of StencilFunc ref should be 8");
static_assert(offsetof(StencilFunc, mask) == 12,
              "offset of StencilFunc mask should be 12");

struct StencilFuncSeparate {
  typedef StencilFuncSeparate ValueType;
  static const CommandId kCmdId = kStencilFuncSeparate;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _face, GLenum _func, GLint _ref, GLuint _mask) {
    SetHeader();
    face = _face;
    func = _func;
    ref = _ref;
    mask = _mask;
  }

  void* Set(void* cmd, GLenum _face, GLenum _func, GLint _ref, GLuint _mask) {
    static_cast<ValueType*>(cmd)->Init(_face, _func, _ref, _mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t face;
  uint32_t func;
  int32_t ref;
  uint32_t mask;
};

static_assert(sizeof(StencilFuncSeparate) == 20,
              "size of StencilFuncSeparate should be 20");
static_assert(offsetof(StencilFuncSeparate, header) == 0,
              "offset of StencilFuncSeparate header should be 0");
static_assert(offsetof(StencilFuncSeparate, face) == 4,
              "offset of StencilFuncSeparate face should be 4");
static_assert(offsetof(StencilFuncSeparate, func) == 8,
              "offset of StencilFuncSeparate func should be 8");
static_assert(offsetof(StencilFuncSeparate, ref) == 12,
              "offset of StencilFuncSeparate ref should be 12");
static_assert(offsetof(StencilFuncSeparate, mask) == 16,
              "offset of StencilFuncSeparate mask should be 16");

struct StencilMask {
  typedef StencilMask ValueType;
  static const CommandId kCmdId = kStencilMask;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _mask) {
    SetHeader();
    mask = _mask;
  }

  void* Set(void* cmd, GLuint _mask) {
    static_cast<ValueType*>(cmd)->Init(_mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mask;
};

static_assert(sizeof(StencilMask) == 8, "size of StencilMask should be 8");
static_assert(offsetof(StencilMask, header) == 0,
              "offset of StencilMask header should be 0");
static_assert(offsetof(StencilMask, mask) == 4,
              "offset of StencilMask mask should be 4");

struct StencilMaskSeparate {
  typedef StencilMaskSeparate ValueType;
  static const CommandId kCmdId = kStencilMaskSeparate;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _face, GLuint _mask) {
    SetHeader();
    face = _face;
    mask = _mask;
  }

  void* Set(void* cmd, GLenum _face, GLuint _mask) {
    static_cast<ValueType*>(cmd)->Init(_face, _mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t face;
  uint32_t mask;
};

static_assert(sizeof(StencilMaskSeparate) == 12,
              "size of StencilMaskSeparate should be 12");
static_assert(offsetof(StencilMaskSeparate, header) == 0,
              "offset of StencilMaskSeparate header should be 0");
static_assert(offsetof(StencilMaskSeparate, face) == 4,
              "offset of StencilMaskSeparate face should be 4");
static_assert(offsetof(StencilMaskSeparate, mask) == 8,
              "offset of StencilMaskSeparate mask should be 8");

struct StencilOp {
  typedef StencilOp ValueType;
  static const CommandId kCmdId = kStencilOp;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _fail, GLenum _zfail, GLenum _zpass) {
    SetHeader();
    fail = _fail;
    zfail = _zfail;
    zpass = _zpass;
  }

  void* Set(void* cmd, GLenum _fail, GLenum _zfail, GLenum _zpass) {
    static_cast<ValueType*>(cmd)->Init(_fail, _zfail, _zpass);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t fail;
  uint32_t zfail;
  uint32_t zpass;
};

static_assert(sizeof(StencilOp) == 16, "size of StencilOp should be 16");
static_assert(offsetof(StencilOp, header) == 0,
              "offset of StencilOp header should be 0");
static_assert(offsetof(StencilOp, fail) == 4,
              "offset of StencilOp fail should be 4");
static_assert(offsetof(StencilOp, zfail) == 8,
              "offset of StencilOp zfail should be 8");
static_assert(offsetof(StencilOp, zpass) == 12,
              "offset of StencilOp zpass should be 12");

struct StencilOpSeparate {
  typedef StencilOpSeparate ValueType;
  static const CommandId kCmdId = kStencilOpSeparate;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _face, GLenum _fail, GLenum _zfail, GLenum _zpass) {
    SetHeader();
    face = _face;
    fail = _fail;
    zfail = _zfail;
    zpass = _zpass;
  }

  void* Set(void* cmd,
            GLenum _face,
            GLenum _fail,
            GLenum _zfail,
            GLenum _zpass) {
    static_cast<ValueType*>(cmd)->Init(_face, _fail, _zfail, _zpass);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t face;
  uint32_t fail;
  uint32_t zfail;
  uint32_t zpass;
};

static_assert(sizeof(StencilOpSeparate) == 20,
              "size of StencilOpSeparate should be 20");
static_assert(offsetof(StencilOpSeparate, header) == 0,
              "offset of StencilOpSeparate header should be 0");
static_assert(offsetof(StencilOpSeparate, face) == 4,
              "offset of StencilOpSeparate face should be 4");
static_assert(offsetof(StencilOpSeparate, fail) == 8,
              "offset of StencilOpSeparate fail should be 8");
static_assert(offsetof(StencilOpSeparate, zfail) == 12,
              "offset of StencilOpSeparate zfail should be 12");
static_assert(offsetof(StencilOpSeparate, zpass) == 16,
              "offset of StencilOpSeparate zpass should be 16");

struct TexImage2D {
  typedef TexImage2D ValueType;
  static const CommandId kCmdId = kTexImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    format = _format;
    type = _type;
    pixels_shm_id = _pixels_shm_id;
    pixels_shm_offset = _pixels_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _format, _type, _pixels_shm_id,
                                       _pixels_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t internalformat;
  int32_t width;
  int32_t height;
  uint32_t format;
  uint32_t type;
  uint32_t pixels_shm_id;
  uint32_t pixels_shm_offset;
  static const int32_t border = 0;
};

static_assert(sizeof(TexImage2D) == 40, "size of TexImage2D should be 40");
static_assert(offsetof(TexImage2D, header) == 0,
              "offset of TexImage2D header should be 0");
static_assert(offsetof(TexImage2D, target) == 4,
              "offset of TexImage2D target should be 4");
static_assert(offsetof(TexImage2D, level) == 8,
              "offset of TexImage2D level should be 8");
static_assert(offsetof(TexImage2D, internalformat) == 12,
              "offset of TexImage2D internalformat should be 12");
static_assert(offsetof(TexImage2D, width) == 16,
              "offset of TexImage2D width should be 16");
static_assert(offsetof(TexImage2D, height) == 20,
              "offset of TexImage2D height should be 20");
static_assert(offsetof(TexImage2D, format) == 24,
              "offset of TexImage2D format should be 24");
static_assert(offsetof(TexImage2D, type) == 28,
              "offset of TexImage2D type should be 28");
static_assert(offsetof(TexImage2D, pixels_shm_id) == 32,
              "offset of TexImage2D pixels_shm_id should be 32");
static_assert(offsetof(TexImage2D, pixels_shm_offset) == 36,
              "offset of TexImage2D pixels_shm_offset should be 36");

struct TexImage3D {
  typedef TexImage3D ValueType;
  static const CommandId kCmdId = kTexImage3D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    depth = _depth;
    format = _format;
    type = _type;
    pixels_shm_id = _pixels_shm_id;
    pixels_shm_offset = _pixels_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _depth, _format, _type,
                                       _pixels_shm_id, _pixels_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t internalformat;
  int32_t width;
  int32_t height;
  int32_t depth;
  uint32_t format;
  uint32_t type;
  uint32_t pixels_shm_id;
  uint32_t pixels_shm_offset;
  static const int32_t border = 0;
};

static_assert(sizeof(TexImage3D) == 44, "size of TexImage3D should be 44");
static_assert(offsetof(TexImage3D, header) == 0,
              "offset of TexImage3D header should be 0");
static_assert(offsetof(TexImage3D, target) == 4,
              "offset of TexImage3D target should be 4");
static_assert(offsetof(TexImage3D, level) == 8,
              "offset of TexImage3D level should be 8");
static_assert(offsetof(TexImage3D, internalformat) == 12,
              "offset of TexImage3D internalformat should be 12");
static_assert(offsetof(TexImage3D, width) == 16,
              "offset of TexImage3D width should be 16");
static_assert(offsetof(TexImage3D, height) == 20,
              "offset of TexImage3D height should be 20");
static_assert(offsetof(TexImage3D, depth) == 24,
              "offset of TexImage3D depth should be 24");
static_assert(offsetof(TexImage3D, format) == 28,
              "offset of TexImage3D format should be 28");
static_assert(offsetof(TexImage3D, type) == 32,
              "offset of TexImage3D type should be 32");
static_assert(offsetof(TexImage3D, pixels_shm_id) == 36,
              "offset of TexImage3D pixels_shm_id should be 36");
static_assert(offsetof(TexImage3D, pixels_shm_offset) == 40,
              "offset of TexImage3D pixels_shm_offset should be 40");

struct TexParameterf {
  typedef TexParameterf ValueType;
  static const CommandId kCmdId = kTexParameterf;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLenum _pname, GLfloat _param) {
    SetHeader();
    target = _target;
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLenum _target, GLenum _pname, GLfloat _param) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  float param;
};

static_assert(sizeof(TexParameterf) == 16,
              "size of TexParameterf should be 16");
static_assert(offsetof(TexParameterf, header) == 0,
              "offset of TexParameterf header should be 0");
static_assert(offsetof(TexParameterf, target) == 4,
              "offset of TexParameterf target should be 4");
static_assert(offsetof(TexParameterf, pname) == 8,
              "offset of TexParameterf pname should be 8");
static_assert(offsetof(TexParameterf, param) == 12,
              "offset of TexParameterf param should be 12");

struct TexParameterfvImmediate {
  typedef TexParameterfvImmediate ValueType;
  static const CommandId kCmdId = kTexParameterfvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 1);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLenum _target, GLenum _pname, const GLfloat* _params) {
    SetHeader();
    target = _target;
    pname = _pname;
    memcpy(ImmediateDataAddress(this), _params, ComputeDataSize());
  }

  void* Set(void* cmd, GLenum _target, GLenum _pname, const GLfloat* _params) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _params);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
};

static_assert(sizeof(TexParameterfvImmediate) == 12,
              "size of TexParameterfvImmediate should be 12");
static_assert(offsetof(TexParameterfvImmediate, header) == 0,
              "offset of TexParameterfvImmediate header should be 0");
static_assert(offsetof(TexParameterfvImmediate, target) == 4,
              "offset of TexParameterfvImmediate target should be 4");
static_assert(offsetof(TexParameterfvImmediate, pname) == 8,
              "offset of TexParameterfvImmediate pname should be 8");

struct TexParameteri {
  typedef TexParameteri ValueType;
  static const CommandId kCmdId = kTexParameteri;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLenum _pname, GLint _param) {
    SetHeader();
    target = _target;
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLenum _target, GLenum _pname, GLint _param) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  int32_t param;
};

static_assert(sizeof(TexParameteri) == 16,
              "size of TexParameteri should be 16");
static_assert(offsetof(TexParameteri, header) == 0,
              "offset of TexParameteri header should be 0");
static_assert(offsetof(TexParameteri, target) == 4,
              "offset of TexParameteri target should be 4");
static_assert(offsetof(TexParameteri, pname) == 8,
              "offset of TexParameteri pname should be 8");
static_assert(offsetof(TexParameteri, param) == 12,
              "offset of TexParameteri param should be 12");

struct TexParameterivImmediate {
  typedef TexParameterivImmediate ValueType;
  static const CommandId kCmdId = kTexParameterivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLint) * 1);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLenum _target, GLenum _pname, const GLint* _params) {
    SetHeader();
    target = _target;
    pname = _pname;
    memcpy(ImmediateDataAddress(this), _params, ComputeDataSize());
  }

  void* Set(void* cmd, GLenum _target, GLenum _pname, const GLint* _params) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _params);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
};

static_assert(sizeof(TexParameterivImmediate) == 12,
              "size of TexParameterivImmediate should be 12");
static_assert(offsetof(TexParameterivImmediate, header) == 0,
              "offset of TexParameterivImmediate header should be 0");
static_assert(offsetof(TexParameterivImmediate, target) == 4,
              "offset of TexParameterivImmediate target should be 4");
static_assert(offsetof(TexParameterivImmediate, pname) == 8,
              "offset of TexParameterivImmediate pname should be 8");

struct TexStorage3D {
  typedef TexStorage3D ValueType;
  static const CommandId kCmdId = kTexStorage3D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLsizei _levels,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth) {
    SetHeader();
    target = _target;
    levels = _levels;
    internalFormat = _internalFormat;
    width = _width;
    height = _height;
    depth = _depth;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _levels,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth) {
    static_cast<ValueType*>(cmd)->Init(_target, _levels, _internalFormat,
                                       _width, _height, _depth);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t levels;
  uint32_t internalFormat;
  int32_t width;
  int32_t height;
  int32_t depth;
};

static_assert(sizeof(TexStorage3D) == 28, "size of TexStorage3D should be 28");
static_assert(offsetof(TexStorage3D, header) == 0,
              "offset of TexStorage3D header should be 0");
static_assert(offsetof(TexStorage3D, target) == 4,
              "offset of TexStorage3D target should be 4");
static_assert(offsetof(TexStorage3D, levels) == 8,
              "offset of TexStorage3D levels should be 8");
static_assert(offsetof(TexStorage3D, internalFormat) == 12,
              "offset of TexStorage3D internalFormat should be 12");
static_assert(offsetof(TexStorage3D, width) == 16,
              "offset of TexStorage3D width should be 16");
static_assert(offsetof(TexStorage3D, height) == 20,
              "offset of TexStorage3D height should be 20");
static_assert(offsetof(TexStorage3D, depth) == 24,
              "offset of TexStorage3D depth should be 24");

struct TexSubImage2D {
  typedef TexSubImage2D ValueType;
  static const CommandId kCmdId = kTexSubImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            GLboolean _internal) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    width = _width;
    height = _height;
    format = _format;
    type = _type;
    pixels_shm_id = _pixels_shm_id;
    pixels_shm_offset = _pixels_shm_offset;
    internal = _internal;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            GLboolean _internal) {
    static_cast<ValueType*>(cmd)->Init(
        _target, _level, _xoffset, _yoffset, _width, _height, _format, _type,
        _pixels_shm_id, _pixels_shm_offset, _internal);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t width;
  int32_t height;
  uint32_t format;
  uint32_t type;
  uint32_t pixels_shm_id;
  uint32_t pixels_shm_offset;
  uint32_t internal;
};

static_assert(sizeof(TexSubImage2D) == 48,
              "size of TexSubImage2D should be 48");
static_assert(offsetof(TexSubImage2D, header) == 0,
              "offset of TexSubImage2D header should be 0");
static_assert(offsetof(TexSubImage2D, target) == 4,
              "offset of TexSubImage2D target should be 4");
static_assert(offsetof(TexSubImage2D, level) == 8,
              "offset of TexSubImage2D level should be 8");
static_assert(offsetof(TexSubImage2D, xoffset) == 12,
              "offset of TexSubImage2D xoffset should be 12");
static_assert(offsetof(TexSubImage2D, yoffset) == 16,
              "offset of TexSubImage2D yoffset should be 16");
static_assert(offsetof(TexSubImage2D, width) == 20,
              "offset of TexSubImage2D width should be 20");
static_assert(offsetof(TexSubImage2D, height) == 24,
              "offset of TexSubImage2D height should be 24");
static_assert(offsetof(TexSubImage2D, format) == 28,
              "offset of TexSubImage2D format should be 28");
static_assert(offsetof(TexSubImage2D, type) == 32,
              "offset of TexSubImage2D type should be 32");
static_assert(offsetof(TexSubImage2D, pixels_shm_id) == 36,
              "offset of TexSubImage2D pixels_shm_id should be 36");
static_assert(offsetof(TexSubImage2D, pixels_shm_offset) == 40,
              "offset of TexSubImage2D pixels_shm_offset should be 40");
static_assert(offsetof(TexSubImage2D, internal) == 44,
              "offset of TexSubImage2D internal should be 44");

struct TexSubImage3D {
  typedef TexSubImage3D ValueType;
  static const CommandId kCmdId = kTexSubImage3D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            GLboolean _internal) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    zoffset = _zoffset;
    width = _width;
    height = _height;
    depth = _depth;
    format = _format;
    type = _type;
    pixels_shm_id = _pixels_shm_id;
    pixels_shm_offset = _pixels_shm_offset;
    internal = _internal;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _zoffset,
            GLsizei _width,
            GLsizei _height,
            GLsizei _depth,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            GLboolean _internal) {
    static_cast<ValueType*>(cmd)->Init(
        _target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth,
        _format, _type, _pixels_shm_id, _pixels_shm_offset, _internal);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t zoffset;
  int32_t width;
  int32_t height;
  int32_t depth;
  uint32_t format;
  uint32_t type;
  uint32_t pixels_shm_id;
  uint32_t pixels_shm_offset;
  uint32_t internal;
};

static_assert(sizeof(TexSubImage3D) == 56,
              "size of TexSubImage3D should be 56");
static_assert(offsetof(TexSubImage3D, header) == 0,
              "offset of TexSubImage3D header should be 0");
static_assert(offsetof(TexSubImage3D, target) == 4,
              "offset of TexSubImage3D target should be 4");
static_assert(offsetof(TexSubImage3D, level) == 8,
              "offset of TexSubImage3D level should be 8");
static_assert(offsetof(TexSubImage3D, xoffset) == 12,
              "offset of TexSubImage3D xoffset should be 12");
static_assert(offsetof(TexSubImage3D, yoffset) == 16,
              "offset of TexSubImage3D yoffset should be 16");
static_assert(offsetof(TexSubImage3D, zoffset) == 20,
              "offset of TexSubImage3D zoffset should be 20");
static_assert(offsetof(TexSubImage3D, width) == 24,
              "offset of TexSubImage3D width should be 24");
static_assert(offsetof(TexSubImage3D, height) == 28,
              "offset of TexSubImage3D height should be 28");
static_assert(offsetof(TexSubImage3D, depth) == 32,
              "offset of TexSubImage3D depth should be 32");
static_assert(offsetof(TexSubImage3D, format) == 36,
              "offset of TexSubImage3D format should be 36");
static_assert(offsetof(TexSubImage3D, type) == 40,
              "offset of TexSubImage3D type should be 40");
static_assert(offsetof(TexSubImage3D, pixels_shm_id) == 44,
              "offset of TexSubImage3D pixels_shm_id should be 44");
static_assert(offsetof(TexSubImage3D, pixels_shm_offset) == 48,
              "offset of TexSubImage3D pixels_shm_offset should be 48");
static_assert(offsetof(TexSubImage3D, internal) == 52,
              "offset of TexSubImage3D internal should be 52");

struct TransformFeedbackVaryingsBucket {
  typedef TransformFeedbackVaryingsBucket ValueType;
  static const CommandId kCmdId = kTransformFeedbackVaryingsBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, uint32_t _varyings_bucket_id, GLenum _buffermode) {
    SetHeader();
    program = _program;
    varyings_bucket_id = _varyings_bucket_id;
    buffermode = _buffermode;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _varyings_bucket_id,
            GLenum _buffermode) {
    static_cast<ValueType*>(cmd)->Init(_program, _varyings_bucket_id,
                                       _buffermode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t varyings_bucket_id;
  uint32_t buffermode;
};

static_assert(sizeof(TransformFeedbackVaryingsBucket) == 16,
              "size of TransformFeedbackVaryingsBucket should be 16");
static_assert(offsetof(TransformFeedbackVaryingsBucket, header) == 0,
              "offset of TransformFeedbackVaryingsBucket header should be 0");
static_assert(offsetof(TransformFeedbackVaryingsBucket, program) == 4,
              "offset of TransformFeedbackVaryingsBucket program should be 4");
static_assert(
    offsetof(TransformFeedbackVaryingsBucket, varyings_bucket_id) == 8,
    "offset of TransformFeedbackVaryingsBucket varyings_bucket_id should be 8");
static_assert(
    offsetof(TransformFeedbackVaryingsBucket, buffermode) == 12,
    "offset of TransformFeedbackVaryingsBucket buffermode should be 12");

struct Uniform1f {
  typedef Uniform1f ValueType;
  static const CommandId kCmdId = kUniform1f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLfloat _x) {
    SetHeader();
    location = _location;
    x = _x;
  }

  void* Set(void* cmd, GLint _location, GLfloat _x) {
    static_cast<ValueType*>(cmd)->Init(_location, _x);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  float x;
};

static_assert(sizeof(Uniform1f) == 12, "size of Uniform1f should be 12");
static_assert(offsetof(Uniform1f, header) == 0,
              "offset of Uniform1f header should be 0");
static_assert(offsetof(Uniform1f, location) == 4,
              "offset of Uniform1f location should be 4");
static_assert(offsetof(Uniform1f, x) == 8, "offset of Uniform1f x should be 8");

struct Uniform1fvImmediate {
  typedef Uniform1fvImmediate ValueType;
  static const CommandId kCmdId = kUniform1fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLfloat* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLfloat* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform1fvImmediate) == 12,
              "size of Uniform1fvImmediate should be 12");
static_assert(offsetof(Uniform1fvImmediate, header) == 0,
              "offset of Uniform1fvImmediate header should be 0");
static_assert(offsetof(Uniform1fvImmediate, location) == 4,
              "offset of Uniform1fvImmediate location should be 4");
static_assert(offsetof(Uniform1fvImmediate, count) == 8,
              "offset of Uniform1fvImmediate count should be 8");

struct Uniform1i {
  typedef Uniform1i ValueType;
  static const CommandId kCmdId = kUniform1i;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLint _x) {
    SetHeader();
    location = _location;
    x = _x;
  }

  void* Set(void* cmd, GLint _location, GLint _x) {
    static_cast<ValueType*>(cmd)->Init(_location, _x);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t x;
};

static_assert(sizeof(Uniform1i) == 12, "size of Uniform1i should be 12");
static_assert(offsetof(Uniform1i, header) == 0,
              "offset of Uniform1i header should be 0");
static_assert(offsetof(Uniform1i, location) == 4,
              "offset of Uniform1i location should be 4");
static_assert(offsetof(Uniform1i, x) == 8, "offset of Uniform1i x should be 8");

struct Uniform1ivImmediate {
  typedef Uniform1ivImmediate ValueType;
  static const CommandId kCmdId = kUniform1ivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLint) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform1ivImmediate) == 12,
              "size of Uniform1ivImmediate should be 12");
static_assert(offsetof(Uniform1ivImmediate, header) == 0,
              "offset of Uniform1ivImmediate header should be 0");
static_assert(offsetof(Uniform1ivImmediate, location) == 4,
              "offset of Uniform1ivImmediate location should be 4");
static_assert(offsetof(Uniform1ivImmediate, count) == 8,
              "offset of Uniform1ivImmediate count should be 8");

struct Uniform1ui {
  typedef Uniform1ui ValueType;
  static const CommandId kCmdId = kUniform1ui;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLuint _x) {
    SetHeader();
    location = _location;
    x = _x;
  }

  void* Set(void* cmd, GLint _location, GLuint _x) {
    static_cast<ValueType*>(cmd)->Init(_location, _x);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  uint32_t x;
};

static_assert(sizeof(Uniform1ui) == 12, "size of Uniform1ui should be 12");
static_assert(offsetof(Uniform1ui, header) == 0,
              "offset of Uniform1ui header should be 0");
static_assert(offsetof(Uniform1ui, location) == 4,
              "offset of Uniform1ui location should be 4");
static_assert(offsetof(Uniform1ui, x) == 8,
              "offset of Uniform1ui x should be 8");

struct Uniform1uivImmediate {
  typedef Uniform1uivImmediate ValueType;
  static const CommandId kCmdId = kUniform1uivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLuint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLuint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform1uivImmediate) == 12,
              "size of Uniform1uivImmediate should be 12");
static_assert(offsetof(Uniform1uivImmediate, header) == 0,
              "offset of Uniform1uivImmediate header should be 0");
static_assert(offsetof(Uniform1uivImmediate, location) == 4,
              "offset of Uniform1uivImmediate location should be 4");
static_assert(offsetof(Uniform1uivImmediate, count) == 8,
              "offset of Uniform1uivImmediate count should be 8");

struct Uniform2f {
  typedef Uniform2f ValueType;
  static const CommandId kCmdId = kUniform2f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLfloat _x, GLfloat _y) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
  }

  void* Set(void* cmd, GLint _location, GLfloat _x, GLfloat _y) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  float x;
  float y;
};

static_assert(sizeof(Uniform2f) == 16, "size of Uniform2f should be 16");
static_assert(offsetof(Uniform2f, header) == 0,
              "offset of Uniform2f header should be 0");
static_assert(offsetof(Uniform2f, location) == 4,
              "offset of Uniform2f location should be 4");
static_assert(offsetof(Uniform2f, x) == 8, "offset of Uniform2f x should be 8");
static_assert(offsetof(Uniform2f, y) == 12,
              "offset of Uniform2f y should be 12");

struct Uniform2fvImmediate {
  typedef Uniform2fvImmediate ValueType;
  static const CommandId kCmdId = kUniform2fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 2 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLfloat* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLfloat* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform2fvImmediate) == 12,
              "size of Uniform2fvImmediate should be 12");
static_assert(offsetof(Uniform2fvImmediate, header) == 0,
              "offset of Uniform2fvImmediate header should be 0");
static_assert(offsetof(Uniform2fvImmediate, location) == 4,
              "offset of Uniform2fvImmediate location should be 4");
static_assert(offsetof(Uniform2fvImmediate, count) == 8,
              "offset of Uniform2fvImmediate count should be 8");

struct Uniform2i {
  typedef Uniform2i ValueType;
  static const CommandId kCmdId = kUniform2i;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLint _x, GLint _y) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
  }

  void* Set(void* cmd, GLint _location, GLint _x, GLint _y) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t x;
  int32_t y;
};

static_assert(sizeof(Uniform2i) == 16, "size of Uniform2i should be 16");
static_assert(offsetof(Uniform2i, header) == 0,
              "offset of Uniform2i header should be 0");
static_assert(offsetof(Uniform2i, location) == 4,
              "offset of Uniform2i location should be 4");
static_assert(offsetof(Uniform2i, x) == 8, "offset of Uniform2i x should be 8");
static_assert(offsetof(Uniform2i, y) == 12,
              "offset of Uniform2i y should be 12");

struct Uniform2ivImmediate {
  typedef Uniform2ivImmediate ValueType;
  static const CommandId kCmdId = kUniform2ivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLint) * 2 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform2ivImmediate) == 12,
              "size of Uniform2ivImmediate should be 12");
static_assert(offsetof(Uniform2ivImmediate, header) == 0,
              "offset of Uniform2ivImmediate header should be 0");
static_assert(offsetof(Uniform2ivImmediate, location) == 4,
              "offset of Uniform2ivImmediate location should be 4");
static_assert(offsetof(Uniform2ivImmediate, count) == 8,
              "offset of Uniform2ivImmediate count should be 8");

struct Uniform2ui {
  typedef Uniform2ui ValueType;
  static const CommandId kCmdId = kUniform2ui;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLuint _x, GLuint _y) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
  }

  void* Set(void* cmd, GLint _location, GLuint _x, GLuint _y) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  uint32_t x;
  uint32_t y;
};

static_assert(sizeof(Uniform2ui) == 16, "size of Uniform2ui should be 16");
static_assert(offsetof(Uniform2ui, header) == 0,
              "offset of Uniform2ui header should be 0");
static_assert(offsetof(Uniform2ui, location) == 4,
              "offset of Uniform2ui location should be 4");
static_assert(offsetof(Uniform2ui, x) == 8,
              "offset of Uniform2ui x should be 8");
static_assert(offsetof(Uniform2ui, y) == 12,
              "offset of Uniform2ui y should be 12");

struct Uniform2uivImmediate {
  typedef Uniform2uivImmediate ValueType;
  static const CommandId kCmdId = kUniform2uivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * 2 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLuint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLuint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform2uivImmediate) == 12,
              "size of Uniform2uivImmediate should be 12");
static_assert(offsetof(Uniform2uivImmediate, header) == 0,
              "offset of Uniform2uivImmediate header should be 0");
static_assert(offsetof(Uniform2uivImmediate, location) == 4,
              "offset of Uniform2uivImmediate location should be 4");
static_assert(offsetof(Uniform2uivImmediate, count) == 8,
              "offset of Uniform2uivImmediate count should be 8");

struct Uniform3f {
  typedef Uniform3f ValueType;
  static const CommandId kCmdId = kUniform3f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLfloat _x, GLfloat _y, GLfloat _z) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
    z = _z;
  }

  void* Set(void* cmd, GLint _location, GLfloat _x, GLfloat _y, GLfloat _z) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y, _z);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  float x;
  float y;
  float z;
};

static_assert(sizeof(Uniform3f) == 20, "size of Uniform3f should be 20");
static_assert(offsetof(Uniform3f, header) == 0,
              "offset of Uniform3f header should be 0");
static_assert(offsetof(Uniform3f, location) == 4,
              "offset of Uniform3f location should be 4");
static_assert(offsetof(Uniform3f, x) == 8, "offset of Uniform3f x should be 8");
static_assert(offsetof(Uniform3f, y) == 12,
              "offset of Uniform3f y should be 12");
static_assert(offsetof(Uniform3f, z) == 16,
              "offset of Uniform3f z should be 16");

struct Uniform3fvImmediate {
  typedef Uniform3fvImmediate ValueType;
  static const CommandId kCmdId = kUniform3fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 3 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLfloat* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLfloat* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform3fvImmediate) == 12,
              "size of Uniform3fvImmediate should be 12");
static_assert(offsetof(Uniform3fvImmediate, header) == 0,
              "offset of Uniform3fvImmediate header should be 0");
static_assert(offsetof(Uniform3fvImmediate, location) == 4,
              "offset of Uniform3fvImmediate location should be 4");
static_assert(offsetof(Uniform3fvImmediate, count) == 8,
              "offset of Uniform3fvImmediate count should be 8");

struct Uniform3i {
  typedef Uniform3i ValueType;
  static const CommandId kCmdId = kUniform3i;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLint _x, GLint _y, GLint _z) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
    z = _z;
  }

  void* Set(void* cmd, GLint _location, GLint _x, GLint _y, GLint _z) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y, _z);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t x;
  int32_t y;
  int32_t z;
};

static_assert(sizeof(Uniform3i) == 20, "size of Uniform3i should be 20");
static_assert(offsetof(Uniform3i, header) == 0,
              "offset of Uniform3i header should be 0");
static_assert(offsetof(Uniform3i, location) == 4,
              "offset of Uniform3i location should be 4");
static_assert(offsetof(Uniform3i, x) == 8, "offset of Uniform3i x should be 8");
static_assert(offsetof(Uniform3i, y) == 12,
              "offset of Uniform3i y should be 12");
static_assert(offsetof(Uniform3i, z) == 16,
              "offset of Uniform3i z should be 16");

struct Uniform3ivImmediate {
  typedef Uniform3ivImmediate ValueType;
  static const CommandId kCmdId = kUniform3ivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLint) * 3 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform3ivImmediate) == 12,
              "size of Uniform3ivImmediate should be 12");
static_assert(offsetof(Uniform3ivImmediate, header) == 0,
              "offset of Uniform3ivImmediate header should be 0");
static_assert(offsetof(Uniform3ivImmediate, location) == 4,
              "offset of Uniform3ivImmediate location should be 4");
static_assert(offsetof(Uniform3ivImmediate, count) == 8,
              "offset of Uniform3ivImmediate count should be 8");

struct Uniform3ui {
  typedef Uniform3ui ValueType;
  static const CommandId kCmdId = kUniform3ui;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLuint _x, GLuint _y, GLuint _z) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
    z = _z;
  }

  void* Set(void* cmd, GLint _location, GLuint _x, GLuint _y, GLuint _z) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y, _z);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  uint32_t x;
  uint32_t y;
  uint32_t z;
};

static_assert(sizeof(Uniform3ui) == 20, "size of Uniform3ui should be 20");
static_assert(offsetof(Uniform3ui, header) == 0,
              "offset of Uniform3ui header should be 0");
static_assert(offsetof(Uniform3ui, location) == 4,
              "offset of Uniform3ui location should be 4");
static_assert(offsetof(Uniform3ui, x) == 8,
              "offset of Uniform3ui x should be 8");
static_assert(offsetof(Uniform3ui, y) == 12,
              "offset of Uniform3ui y should be 12");
static_assert(offsetof(Uniform3ui, z) == 16,
              "offset of Uniform3ui z should be 16");

struct Uniform3uivImmediate {
  typedef Uniform3uivImmediate ValueType;
  static const CommandId kCmdId = kUniform3uivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * 3 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLuint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLuint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform3uivImmediate) == 12,
              "size of Uniform3uivImmediate should be 12");
static_assert(offsetof(Uniform3uivImmediate, header) == 0,
              "offset of Uniform3uivImmediate header should be 0");
static_assert(offsetof(Uniform3uivImmediate, location) == 4,
              "offset of Uniform3uivImmediate location should be 4");
static_assert(offsetof(Uniform3uivImmediate, count) == 8,
              "offset of Uniform3uivImmediate count should be 8");

struct Uniform4f {
  typedef Uniform4f ValueType;
  static const CommandId kCmdId = kUniform4f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLfloat _x, GLfloat _y, GLfloat _z, GLfloat _w) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }

  void* Set(void* cmd,
            GLint _location,
            GLfloat _x,
            GLfloat _y,
            GLfloat _z,
            GLfloat _w) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y, _z, _w);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  float x;
  float y;
  float z;
  float w;
};

static_assert(sizeof(Uniform4f) == 24, "size of Uniform4f should be 24");
static_assert(offsetof(Uniform4f, header) == 0,
              "offset of Uniform4f header should be 0");
static_assert(offsetof(Uniform4f, location) == 4,
              "offset of Uniform4f location should be 4");
static_assert(offsetof(Uniform4f, x) == 8, "offset of Uniform4f x should be 8");
static_assert(offsetof(Uniform4f, y) == 12,
              "offset of Uniform4f y should be 12");
static_assert(offsetof(Uniform4f, z) == 16,
              "offset of Uniform4f z should be 16");
static_assert(offsetof(Uniform4f, w) == 20,
              "offset of Uniform4f w should be 20");

struct Uniform4fvImmediate {
  typedef Uniform4fvImmediate ValueType;
  static const CommandId kCmdId = kUniform4fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 4 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLfloat* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLfloat* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform4fvImmediate) == 12,
              "size of Uniform4fvImmediate should be 12");
static_assert(offsetof(Uniform4fvImmediate, header) == 0,
              "offset of Uniform4fvImmediate header should be 0");
static_assert(offsetof(Uniform4fvImmediate, location) == 4,
              "offset of Uniform4fvImmediate location should be 4");
static_assert(offsetof(Uniform4fvImmediate, count) == 8,
              "offset of Uniform4fvImmediate count should be 8");

struct Uniform4i {
  typedef Uniform4i ValueType;
  static const CommandId kCmdId = kUniform4i;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLint _x, GLint _y, GLint _z, GLint _w) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }

  void* Set(void* cmd,
            GLint _location,
            GLint _x,
            GLint _y,
            GLint _z,
            GLint _w) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y, _z, _w);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t x;
  int32_t y;
  int32_t z;
  int32_t w;
};

static_assert(sizeof(Uniform4i) == 24, "size of Uniform4i should be 24");
static_assert(offsetof(Uniform4i, header) == 0,
              "offset of Uniform4i header should be 0");
static_assert(offsetof(Uniform4i, location) == 4,
              "offset of Uniform4i location should be 4");
static_assert(offsetof(Uniform4i, x) == 8, "offset of Uniform4i x should be 8");
static_assert(offsetof(Uniform4i, y) == 12,
              "offset of Uniform4i y should be 12");
static_assert(offsetof(Uniform4i, z) == 16,
              "offset of Uniform4i z should be 16");
static_assert(offsetof(Uniform4i, w) == 20,
              "offset of Uniform4i w should be 20");

struct Uniform4ivImmediate {
  typedef Uniform4ivImmediate ValueType;
  static const CommandId kCmdId = kUniform4ivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLint) * 4 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform4ivImmediate) == 12,
              "size of Uniform4ivImmediate should be 12");
static_assert(offsetof(Uniform4ivImmediate, header) == 0,
              "offset of Uniform4ivImmediate header should be 0");
static_assert(offsetof(Uniform4ivImmediate, location) == 4,
              "offset of Uniform4ivImmediate location should be 4");
static_assert(offsetof(Uniform4ivImmediate, count) == 8,
              "offset of Uniform4ivImmediate count should be 8");

struct Uniform4ui {
  typedef Uniform4ui ValueType;
  static const CommandId kCmdId = kUniform4ui;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _location, GLuint _x, GLuint _y, GLuint _z, GLuint _w) {
    SetHeader();
    location = _location;
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }

  void* Set(void* cmd,
            GLint _location,
            GLuint _x,
            GLuint _y,
            GLuint _z,
            GLuint _w) {
    static_cast<ValueType*>(cmd)->Init(_location, _x, _y, _z, _w);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t location;
  uint32_t x;
  uint32_t y;
  uint32_t z;
  uint32_t w;
};

static_assert(sizeof(Uniform4ui) == 24, "size of Uniform4ui should be 24");
static_assert(offsetof(Uniform4ui, header) == 0,
              "offset of Uniform4ui header should be 0");
static_assert(offsetof(Uniform4ui, location) == 4,
              "offset of Uniform4ui location should be 4");
static_assert(offsetof(Uniform4ui, x) == 8,
              "offset of Uniform4ui x should be 8");
static_assert(offsetof(Uniform4ui, y) == 12,
              "offset of Uniform4ui y should be 12");
static_assert(offsetof(Uniform4ui, z) == 16,
              "offset of Uniform4ui z should be 16");
static_assert(offsetof(Uniform4ui, w) == 20,
              "offset of Uniform4ui w should be 20");

struct Uniform4uivImmediate {
  typedef Uniform4uivImmediate ValueType;
  static const CommandId kCmdId = kUniform4uivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * 4 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location, GLsizei _count, const GLuint* _v) {
    SetHeader(_count);
    location = _location;
    count = _count;
    memcpy(ImmediateDataAddress(this), _v, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLint _location, GLsizei _count, const GLuint* _v) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _v);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
};

static_assert(sizeof(Uniform4uivImmediate) == 12,
              "size of Uniform4uivImmediate should be 12");
static_assert(offsetof(Uniform4uivImmediate, header) == 0,
              "offset of Uniform4uivImmediate header should be 0");
static_assert(offsetof(Uniform4uivImmediate, location) == 4,
              "offset of Uniform4uivImmediate location should be 4");
static_assert(offsetof(Uniform4uivImmediate, count) == 8,
              "offset of Uniform4uivImmediate count should be 8");

struct UniformBlockBinding {
  typedef UniformBlockBinding ValueType;
  static const CommandId kCmdId = kUniformBlockBinding;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, GLuint _index, GLuint _binding) {
    SetHeader();
    program = _program;
    index = _index;
    binding = _binding;
  }

  void* Set(void* cmd, GLuint _program, GLuint _index, GLuint _binding) {
    static_cast<ValueType*>(cmd)->Init(_program, _index, _binding);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t index;
  uint32_t binding;
};

static_assert(sizeof(UniformBlockBinding) == 16,
              "size of UniformBlockBinding should be 16");
static_assert(offsetof(UniformBlockBinding, header) == 0,
              "offset of UniformBlockBinding header should be 0");
static_assert(offsetof(UniformBlockBinding, program) == 4,
              "offset of UniformBlockBinding program should be 4");
static_assert(offsetof(UniformBlockBinding, index) == 8,
              "offset of UniformBlockBinding index should be 8");
static_assert(offsetof(UniformBlockBinding, binding) == 12,
              "offset of UniformBlockBinding binding should be 12");

struct UniformMatrix2fvImmediate {
  typedef UniformMatrix2fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix2fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 4 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix2fvImmediate) == 16,
              "size of UniformMatrix2fvImmediate should be 16");
static_assert(offsetof(UniformMatrix2fvImmediate, header) == 0,
              "offset of UniformMatrix2fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix2fvImmediate, location) == 4,
              "offset of UniformMatrix2fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix2fvImmediate, count) == 8,
              "offset of UniformMatrix2fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix2fvImmediate, transpose) == 12,
              "offset of UniformMatrix2fvImmediate transpose should be 12");

struct UniformMatrix2x3fvImmediate {
  typedef UniformMatrix2x3fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix2x3fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 6 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix2x3fvImmediate) == 16,
              "size of UniformMatrix2x3fvImmediate should be 16");
static_assert(offsetof(UniformMatrix2x3fvImmediate, header) == 0,
              "offset of UniformMatrix2x3fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix2x3fvImmediate, location) == 4,
              "offset of UniformMatrix2x3fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix2x3fvImmediate, count) == 8,
              "offset of UniformMatrix2x3fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix2x3fvImmediate, transpose) == 12,
              "offset of UniformMatrix2x3fvImmediate transpose should be 12");

struct UniformMatrix2x4fvImmediate {
  typedef UniformMatrix2x4fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix2x4fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 8 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix2x4fvImmediate) == 16,
              "size of UniformMatrix2x4fvImmediate should be 16");
static_assert(offsetof(UniformMatrix2x4fvImmediate, header) == 0,
              "offset of UniformMatrix2x4fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix2x4fvImmediate, location) == 4,
              "offset of UniformMatrix2x4fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix2x4fvImmediate, count) == 8,
              "offset of UniformMatrix2x4fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix2x4fvImmediate, transpose) == 12,
              "offset of UniformMatrix2x4fvImmediate transpose should be 12");

struct UniformMatrix3fvImmediate {
  typedef UniformMatrix3fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix3fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 9 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix3fvImmediate) == 16,
              "size of UniformMatrix3fvImmediate should be 16");
static_assert(offsetof(UniformMatrix3fvImmediate, header) == 0,
              "offset of UniformMatrix3fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix3fvImmediate, location) == 4,
              "offset of UniformMatrix3fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix3fvImmediate, count) == 8,
              "offset of UniformMatrix3fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix3fvImmediate, transpose) == 12,
              "offset of UniformMatrix3fvImmediate transpose should be 12");

struct UniformMatrix3x2fvImmediate {
  typedef UniformMatrix3x2fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix3x2fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 6 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix3x2fvImmediate) == 16,
              "size of UniformMatrix3x2fvImmediate should be 16");
static_assert(offsetof(UniformMatrix3x2fvImmediate, header) == 0,
              "offset of UniformMatrix3x2fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix3x2fvImmediate, location) == 4,
              "offset of UniformMatrix3x2fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix3x2fvImmediate, count) == 8,
              "offset of UniformMatrix3x2fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix3x2fvImmediate, transpose) == 12,
              "offset of UniformMatrix3x2fvImmediate transpose should be 12");

struct UniformMatrix3x4fvImmediate {
  typedef UniformMatrix3x4fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix3x4fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 12 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix3x4fvImmediate) == 16,
              "size of UniformMatrix3x4fvImmediate should be 16");
static_assert(offsetof(UniformMatrix3x4fvImmediate, header) == 0,
              "offset of UniformMatrix3x4fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix3x4fvImmediate, location) == 4,
              "offset of UniformMatrix3x4fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix3x4fvImmediate, count) == 8,
              "offset of UniformMatrix3x4fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix3x4fvImmediate, transpose) == 12,
              "offset of UniformMatrix3x4fvImmediate transpose should be 12");

struct UniformMatrix4fvImmediate {
  typedef UniformMatrix4fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix4fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 16 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix4fvImmediate) == 16,
              "size of UniformMatrix4fvImmediate should be 16");
static_assert(offsetof(UniformMatrix4fvImmediate, header) == 0,
              "offset of UniformMatrix4fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix4fvImmediate, location) == 4,
              "offset of UniformMatrix4fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix4fvImmediate, count) == 8,
              "offset of UniformMatrix4fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix4fvImmediate, transpose) == 12,
              "offset of UniformMatrix4fvImmediate transpose should be 12");

struct UniformMatrix4x2fvImmediate {
  typedef UniformMatrix4x2fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix4x2fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 8 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix4x2fvImmediate) == 16,
              "size of UniformMatrix4x2fvImmediate should be 16");
static_assert(offsetof(UniformMatrix4x2fvImmediate, header) == 0,
              "offset of UniformMatrix4x2fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix4x2fvImmediate, location) == 4,
              "offset of UniformMatrix4x2fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix4x2fvImmediate, count) == 8,
              "offset of UniformMatrix4x2fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix4x2fvImmediate, transpose) == 12,
              "offset of UniformMatrix4x2fvImmediate transpose should be 12");

struct UniformMatrix4x3fvImmediate {
  typedef UniformMatrix4x3fvImmediate ValueType;
  static const CommandId kCmdId = kUniformMatrix4x3fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLfloat) * 12 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    SetHeader(_count);
    location = _location;
    count = _count;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _value, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLint _location,
            GLsizei _count,
            GLboolean _transpose,
            const GLfloat* _value) {
    static_cast<ValueType*>(cmd)->Init(_location, _count, _transpose, _value);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  int32_t count;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix4x3fvImmediate) == 16,
              "size of UniformMatrix4x3fvImmediate should be 16");
static_assert(offsetof(UniformMatrix4x3fvImmediate, header) == 0,
              "offset of UniformMatrix4x3fvImmediate header should be 0");
static_assert(offsetof(UniformMatrix4x3fvImmediate, location) == 4,
              "offset of UniformMatrix4x3fvImmediate location should be 4");
static_assert(offsetof(UniformMatrix4x3fvImmediate, count) == 8,
              "offset of UniformMatrix4x3fvImmediate count should be 8");
static_assert(offsetof(UniformMatrix4x3fvImmediate, transpose) == 12,
              "offset of UniformMatrix4x3fvImmediate transpose should be 12");

struct UseProgram {
  typedef UseProgram ValueType;
  static const CommandId kCmdId = kUseProgram;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program) {
    SetHeader();
    program = _program;
  }

  void* Set(void* cmd, GLuint _program) {
    static_cast<ValueType*>(cmd)->Init(_program);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
};

static_assert(sizeof(UseProgram) == 8, "size of UseProgram should be 8");
static_assert(offsetof(UseProgram, header) == 0,
              "offset of UseProgram header should be 0");
static_assert(offsetof(UseProgram, program) == 4,
              "offset of UseProgram program should be 4");

struct ValidateProgram {
  typedef ValidateProgram ValueType;
  static const CommandId kCmdId = kValidateProgram;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program) {
    SetHeader();
    program = _program;
  }

  void* Set(void* cmd, GLuint _program) {
    static_cast<ValueType*>(cmd)->Init(_program);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
};

static_assert(sizeof(ValidateProgram) == 8,
              "size of ValidateProgram should be 8");
static_assert(offsetof(ValidateProgram, header) == 0,
              "offset of ValidateProgram header should be 0");
static_assert(offsetof(ValidateProgram, program) == 4,
              "offset of ValidateProgram program should be 4");

struct VertexAttrib1f {
  typedef VertexAttrib1f ValueType;
  static const CommandId kCmdId = kVertexAttrib1f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx, GLfloat _x) {
    SetHeader();
    indx = _indx;
    x = _x;
  }

  void* Set(void* cmd, GLuint _indx, GLfloat _x) {
    static_cast<ValueType*>(cmd)->Init(_indx, _x);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  float x;
};

static_assert(sizeof(VertexAttrib1f) == 12,
              "size of VertexAttrib1f should be 12");
static_assert(offsetof(VertexAttrib1f, header) == 0,
              "offset of VertexAttrib1f header should be 0");
static_assert(offsetof(VertexAttrib1f, indx) == 4,
              "offset of VertexAttrib1f indx should be 4");
static_assert(offsetof(VertexAttrib1f, x) == 8,
              "offset of VertexAttrib1f x should be 8");

struct VertexAttrib1fvImmediate {
  typedef VertexAttrib1fvImmediate ValueType;
  static const CommandId kCmdId = kVertexAttrib1fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 1);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _indx, const GLfloat* _values) {
    SetHeader();
    indx = _indx;
    memcpy(ImmediateDataAddress(this), _values, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _indx, const GLfloat* _values) {
    static_cast<ValueType*>(cmd)->Init(_indx, _values);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t indx;
};

static_assert(sizeof(VertexAttrib1fvImmediate) == 8,
              "size of VertexAttrib1fvImmediate should be 8");
static_assert(offsetof(VertexAttrib1fvImmediate, header) == 0,
              "offset of VertexAttrib1fvImmediate header should be 0");
static_assert(offsetof(VertexAttrib1fvImmediate, indx) == 4,
              "offset of VertexAttrib1fvImmediate indx should be 4");

struct VertexAttrib2f {
  typedef VertexAttrib2f ValueType;
  static const CommandId kCmdId = kVertexAttrib2f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx, GLfloat _x, GLfloat _y) {
    SetHeader();
    indx = _indx;
    x = _x;
    y = _y;
  }

  void* Set(void* cmd, GLuint _indx, GLfloat _x, GLfloat _y) {
    static_cast<ValueType*>(cmd)->Init(_indx, _x, _y);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  float x;
  float y;
};

static_assert(sizeof(VertexAttrib2f) == 16,
              "size of VertexAttrib2f should be 16");
static_assert(offsetof(VertexAttrib2f, header) == 0,
              "offset of VertexAttrib2f header should be 0");
static_assert(offsetof(VertexAttrib2f, indx) == 4,
              "offset of VertexAttrib2f indx should be 4");
static_assert(offsetof(VertexAttrib2f, x) == 8,
              "offset of VertexAttrib2f x should be 8");
static_assert(offsetof(VertexAttrib2f, y) == 12,
              "offset of VertexAttrib2f y should be 12");

struct VertexAttrib2fvImmediate {
  typedef VertexAttrib2fvImmediate ValueType;
  static const CommandId kCmdId = kVertexAttrib2fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 2);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _indx, const GLfloat* _values) {
    SetHeader();
    indx = _indx;
    memcpy(ImmediateDataAddress(this), _values, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _indx, const GLfloat* _values) {
    static_cast<ValueType*>(cmd)->Init(_indx, _values);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t indx;
};

static_assert(sizeof(VertexAttrib2fvImmediate) == 8,
              "size of VertexAttrib2fvImmediate should be 8");
static_assert(offsetof(VertexAttrib2fvImmediate, header) == 0,
              "offset of VertexAttrib2fvImmediate header should be 0");
static_assert(offsetof(VertexAttrib2fvImmediate, indx) == 4,
              "offset of VertexAttrib2fvImmediate indx should be 4");

struct VertexAttrib3f {
  typedef VertexAttrib3f ValueType;
  static const CommandId kCmdId = kVertexAttrib3f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx, GLfloat _x, GLfloat _y, GLfloat _z) {
    SetHeader();
    indx = _indx;
    x = _x;
    y = _y;
    z = _z;
  }

  void* Set(void* cmd, GLuint _indx, GLfloat _x, GLfloat _y, GLfloat _z) {
    static_cast<ValueType*>(cmd)->Init(_indx, _x, _y, _z);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  float x;
  float y;
  float z;
};

static_assert(sizeof(VertexAttrib3f) == 20,
              "size of VertexAttrib3f should be 20");
static_assert(offsetof(VertexAttrib3f, header) == 0,
              "offset of VertexAttrib3f header should be 0");
static_assert(offsetof(VertexAttrib3f, indx) == 4,
              "offset of VertexAttrib3f indx should be 4");
static_assert(offsetof(VertexAttrib3f, x) == 8,
              "offset of VertexAttrib3f x should be 8");
static_assert(offsetof(VertexAttrib3f, y) == 12,
              "offset of VertexAttrib3f y should be 12");
static_assert(offsetof(VertexAttrib3f, z) == 16,
              "offset of VertexAttrib3f z should be 16");

struct VertexAttrib3fvImmediate {
  typedef VertexAttrib3fvImmediate ValueType;
  static const CommandId kCmdId = kVertexAttrib3fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 3);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _indx, const GLfloat* _values) {
    SetHeader();
    indx = _indx;
    memcpy(ImmediateDataAddress(this), _values, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _indx, const GLfloat* _values) {
    static_cast<ValueType*>(cmd)->Init(_indx, _values);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t indx;
};

static_assert(sizeof(VertexAttrib3fvImmediate) == 8,
              "size of VertexAttrib3fvImmediate should be 8");
static_assert(offsetof(VertexAttrib3fvImmediate, header) == 0,
              "offset of VertexAttrib3fvImmediate header should be 0");
static_assert(offsetof(VertexAttrib3fvImmediate, indx) == 4,
              "offset of VertexAttrib3fvImmediate indx should be 4");

struct VertexAttrib4f {
  typedef VertexAttrib4f ValueType;
  static const CommandId kCmdId = kVertexAttrib4f;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx, GLfloat _x, GLfloat _y, GLfloat _z, GLfloat _w) {
    SetHeader();
    indx = _indx;
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }

  void* Set(void* cmd,
            GLuint _indx,
            GLfloat _x,
            GLfloat _y,
            GLfloat _z,
            GLfloat _w) {
    static_cast<ValueType*>(cmd)->Init(_indx, _x, _y, _z, _w);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  float x;
  float y;
  float z;
  float w;
};

static_assert(sizeof(VertexAttrib4f) == 24,
              "size of VertexAttrib4f should be 24");
static_assert(offsetof(VertexAttrib4f, header) == 0,
              "offset of VertexAttrib4f header should be 0");
static_assert(offsetof(VertexAttrib4f, indx) == 4,
              "offset of VertexAttrib4f indx should be 4");
static_assert(offsetof(VertexAttrib4f, x) == 8,
              "offset of VertexAttrib4f x should be 8");
static_assert(offsetof(VertexAttrib4f, y) == 12,
              "offset of VertexAttrib4f y should be 12");
static_assert(offsetof(VertexAttrib4f, z) == 16,
              "offset of VertexAttrib4f z should be 16");
static_assert(offsetof(VertexAttrib4f, w) == 20,
              "offset of VertexAttrib4f w should be 20");

struct VertexAttrib4fvImmediate {
  typedef VertexAttrib4fvImmediate ValueType;
  static const CommandId kCmdId = kVertexAttrib4fvImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 4);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _indx, const GLfloat* _values) {
    SetHeader();
    indx = _indx;
    memcpy(ImmediateDataAddress(this), _values, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _indx, const GLfloat* _values) {
    static_cast<ValueType*>(cmd)->Init(_indx, _values);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t indx;
};

static_assert(sizeof(VertexAttrib4fvImmediate) == 8,
              "size of VertexAttrib4fvImmediate should be 8");
static_assert(offsetof(VertexAttrib4fvImmediate, header) == 0,
              "offset of VertexAttrib4fvImmediate header should be 0");
static_assert(offsetof(VertexAttrib4fvImmediate, indx) == 4,
              "offset of VertexAttrib4fvImmediate indx should be 4");

struct VertexAttribI4i {
  typedef VertexAttribI4i ValueType;
  static const CommandId kCmdId = kVertexAttribI4i;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx, GLint _x, GLint _y, GLint _z, GLint _w) {
    SetHeader();
    indx = _indx;
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }

  void* Set(void* cmd, GLuint _indx, GLint _x, GLint _y, GLint _z, GLint _w) {
    static_cast<ValueType*>(cmd)->Init(_indx, _x, _y, _z, _w);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  int32_t x;
  int32_t y;
  int32_t z;
  int32_t w;
};

static_assert(sizeof(VertexAttribI4i) == 24,
              "size of VertexAttribI4i should be 24");
static_assert(offsetof(VertexAttribI4i, header) == 0,
              "offset of VertexAttribI4i header should be 0");
static_assert(offsetof(VertexAttribI4i, indx) == 4,
              "offset of VertexAttribI4i indx should be 4");
static_assert(offsetof(VertexAttribI4i, x) == 8,
              "offset of VertexAttribI4i x should be 8");
static_assert(offsetof(VertexAttribI4i, y) == 12,
              "offset of VertexAttribI4i y should be 12");
static_assert(offsetof(VertexAttribI4i, z) == 16,
              "offset of VertexAttribI4i z should be 16");
static_assert(offsetof(VertexAttribI4i, w) == 20,
              "offset of VertexAttribI4i w should be 20");

struct VertexAttribI4ivImmediate {
  typedef VertexAttribI4ivImmediate ValueType;
  static const CommandId kCmdId = kVertexAttribI4ivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLint) * 4);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _indx, const GLint* _values) {
    SetHeader();
    indx = _indx;
    memcpy(ImmediateDataAddress(this), _values, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _indx, const GLint* _values) {
    static_cast<ValueType*>(cmd)->Init(_indx, _values);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t indx;
};

static_assert(sizeof(VertexAttribI4ivImmediate) == 8,
              "size of VertexAttribI4ivImmediate should be 8");
static_assert(offsetof(VertexAttribI4ivImmediate, header) == 0,
              "offset of VertexAttribI4ivImmediate header should be 0");
static_assert(offsetof(VertexAttribI4ivImmediate, indx) == 4,
              "offset of VertexAttribI4ivImmediate indx should be 4");

struct VertexAttribI4ui {
  typedef VertexAttribI4ui ValueType;
  static const CommandId kCmdId = kVertexAttribI4ui;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx, GLuint _x, GLuint _y, GLuint _z, GLuint _w) {
    SetHeader();
    indx = _indx;
    x = _x;
    y = _y;
    z = _z;
    w = _w;
  }

  void* Set(void* cmd,
            GLuint _indx,
            GLuint _x,
            GLuint _y,
            GLuint _z,
            GLuint _w) {
    static_cast<ValueType*>(cmd)->Init(_indx, _x, _y, _z, _w);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  uint32_t x;
  uint32_t y;
  uint32_t z;
  uint32_t w;
};

static_assert(sizeof(VertexAttribI4ui) == 24,
              "size of VertexAttribI4ui should be 24");
static_assert(offsetof(VertexAttribI4ui, header) == 0,
              "offset of VertexAttribI4ui header should be 0");
static_assert(offsetof(VertexAttribI4ui, indx) == 4,
              "offset of VertexAttribI4ui indx should be 4");
static_assert(offsetof(VertexAttribI4ui, x) == 8,
              "offset of VertexAttribI4ui x should be 8");
static_assert(offsetof(VertexAttribI4ui, y) == 12,
              "offset of VertexAttribI4ui y should be 12");
static_assert(offsetof(VertexAttribI4ui, z) == 16,
              "offset of VertexAttribI4ui z should be 16");
static_assert(offsetof(VertexAttribI4ui, w) == 20,
              "offset of VertexAttribI4ui w should be 20");

struct VertexAttribI4uivImmediate {
  typedef VertexAttribI4uivImmediate ValueType;
  static const CommandId kCmdId = kVertexAttribI4uivImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLuint) * 4);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _indx, const GLuint* _values) {
    SetHeader();
    indx = _indx;
    memcpy(ImmediateDataAddress(this), _values, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _indx, const GLuint* _values) {
    static_cast<ValueType*>(cmd)->Init(_indx, _values);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t indx;
};

static_assert(sizeof(VertexAttribI4uivImmediate) == 8,
              "size of VertexAttribI4uivImmediate should be 8");
static_assert(offsetof(VertexAttribI4uivImmediate, header) == 0,
              "offset of VertexAttribI4uivImmediate header should be 0");
static_assert(offsetof(VertexAttribI4uivImmediate, indx) == 4,
              "offset of VertexAttribI4uivImmediate indx should be 4");

struct VertexAttribIPointer {
  typedef VertexAttribIPointer ValueType;
  static const CommandId kCmdId = kVertexAttribIPointer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx,
            GLint _size,
            GLenum _type,
            GLsizei _stride,
            GLuint _offset) {
    SetHeader();
    indx = _indx;
    size = _size;
    type = _type;
    stride = _stride;
    offset = _offset;
  }

  void* Set(void* cmd,
            GLuint _indx,
            GLint _size,
            GLenum _type,
            GLsizei _stride,
            GLuint _offset) {
    static_cast<ValueType*>(cmd)->Init(_indx, _size, _type, _stride, _offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  int32_t size;
  uint32_t type;
  int32_t stride;
  uint32_t offset;
};

static_assert(sizeof(VertexAttribIPointer) == 24,
              "size of VertexAttribIPointer should be 24");
static_assert(offsetof(VertexAttribIPointer, header) == 0,
              "offset of VertexAttribIPointer header should be 0");
static_assert(offsetof(VertexAttribIPointer, indx) == 4,
              "offset of VertexAttribIPointer indx should be 4");
static_assert(offsetof(VertexAttribIPointer, size) == 8,
              "offset of VertexAttribIPointer size should be 8");
static_assert(offsetof(VertexAttribIPointer, type) == 12,
              "offset of VertexAttribIPointer type should be 12");
static_assert(offsetof(VertexAttribIPointer, stride) == 16,
              "offset of VertexAttribIPointer stride should be 16");
static_assert(offsetof(VertexAttribIPointer, offset) == 20,
              "offset of VertexAttribIPointer offset should be 20");

struct VertexAttribPointer {
  typedef VertexAttribPointer ValueType;
  static const CommandId kCmdId = kVertexAttribPointer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _indx,
            GLint _size,
            GLenum _type,
            GLboolean _normalized,
            GLsizei _stride,
            GLuint _offset) {
    SetHeader();
    indx = _indx;
    size = _size;
    type = _type;
    normalized = _normalized;
    stride = _stride;
    offset = _offset;
  }

  void* Set(void* cmd,
            GLuint _indx,
            GLint _size,
            GLenum _type,
            GLboolean _normalized,
            GLsizei _stride,
            GLuint _offset) {
    static_cast<ValueType*>(cmd)->Init(_indx, _size, _type, _normalized,
                                       _stride, _offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t indx;
  int32_t size;
  uint32_t type;
  uint32_t normalized;
  int32_t stride;
  uint32_t offset;
};

static_assert(sizeof(VertexAttribPointer) == 28,
              "size of VertexAttribPointer should be 28");
static_assert(offsetof(VertexAttribPointer, header) == 0,
              "offset of VertexAttribPointer header should be 0");
static_assert(offsetof(VertexAttribPointer, indx) == 4,
              "offset of VertexAttribPointer indx should be 4");
static_assert(offsetof(VertexAttribPointer, size) == 8,
              "offset of VertexAttribPointer size should be 8");
static_assert(offsetof(VertexAttribPointer, type) == 12,
              "offset of VertexAttribPointer type should be 12");
static_assert(offsetof(VertexAttribPointer, normalized) == 16,
              "offset of VertexAttribPointer normalized should be 16");
static_assert(offsetof(VertexAttribPointer, stride) == 20,
              "offset of VertexAttribPointer stride should be 20");
static_assert(offsetof(VertexAttribPointer, offset) == 24,
              "offset of VertexAttribPointer offset should be 24");

struct Viewport {
  typedef Viewport ValueType;
  static const CommandId kCmdId = kViewport;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _x, GLint _y, GLsizei _width, GLsizei _height) {
    SetHeader();
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd, GLint _x, GLint _y, GLsizei _width, GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_x, _y, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(Viewport) == 20, "size of Viewport should be 20");
static_assert(offsetof(Viewport, header) == 0,
              "offset of Viewport header should be 0");
static_assert(offsetof(Viewport, x) == 4, "offset of Viewport x should be 4");
static_assert(offsetof(Viewport, y) == 8, "offset of Viewport y should be 8");
static_assert(offsetof(Viewport, width) == 12,
              "offset of Viewport width should be 12");
static_assert(offsetof(Viewport, height) == 16,
              "offset of Viewport height should be 16");

struct WaitSync {
  typedef WaitSync ValueType;
  static const CommandId kCmdId = kWaitSync;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _sync, GLbitfield _flags, GLuint64 _timeout) {
    SetHeader();
    sync = _sync;
    flags = _flags;
    GLES2Util::MapUint64ToTwoUint32(static_cast<uint64_t>(_timeout), &timeout_0,
                                    &timeout_1);
  }

  void* Set(void* cmd, GLuint _sync, GLbitfield _flags, GLuint64 _timeout) {
    static_cast<ValueType*>(cmd)->Init(_sync, _flags, _timeout);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 timeout() const volatile {
    return static_cast<GLuint64>(
        GLES2Util::MapTwoUint32ToUint64(timeout_0, timeout_1));
  }

  gpu::CommandHeader header;
  uint32_t sync;
  uint32_t flags;
  uint32_t timeout_0;
  uint32_t timeout_1;
};

static_assert(sizeof(WaitSync) == 20, "size of WaitSync should be 20");
static_assert(offsetof(WaitSync, header) == 0,
              "offset of WaitSync header should be 0");
static_assert(offsetof(WaitSync, sync) == 4,
              "offset of WaitSync sync should be 4");
static_assert(offsetof(WaitSync, flags) == 8,
              "offset of WaitSync flags should be 8");
static_assert(offsetof(WaitSync, timeout_0) == 12,
              "offset of WaitSync timeout_0 should be 12");
static_assert(offsetof(WaitSync, timeout_1) == 16,
              "offset of WaitSync timeout_1 should be 16");

struct BlitFramebufferCHROMIUM {
  typedef BlitFramebufferCHROMIUM ValueType;
  static const CommandId kCmdId = kBlitFramebufferCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _srcX0,
            GLint _srcY0,
            GLint _srcX1,
            GLint _srcY1,
            GLint _dstX0,
            GLint _dstY0,
            GLint _dstX1,
            GLint _dstY1,
            GLbitfield _mask,
            GLenum _filter) {
    SetHeader();
    srcX0 = _srcX0;
    srcY0 = _srcY0;
    srcX1 = _srcX1;
    srcY1 = _srcY1;
    dstX0 = _dstX0;
    dstY0 = _dstY0;
    dstX1 = _dstX1;
    dstY1 = _dstY1;
    mask = _mask;
    filter = _filter;
  }

  void* Set(void* cmd,
            GLint _srcX0,
            GLint _srcY0,
            GLint _srcX1,
            GLint _srcY1,
            GLint _dstX0,
            GLint _dstY0,
            GLint _dstX1,
            GLint _dstY1,
            GLbitfield _mask,
            GLenum _filter) {
    static_cast<ValueType*>(cmd)->Init(_srcX0, _srcY0, _srcX1, _srcY1, _dstX0,
                                       _dstY0, _dstX1, _dstY1, _mask, _filter);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t srcX0;
  int32_t srcY0;
  int32_t srcX1;
  int32_t srcY1;
  int32_t dstX0;
  int32_t dstY0;
  int32_t dstX1;
  int32_t dstY1;
  uint32_t mask;
  uint32_t filter;
};

static_assert(sizeof(BlitFramebufferCHROMIUM) == 44,
              "size of BlitFramebufferCHROMIUM should be 44");
static_assert(offsetof(BlitFramebufferCHROMIUM, header) == 0,
              "offset of BlitFramebufferCHROMIUM header should be 0");
static_assert(offsetof(BlitFramebufferCHROMIUM, srcX0) == 4,
              "offset of BlitFramebufferCHROMIUM srcX0 should be 4");
static_assert(offsetof(BlitFramebufferCHROMIUM, srcY0) == 8,
              "offset of BlitFramebufferCHROMIUM srcY0 should be 8");
static_assert(offsetof(BlitFramebufferCHROMIUM, srcX1) == 12,
              "offset of BlitFramebufferCHROMIUM srcX1 should be 12");
static_assert(offsetof(BlitFramebufferCHROMIUM, srcY1) == 16,
              "offset of BlitFramebufferCHROMIUM srcY1 should be 16");
static_assert(offsetof(BlitFramebufferCHROMIUM, dstX0) == 20,
              "offset of BlitFramebufferCHROMIUM dstX0 should be 20");
static_assert(offsetof(BlitFramebufferCHROMIUM, dstY0) == 24,
              "offset of BlitFramebufferCHROMIUM dstY0 should be 24");
static_assert(offsetof(BlitFramebufferCHROMIUM, dstX1) == 28,
              "offset of BlitFramebufferCHROMIUM dstX1 should be 28");
static_assert(offsetof(BlitFramebufferCHROMIUM, dstY1) == 32,
              "offset of BlitFramebufferCHROMIUM dstY1 should be 32");
static_assert(offsetof(BlitFramebufferCHROMIUM, mask) == 36,
              "offset of BlitFramebufferCHROMIUM mask should be 36");
static_assert(offsetof(BlitFramebufferCHROMIUM, filter) == 40,
              "offset of BlitFramebufferCHROMIUM filter should be 40");

// GL_CHROMIUM_framebuffer_multisample
struct RenderbufferStorageMultisampleCHROMIUM {
  typedef RenderbufferStorageMultisampleCHROMIUM ValueType;
  static const CommandId kCmdId = kRenderbufferStorageMultisampleCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLsizei _samples,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    samples = _samples;
    internalformat = _internalformat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _samples,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _samples, _internalformat,
                                       _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t samples;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(RenderbufferStorageMultisampleCHROMIUM) == 24,
              "size of RenderbufferStorageMultisampleCHROMIUM should be 24");
static_assert(
    offsetof(RenderbufferStorageMultisampleCHROMIUM, header) == 0,
    "offset of RenderbufferStorageMultisampleCHROMIUM header should be 0");
static_assert(
    offsetof(RenderbufferStorageMultisampleCHROMIUM, target) == 4,
    "offset of RenderbufferStorageMultisampleCHROMIUM target should be 4");
static_assert(
    offsetof(RenderbufferStorageMultisampleCHROMIUM, samples) == 8,
    "offset of RenderbufferStorageMultisampleCHROMIUM samples should be 8");
static_assert(offsetof(RenderbufferStorageMultisampleCHROMIUM,
                       internalformat) == 12,
              "offset of RenderbufferStorageMultisampleCHROMIUM internalformat "
              "should be 12");
static_assert(
    offsetof(RenderbufferStorageMultisampleCHROMIUM, width) == 16,
    "offset of RenderbufferStorageMultisampleCHROMIUM width should be 16");
static_assert(
    offsetof(RenderbufferStorageMultisampleCHROMIUM, height) == 20,
    "offset of RenderbufferStorageMultisampleCHROMIUM height should be 20");

// GL_AMD_framebuffer_multisample_advanced
struct RenderbufferStorageMultisampleAdvancedAMD {
  typedef RenderbufferStorageMultisampleAdvancedAMD ValueType;
  static const CommandId kCmdId = kRenderbufferStorageMultisampleAdvancedAMD;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLsizei _samples,
            GLsizei _storageSamples,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    samples = _samples;
    storageSamples = _storageSamples;
    internalformat = _internalformat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _samples,
            GLsizei _storageSamples,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _samples, _storageSamples,
                                       _internalformat, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t samples;
  int32_t storageSamples;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(RenderbufferStorageMultisampleAdvancedAMD) == 28,
              "size of RenderbufferStorageMultisampleAdvancedAMD should be 28");
static_assert(
    offsetof(RenderbufferStorageMultisampleAdvancedAMD, header) == 0,
    "offset of RenderbufferStorageMultisampleAdvancedAMD header should be 0");
static_assert(
    offsetof(RenderbufferStorageMultisampleAdvancedAMD, target) == 4,
    "offset of RenderbufferStorageMultisampleAdvancedAMD target should be 4");
static_assert(
    offsetof(RenderbufferStorageMultisampleAdvancedAMD, samples) == 8,
    "offset of RenderbufferStorageMultisampleAdvancedAMD samples should be 8");
static_assert(offsetof(RenderbufferStorageMultisampleAdvancedAMD,
                       storageSamples) == 12,
              "offset of RenderbufferStorageMultisampleAdvancedAMD "
              "storageSamples should be 12");
static_assert(offsetof(RenderbufferStorageMultisampleAdvancedAMD,
                       internalformat) == 16,
              "offset of RenderbufferStorageMultisampleAdvancedAMD "
              "internalformat should be 16");
static_assert(
    offsetof(RenderbufferStorageMultisampleAdvancedAMD, width) == 20,
    "offset of RenderbufferStorageMultisampleAdvancedAMD width should be 20");
static_assert(
    offsetof(RenderbufferStorageMultisampleAdvancedAMD, height) == 24,
    "offset of RenderbufferStorageMultisampleAdvancedAMD height should be 24");

// GL_EXT_multisampled_render_to_texture
struct RenderbufferStorageMultisampleEXT {
  typedef RenderbufferStorageMultisampleEXT ValueType;
  static const CommandId kCmdId = kRenderbufferStorageMultisampleEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLsizei _samples,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    samples = _samples;
    internalformat = _internalformat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _samples,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _samples, _internalformat,
                                       _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t samples;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(RenderbufferStorageMultisampleEXT) == 24,
              "size of RenderbufferStorageMultisampleEXT should be 24");
static_assert(offsetof(RenderbufferStorageMultisampleEXT, header) == 0,
              "offset of RenderbufferStorageMultisampleEXT header should be 0");
static_assert(offsetof(RenderbufferStorageMultisampleEXT, target) == 4,
              "offset of RenderbufferStorageMultisampleEXT target should be 4");
static_assert(
    offsetof(RenderbufferStorageMultisampleEXT, samples) == 8,
    "offset of RenderbufferStorageMultisampleEXT samples should be 8");
static_assert(
    offsetof(RenderbufferStorageMultisampleEXT, internalformat) == 12,
    "offset of RenderbufferStorageMultisampleEXT internalformat should be 12");
static_assert(offsetof(RenderbufferStorageMultisampleEXT, width) == 16,
              "offset of RenderbufferStorageMultisampleEXT width should be 16");
static_assert(
    offsetof(RenderbufferStorageMultisampleEXT, height) == 20,
    "offset of RenderbufferStorageMultisampleEXT height should be 20");

struct FramebufferTexture2DMultisampleEXT {
  typedef FramebufferTexture2DMultisampleEXT ValueType;
  static const CommandId kCmdId = kFramebufferTexture2DMultisampleEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _attachment,
            GLenum _textarget,
            GLuint _texture,
            GLint _level,
            GLsizei _samples) {
    SetHeader();
    target = _target;
    attachment = _attachment;
    textarget = _textarget;
    texture = _texture;
    level = _level;
    samples = _samples;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _attachment,
            GLenum _textarget,
            GLuint _texture,
            GLint _level,
            GLsizei _samples) {
    static_cast<ValueType*>(cmd)->Init(_target, _attachment, _textarget,
                                       _texture, _level, _samples);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t attachment;
  uint32_t textarget;
  uint32_t texture;
  int32_t level;
  int32_t samples;
};

static_assert(sizeof(FramebufferTexture2DMultisampleEXT) == 28,
              "size of FramebufferTexture2DMultisampleEXT should be 28");
static_assert(
    offsetof(FramebufferTexture2DMultisampleEXT, header) == 0,
    "offset of FramebufferTexture2DMultisampleEXT header should be 0");
static_assert(
    offsetof(FramebufferTexture2DMultisampleEXT, target) == 4,
    "offset of FramebufferTexture2DMultisampleEXT target should be 4");
static_assert(
    offsetof(FramebufferTexture2DMultisampleEXT, attachment) == 8,
    "offset of FramebufferTexture2DMultisampleEXT attachment should be 8");
static_assert(
    offsetof(FramebufferTexture2DMultisampleEXT, textarget) == 12,
    "offset of FramebufferTexture2DMultisampleEXT textarget should be 12");
static_assert(
    offsetof(FramebufferTexture2DMultisampleEXT, texture) == 16,
    "offset of FramebufferTexture2DMultisampleEXT texture should be 16");
static_assert(
    offsetof(FramebufferTexture2DMultisampleEXT, level) == 20,
    "offset of FramebufferTexture2DMultisampleEXT level should be 20");
static_assert(
    offsetof(FramebufferTexture2DMultisampleEXT, samples) == 24,
    "offset of FramebufferTexture2DMultisampleEXT samples should be 24");

struct TexStorage2DEXT {
  typedef TexStorage2DEXT ValueType;
  static const CommandId kCmdId = kTexStorage2DEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLsizei _levels,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    levels = _levels;
    internalFormat = _internalFormat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _levels,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _levels, _internalFormat,
                                       _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t levels;
  uint32_t internalFormat;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(TexStorage2DEXT) == 24,
              "size of TexStorage2DEXT should be 24");
static_assert(offsetof(TexStorage2DEXT, header) == 0,
              "offset of TexStorage2DEXT header should be 0");
static_assert(offsetof(TexStorage2DEXT, target) == 4,
              "offset of TexStorage2DEXT target should be 4");
static_assert(offsetof(TexStorage2DEXT, levels) == 8,
              "offset of TexStorage2DEXT levels should be 8");
static_assert(offsetof(TexStorage2DEXT, internalFormat) == 12,
              "offset of TexStorage2DEXT internalFormat should be 12");
static_assert(offsetof(TexStorage2DEXT, width) == 16,
              "offset of TexStorage2DEXT width should be 16");
static_assert(offsetof(TexStorage2DEXT, height) == 20,
              "offset of TexStorage2DEXT height should be 20");

struct GenQueriesEXTImmediate {
  typedef GenQueriesEXTImmediate ValueType;
  static const CommandId kCmdId = kGenQueriesEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _queries) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _queries, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _queries) {
    static_cast<ValueType*>(cmd)->Init(_n, _queries);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenQueriesEXTImmediate) == 8,
              "size of GenQueriesEXTImmediate should be 8");
static_assert(offsetof(GenQueriesEXTImmediate, header) == 0,
              "offset of GenQueriesEXTImmediate header should be 0");
static_assert(offsetof(GenQueriesEXTImmediate, n) == 4,
              "offset of GenQueriesEXTImmediate n should be 4");

struct DeleteQueriesEXTImmediate {
  typedef DeleteQueriesEXTImmediate ValueType;
  static const CommandId kCmdId = kDeleteQueriesEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _queries) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _queries, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _queries) {
    static_cast<ValueType*>(cmd)->Init(_n, _queries);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteQueriesEXTImmediate) == 8,
              "size of DeleteQueriesEXTImmediate should be 8");
static_assert(offsetof(DeleteQueriesEXTImmediate, header) == 0,
              "offset of DeleteQueriesEXTImmediate header should be 0");
static_assert(offsetof(DeleteQueriesEXTImmediate, n) == 4,
              "offset of DeleteQueriesEXTImmediate n should be 4");

struct QueryCounterEXT {
  typedef QueryCounterEXT ValueType;
  static const CommandId kCmdId = kQueryCounterEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _id,
            GLenum _target,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset,
            GLuint _submit_count) {
    SetHeader();
    id = _id;
    target = _target;
    sync_data_shm_id = _sync_data_shm_id;
    sync_data_shm_offset = _sync_data_shm_offset;
    submit_count = _submit_count;
  }

  void* Set(void* cmd,
            GLuint _id,
            GLenum _target,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset,
            GLuint _submit_count) {
    static_cast<ValueType*>(cmd)->Init(_id, _target, _sync_data_shm_id,
                                       _sync_data_shm_offset, _submit_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t id;
  uint32_t target;
  uint32_t sync_data_shm_id;
  uint32_t sync_data_shm_offset;
  uint32_t submit_count;
};

static_assert(sizeof(QueryCounterEXT) == 24,
              "size of QueryCounterEXT should be 24");
static_assert(offsetof(QueryCounterEXT, header) == 0,
              "offset of QueryCounterEXT header should be 0");
static_assert(offsetof(QueryCounterEXT, id) == 4,
              "offset of QueryCounterEXT id should be 4");
static_assert(offsetof(QueryCounterEXT, target) == 8,
              "offset of QueryCounterEXT target should be 8");
static_assert(offsetof(QueryCounterEXT, sync_data_shm_id) == 12,
              "offset of QueryCounterEXT sync_data_shm_id should be 12");
static_assert(offsetof(QueryCounterEXT, sync_data_shm_offset) == 16,
              "offset of QueryCounterEXT sync_data_shm_offset should be 16");
static_assert(offsetof(QueryCounterEXT, submit_count) == 20,
              "offset of QueryCounterEXT submit_count should be 20");

struct BeginQueryEXT {
  typedef BeginQueryEXT ValueType;
  static const CommandId kCmdId = kBeginQueryEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLuint _id,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset) {
    SetHeader();
    target = _target;
    id = _id;
    sync_data_shm_id = _sync_data_shm_id;
    sync_data_shm_offset = _sync_data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLuint _id,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _id, _sync_data_shm_id,
                                       _sync_data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t id;
  uint32_t sync_data_shm_id;
  uint32_t sync_data_shm_offset;
};

static_assert(sizeof(BeginQueryEXT) == 20,
              "size of BeginQueryEXT should be 20");
static_assert(offsetof(BeginQueryEXT, header) == 0,
              "offset of BeginQueryEXT header should be 0");
static_assert(offsetof(BeginQueryEXT, target) == 4,
              "offset of BeginQueryEXT target should be 4");
static_assert(offsetof(BeginQueryEXT, id) == 8,
              "offset of BeginQueryEXT id should be 8");
static_assert(offsetof(BeginQueryEXT, sync_data_shm_id) == 12,
              "offset of BeginQueryEXT sync_data_shm_id should be 12");
static_assert(offsetof(BeginQueryEXT, sync_data_shm_offset) == 16,
              "offset of BeginQueryEXT sync_data_shm_offset should be 16");

struct BeginTransformFeedback {
  typedef BeginTransformFeedback ValueType;
  static const CommandId kCmdId = kBeginTransformFeedback;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _primitivemode) {
    SetHeader();
    primitivemode = _primitivemode;
  }

  void* Set(void* cmd, GLenum _primitivemode) {
    static_cast<ValueType*>(cmd)->Init(_primitivemode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t primitivemode;
};

static_assert(sizeof(BeginTransformFeedback) == 8,
              "size of BeginTransformFeedback should be 8");
static_assert(offsetof(BeginTransformFeedback, header) == 0,
              "offset of BeginTransformFeedback header should be 0");
static_assert(offsetof(BeginTransformFeedback, primitivemode) == 4,
              "offset of BeginTransformFeedback primitivemode should be 4");

struct EndQueryEXT {
  typedef EndQueryEXT ValueType;
  static const CommandId kCmdId = kEndQueryEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _submit_count) {
    SetHeader();
    target = _target;
    submit_count = _submit_count;
  }

  void* Set(void* cmd, GLenum _target, GLuint _submit_count) {
    static_cast<ValueType*>(cmd)->Init(_target, _submit_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t submit_count;
};

static_assert(sizeof(EndQueryEXT) == 12, "size of EndQueryEXT should be 12");
static_assert(offsetof(EndQueryEXT, header) == 0,
              "offset of EndQueryEXT header should be 0");
static_assert(offsetof(EndQueryEXT, target) == 4,
              "offset of EndQueryEXT target should be 4");
static_assert(offsetof(EndQueryEXT, submit_count) == 8,
              "offset of EndQueryEXT submit_count should be 8");

struct EndTransformFeedback {
  typedef EndTransformFeedback ValueType;
  static const CommandId kCmdId = kEndTransformFeedback;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(EndTransformFeedback) == 4,
              "size of EndTransformFeedback should be 4");
static_assert(offsetof(EndTransformFeedback, header) == 0,
              "offset of EndTransformFeedback header should be 0");

struct SetDisjointValueSyncCHROMIUM {
  typedef SetDisjointValueSyncCHROMIUM ValueType;
  static const CommandId kCmdId = kSetDisjointValueSyncCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _sync_data_shm_id, uint32_t _sync_data_shm_offset) {
    SetHeader();
    sync_data_shm_id = _sync_data_shm_id;
    sync_data_shm_offset = _sync_data_shm_offset;
  }

  void* Set(void* cmd,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_sync_data_shm_id,
                                       _sync_data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t sync_data_shm_id;
  uint32_t sync_data_shm_offset;
};

static_assert(sizeof(SetDisjointValueSyncCHROMIUM) == 12,
              "size of SetDisjointValueSyncCHROMIUM should be 12");
static_assert(offsetof(SetDisjointValueSyncCHROMIUM, header) == 0,
              "offset of SetDisjointValueSyncCHROMIUM header should be 0");
static_assert(
    offsetof(SetDisjointValueSyncCHROMIUM, sync_data_shm_id) == 4,
    "offset of SetDisjointValueSyncCHROMIUM sync_data_shm_id should be 4");
static_assert(
    offsetof(SetDisjointValueSyncCHROMIUM, sync_data_shm_offset) == 8,
    "offset of SetDisjointValueSyncCHROMIUM sync_data_shm_offset should be 8");

struct InsertEventMarkerEXT {
  typedef InsertEventMarkerEXT ValueType;
  static const CommandId kCmdId = kInsertEventMarkerEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _bucket_id) {
    SetHeader();
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t bucket_id;
};

static_assert(sizeof(InsertEventMarkerEXT) == 8,
              "size of InsertEventMarkerEXT should be 8");
static_assert(offsetof(InsertEventMarkerEXT, header) == 0,
              "offset of InsertEventMarkerEXT header should be 0");
static_assert(offsetof(InsertEventMarkerEXT, bucket_id) == 4,
              "offset of InsertEventMarkerEXT bucket_id should be 4");

struct PushGroupMarkerEXT {
  typedef PushGroupMarkerEXT ValueType;
  static const CommandId kCmdId = kPushGroupMarkerEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _bucket_id) {
    SetHeader();
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t bucket_id;
};

static_assert(sizeof(PushGroupMarkerEXT) == 8,
              "size of PushGroupMarkerEXT should be 8");
static_assert(offsetof(PushGroupMarkerEXT, header) == 0,
              "offset of PushGroupMarkerEXT header should be 0");
static_assert(offsetof(PushGroupMarkerEXT, bucket_id) == 4,
              "offset of PushGroupMarkerEXT bucket_id should be 4");

struct PopGroupMarkerEXT {
  typedef PopGroupMarkerEXT ValueType;
  static const CommandId kCmdId = kPopGroupMarkerEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(PopGroupMarkerEXT) == 4,
              "size of PopGroupMarkerEXT should be 4");
static_assert(offsetof(PopGroupMarkerEXT, header) == 0,
              "offset of PopGroupMarkerEXT header should be 0");

struct GenVertexArraysOESImmediate {
  typedef GenVertexArraysOESImmediate ValueType;
  static const CommandId kCmdId = kGenVertexArraysOESImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _arrays) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _arrays, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _arrays) {
    static_cast<ValueType*>(cmd)->Init(_n, _arrays);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenVertexArraysOESImmediate) == 8,
              "size of GenVertexArraysOESImmediate should be 8");
static_assert(offsetof(GenVertexArraysOESImmediate, header) == 0,
              "offset of GenVertexArraysOESImmediate header should be 0");
static_assert(offsetof(GenVertexArraysOESImmediate, n) == 4,
              "offset of GenVertexArraysOESImmediate n should be 4");

struct DeleteVertexArraysOESImmediate {
  typedef DeleteVertexArraysOESImmediate ValueType;
  static const CommandId kCmdId = kDeleteVertexArraysOESImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _arrays) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _arrays, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _arrays) {
    static_cast<ValueType*>(cmd)->Init(_n, _arrays);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteVertexArraysOESImmediate) == 8,
              "size of DeleteVertexArraysOESImmediate should be 8");
static_assert(offsetof(DeleteVertexArraysOESImmediate, header) == 0,
              "offset of DeleteVertexArraysOESImmediate header should be 0");
static_assert(offsetof(DeleteVertexArraysOESImmediate, n) == 4,
              "offset of DeleteVertexArraysOESImmediate n should be 4");

struct IsVertexArrayOES {
  typedef IsVertexArrayOES ValueType;
  static const CommandId kCmdId = kIsVertexArrayOES;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _array,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    array = _array;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _array,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_array, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t array;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsVertexArrayOES) == 16,
              "size of IsVertexArrayOES should be 16");
static_assert(offsetof(IsVertexArrayOES, header) == 0,
              "offset of IsVertexArrayOES header should be 0");
static_assert(offsetof(IsVertexArrayOES, array) == 4,
              "offset of IsVertexArrayOES array should be 4");
static_assert(offsetof(IsVertexArrayOES, result_shm_id) == 8,
              "offset of IsVertexArrayOES result_shm_id should be 8");
static_assert(offsetof(IsVertexArrayOES, result_shm_offset) == 12,
              "offset of IsVertexArrayOES result_shm_offset should be 12");

struct BindVertexArrayOES {
  typedef BindVertexArrayOES ValueType;
  static const CommandId kCmdId = kBindVertexArrayOES;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _array) {
    SetHeader();
    array = _array;
  }

  void* Set(void* cmd, GLuint _array) {
    static_cast<ValueType*>(cmd)->Init(_array);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t array;
};

static_assert(sizeof(BindVertexArrayOES) == 8,
              "size of BindVertexArrayOES should be 8");
static_assert(offsetof(BindVertexArrayOES, header) == 0,
              "offset of BindVertexArrayOES header should be 0");
static_assert(offsetof(BindVertexArrayOES, array) == 4,
              "offset of BindVertexArrayOES array should be 4");

struct FramebufferParameteri {
  typedef FramebufferParameteri ValueType;
  static const CommandId kCmdId = kFramebufferParameteri;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLenum _pname, GLint _param) {
    SetHeader();
    target = _target;
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLenum _target, GLenum _pname, GLint _param) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  int32_t param;
};

static_assert(sizeof(FramebufferParameteri) == 16,
              "size of FramebufferParameteri should be 16");
static_assert(offsetof(FramebufferParameteri, header) == 0,
              "offset of FramebufferParameteri header should be 0");
static_assert(offsetof(FramebufferParameteri, target) == 4,
              "offset of FramebufferParameteri target should be 4");
static_assert(offsetof(FramebufferParameteri, pname) == 8,
              "offset of FramebufferParameteri pname should be 8");
static_assert(offsetof(FramebufferParameteri, param) == 12,
              "offset of FramebufferParameteri param should be 12");

struct BindImageTexture {
  typedef BindImageTexture ValueType;
  static const CommandId kCmdId = kBindImageTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _unit,
            GLuint _texture,
            GLint _level,
            GLboolean _layered,
            GLint _layer,
            GLenum _access,
            GLenum _format) {
    SetHeader();
    unit = _unit;
    texture = _texture;
    level = _level;
    layered = _layered;
    layer = _layer;
    access = _access;
    format = _format;
  }

  void* Set(void* cmd,
            GLuint _unit,
            GLuint _texture,
            GLint _level,
            GLboolean _layered,
            GLint _layer,
            GLenum _access,
            GLenum _format) {
    static_cast<ValueType*>(cmd)->Init(_unit, _texture, _level, _layered,
                                       _layer, _access, _format);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t unit;
  uint32_t texture;
  int32_t level;
  uint32_t layered;
  int32_t layer;
  uint32_t access;
  uint32_t format;
};

static_assert(sizeof(BindImageTexture) == 32,
              "size of BindImageTexture should be 32");
static_assert(offsetof(BindImageTexture, header) == 0,
              "offset of BindImageTexture header should be 0");
static_assert(offsetof(BindImageTexture, unit) == 4,
              "offset of BindImageTexture unit should be 4");
static_assert(offsetof(BindImageTexture, texture) == 8,
              "offset of BindImageTexture texture should be 8");
static_assert(offsetof(BindImageTexture, level) == 12,
              "offset of BindImageTexture level should be 12");
static_assert(offsetof(BindImageTexture, layered) == 16,
              "offset of BindImageTexture layered should be 16");
static_assert(offsetof(BindImageTexture, layer) == 20,
              "offset of BindImageTexture layer should be 20");
static_assert(offsetof(BindImageTexture, access) == 24,
              "offset of BindImageTexture access should be 24");
static_assert(offsetof(BindImageTexture, format) == 28,
              "offset of BindImageTexture format should be 28");

struct DispatchCompute {
  typedef DispatchCompute ValueType;
  static const CommandId kCmdId = kDispatchCompute;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _num_groups_x, GLuint _num_groups_y, GLuint _num_groups_z) {
    SetHeader();
    num_groups_x = _num_groups_x;
    num_groups_y = _num_groups_y;
    num_groups_z = _num_groups_z;
  }

  void* Set(void* cmd,
            GLuint _num_groups_x,
            GLuint _num_groups_y,
            GLuint _num_groups_z) {
    static_cast<ValueType*>(cmd)->Init(_num_groups_x, _num_groups_y,
                                       _num_groups_z);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t num_groups_x;
  uint32_t num_groups_y;
  uint32_t num_groups_z;
};

static_assert(sizeof(DispatchCompute) == 16,
              "size of DispatchCompute should be 16");
static_assert(offsetof(DispatchCompute, header) == 0,
              "offset of DispatchCompute header should be 0");
static_assert(offsetof(DispatchCompute, num_groups_x) == 4,
              "offset of DispatchCompute num_groups_x should be 4");
static_assert(offsetof(DispatchCompute, num_groups_y) == 8,
              "offset of DispatchCompute num_groups_y should be 8");
static_assert(offsetof(DispatchCompute, num_groups_z) == 12,
              "offset of DispatchCompute num_groups_z should be 12");

struct DispatchComputeIndirect {
  typedef DispatchComputeIndirect ValueType;
  static const CommandId kCmdId = kDispatchComputeIndirect;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLintptr _offset) {
    SetHeader();
    offset = _offset;
  }

  void* Set(void* cmd, GLintptr _offset) {
    static_cast<ValueType*>(cmd)->Init(_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t offset;
};

static_assert(sizeof(DispatchComputeIndirect) == 8,
              "size of DispatchComputeIndirect should be 8");
static_assert(offsetof(DispatchComputeIndirect, header) == 0,
              "offset of DispatchComputeIndirect header should be 0");
static_assert(offsetof(DispatchComputeIndirect, offset) == 4,
              "offset of DispatchComputeIndirect offset should be 4");

struct DrawArraysIndirect {
  typedef DrawArraysIndirect ValueType;
  static const CommandId kCmdId = kDrawArraysIndirect;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode, GLuint _offset) {
    SetHeader();
    mode = _mode;
    offset = _offset;
  }

  void* Set(void* cmd, GLenum _mode, GLuint _offset) {
    static_cast<ValueType*>(cmd)->Init(_mode, _offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t offset;
};

static_assert(sizeof(DrawArraysIndirect) == 12,
              "size of DrawArraysIndirect should be 12");
static_assert(offsetof(DrawArraysIndirect, header) == 0,
              "offset of DrawArraysIndirect header should be 0");
static_assert(offsetof(DrawArraysIndirect, mode) == 4,
              "offset of DrawArraysIndirect mode should be 4");
static_assert(offsetof(DrawArraysIndirect, offset) == 8,
              "offset of DrawArraysIndirect offset should be 8");

struct DrawElementsIndirect {
  typedef DrawElementsIndirect ValueType;
  static const CommandId kCmdId = kDrawElementsIndirect;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode, GLenum _type, GLuint _offset) {
    SetHeader();
    mode = _mode;
    type = _type;
    offset = _offset;
  }

  void* Set(void* cmd, GLenum _mode, GLenum _type, GLuint _offset) {
    static_cast<ValueType*>(cmd)->Init(_mode, _type, _offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  uint32_t type;
  uint32_t offset;
};

static_assert(sizeof(DrawElementsIndirect) == 16,
              "size of DrawElementsIndirect should be 16");
static_assert(offsetof(DrawElementsIndirect, header) == 0,
              "offset of DrawElementsIndirect header should be 0");
static_assert(offsetof(DrawElementsIndirect, mode) == 4,
              "offset of DrawElementsIndirect mode should be 4");
static_assert(offsetof(DrawElementsIndirect, type) == 8,
              "offset of DrawElementsIndirect type should be 8");
static_assert(offsetof(DrawElementsIndirect, offset) == 12,
              "offset of DrawElementsIndirect offset should be 12");

struct GetProgramInterfaceiv {
  typedef GetProgramInterfaceiv ValueType;
  static const CommandId kCmdId = kGetProgramInterfaceiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLenum _program_interface,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    program_interface = _program_interface;
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLenum _program_interface,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _program_interface, _pname,
                                       _params_shm_id, _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t program_interface;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetProgramInterfaceiv) == 24,
              "size of GetProgramInterfaceiv should be 24");
static_assert(offsetof(GetProgramInterfaceiv, header) == 0,
              "offset of GetProgramInterfaceiv header should be 0");
static_assert(offsetof(GetProgramInterfaceiv, program) == 4,
              "offset of GetProgramInterfaceiv program should be 4");
static_assert(offsetof(GetProgramInterfaceiv, program_interface) == 8,
              "offset of GetProgramInterfaceiv program_interface should be 8");
static_assert(offsetof(GetProgramInterfaceiv, pname) == 12,
              "offset of GetProgramInterfaceiv pname should be 12");
static_assert(offsetof(GetProgramInterfaceiv, params_shm_id) == 16,
              "offset of GetProgramInterfaceiv params_shm_id should be 16");
static_assert(offsetof(GetProgramInterfaceiv, params_shm_offset) == 20,
              "offset of GetProgramInterfaceiv params_shm_offset should be 20");

struct GetProgramResourceIndex {
  typedef GetProgramResourceIndex ValueType;
  static const CommandId kCmdId = kGetProgramResourceIndex;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef GLuint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLenum _program_interface,
            uint32_t _name_bucket_id,
            uint32_t _index_shm_id,
            uint32_t _index_shm_offset) {
    SetHeader();
    program = _program;
    program_interface = _program_interface;
    name_bucket_id = _name_bucket_id;
    index_shm_id = _index_shm_id;
    index_shm_offset = _index_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLenum _program_interface,
            uint32_t _name_bucket_id,
            uint32_t _index_shm_id,
            uint32_t _index_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _program_interface,
                                       _name_bucket_id, _index_shm_id,
                                       _index_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t program_interface;
  uint32_t name_bucket_id;
  uint32_t index_shm_id;
  uint32_t index_shm_offset;
};

static_assert(sizeof(GetProgramResourceIndex) == 24,
              "size of GetProgramResourceIndex should be 24");
static_assert(offsetof(GetProgramResourceIndex, header) == 0,
              "offset of GetProgramResourceIndex header should be 0");
static_assert(offsetof(GetProgramResourceIndex, program) == 4,
              "offset of GetProgramResourceIndex program should be 4");
static_assert(
    offsetof(GetProgramResourceIndex, program_interface) == 8,
    "offset of GetProgramResourceIndex program_interface should be 8");
static_assert(offsetof(GetProgramResourceIndex, name_bucket_id) == 12,
              "offset of GetProgramResourceIndex name_bucket_id should be 12");
static_assert(offsetof(GetProgramResourceIndex, index_shm_id) == 16,
              "offset of GetProgramResourceIndex index_shm_id should be 16");
static_assert(
    offsetof(GetProgramResourceIndex, index_shm_offset) == 20,
    "offset of GetProgramResourceIndex index_shm_offset should be 20");

struct GetProgramResourceName {
  typedef GetProgramResourceName ValueType;
  static const CommandId kCmdId = kGetProgramResourceName;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef int32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLenum _program_interface,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    program = _program;
    program_interface = _program_interface;
    index = _index;
    name_bucket_id = _name_bucket_id;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLenum _program_interface,
            GLuint _index,
            uint32_t _name_bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _program_interface, _index,
                                       _name_bucket_id, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t program_interface;
  uint32_t index;
  uint32_t name_bucket_id;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetProgramResourceName) == 28,
              "size of GetProgramResourceName should be 28");
static_assert(offsetof(GetProgramResourceName, header) == 0,
              "offset of GetProgramResourceName header should be 0");
static_assert(offsetof(GetProgramResourceName, program) == 4,
              "offset of GetProgramResourceName program should be 4");
static_assert(offsetof(GetProgramResourceName, program_interface) == 8,
              "offset of GetProgramResourceName program_interface should be 8");
static_assert(offsetof(GetProgramResourceName, index) == 12,
              "offset of GetProgramResourceName index should be 12");
static_assert(offsetof(GetProgramResourceName, name_bucket_id) == 16,
              "offset of GetProgramResourceName name_bucket_id should be 16");
static_assert(offsetof(GetProgramResourceName, result_shm_id) == 20,
              "offset of GetProgramResourceName result_shm_id should be 20");
static_assert(
    offsetof(GetProgramResourceName, result_shm_offset) == 24,
    "offset of GetProgramResourceName result_shm_offset should be 24");

struct GetProgramResourceiv {
  typedef GetProgramResourceiv ValueType;
  static const CommandId kCmdId = kGetProgramResourceiv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLenum _program_interface,
            GLuint _index,
            uint32_t _props_bucket_id,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    program = _program;
    program_interface = _program_interface;
    index = _index;
    props_bucket_id = _props_bucket_id;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLenum _program_interface,
            GLuint _index,
            uint32_t _props_bucket_id,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _program_interface, _index,
                                       _props_bucket_id, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t program_interface;
  uint32_t index;
  uint32_t props_bucket_id;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetProgramResourceiv) == 28,
              "size of GetProgramResourceiv should be 28");
static_assert(offsetof(GetProgramResourceiv, header) == 0,
              "offset of GetProgramResourceiv header should be 0");
static_assert(offsetof(GetProgramResourceiv, program) == 4,
              "offset of GetProgramResourceiv program should be 4");
static_assert(offsetof(GetProgramResourceiv, program_interface) == 8,
              "offset of GetProgramResourceiv program_interface should be 8");
static_assert(offsetof(GetProgramResourceiv, index) == 12,
              "offset of GetProgramResourceiv index should be 12");
static_assert(offsetof(GetProgramResourceiv, props_bucket_id) == 16,
              "offset of GetProgramResourceiv props_bucket_id should be 16");
static_assert(offsetof(GetProgramResourceiv, params_shm_id) == 20,
              "offset of GetProgramResourceiv params_shm_id should be 20");
static_assert(offsetof(GetProgramResourceiv, params_shm_offset) == 24,
              "offset of GetProgramResourceiv params_shm_offset should be 24");

struct GetProgramResourceLocation {
  typedef GetProgramResourceLocation ValueType;
  static const CommandId kCmdId = kGetProgramResourceLocation;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef GLint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLenum _program_interface,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    SetHeader();
    program = _program;
    program_interface = _program_interface;
    name_bucket_id = _name_bucket_id;
    location_shm_id = _location_shm_id;
    location_shm_offset = _location_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLenum _program_interface,
            uint32_t _name_bucket_id,
            uint32_t _location_shm_id,
            uint32_t _location_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _program_interface,
                                       _name_bucket_id, _location_shm_id,
                                       _location_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t program_interface;
  uint32_t name_bucket_id;
  uint32_t location_shm_id;
  uint32_t location_shm_offset;
};

static_assert(sizeof(GetProgramResourceLocation) == 24,
              "size of GetProgramResourceLocation should be 24");
static_assert(offsetof(GetProgramResourceLocation, header) == 0,
              "offset of GetProgramResourceLocation header should be 0");
static_assert(offsetof(GetProgramResourceLocation, program) == 4,
              "offset of GetProgramResourceLocation program should be 4");
static_assert(
    offsetof(GetProgramResourceLocation, program_interface) == 8,
    "offset of GetProgramResourceLocation program_interface should be 8");
static_assert(
    offsetof(GetProgramResourceLocation, name_bucket_id) == 12,
    "offset of GetProgramResourceLocation name_bucket_id should be 12");
static_assert(
    offsetof(GetProgramResourceLocation, location_shm_id) == 16,
    "offset of GetProgramResourceLocation location_shm_id should be 16");
static_assert(
    offsetof(GetProgramResourceLocation, location_shm_offset) == 20,
    "offset of GetProgramResourceLocation location_shm_offset should be 20");

struct MemoryBarrierEXT {
  typedef MemoryBarrierEXT ValueType;
  static const CommandId kCmdId = kMemoryBarrierEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLbitfield _barriers) {
    SetHeader();
    barriers = _barriers;
  }

  void* Set(void* cmd, GLbitfield _barriers) {
    static_cast<ValueType*>(cmd)->Init(_barriers);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t barriers;
};

static_assert(sizeof(MemoryBarrierEXT) == 8,
              "size of MemoryBarrierEXT should be 8");
static_assert(offsetof(MemoryBarrierEXT, header) == 0,
              "offset of MemoryBarrierEXT header should be 0");
static_assert(offsetof(MemoryBarrierEXT, barriers) == 4,
              "offset of MemoryBarrierEXT barriers should be 4");

struct MemoryBarrierByRegion {
  typedef MemoryBarrierByRegion ValueType;
  static const CommandId kCmdId = kMemoryBarrierByRegion;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLbitfield _barriers) {
    SetHeader();
    barriers = _barriers;
  }

  void* Set(void* cmd, GLbitfield _barriers) {
    static_cast<ValueType*>(cmd)->Init(_barriers);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t barriers;
};

static_assert(sizeof(MemoryBarrierByRegion) == 8,
              "size of MemoryBarrierByRegion should be 8");
static_assert(offsetof(MemoryBarrierByRegion, header) == 0,
              "offset of MemoryBarrierByRegion header should be 0");
static_assert(offsetof(MemoryBarrierByRegion, barriers) == 4,
              "offset of MemoryBarrierByRegion barriers should be 4");

struct SwapBuffers {
  typedef SwapBuffers ValueType;
  static const CommandId kCmdId = kSwapBuffers;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint64 _swap_id, GLbitfield _flags) {
    SetHeader();
    GLES2Util::MapUint64ToTwoUint32(static_cast<uint64_t>(_swap_id), &swap_id_0,
                                    &swap_id_1);
    flags = _flags;
    bool is_tracing = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(
        TRACE_DISABLED_BY_DEFAULT("gpu_cmd_queue"), &is_tracing);
    if (is_tracing) {
      trace_id = base::RandUint64();
      TRACE_EVENT_WITH_FLOW1(
          TRACE_DISABLED_BY_DEFAULT("gpu_cmd_queue"), "CommandBufferQueue",
          trace_id, TRACE_EVENT_FLAG_FLOW_OUT, "command", "SwapBuffers");
    } else {
      trace_id = 0;
    }
  }

  void* Set(void* cmd, GLuint64 _swap_id, GLbitfield _flags) {
    static_cast<ValueType*>(cmd)->Init(_swap_id, _flags);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 swap_id() const volatile {
    return static_cast<GLuint64>(
        GLES2Util::MapTwoUint32ToUint64(swap_id_0, swap_id_1));
  }

  gpu::CommandHeader header;
  uint32_t swap_id_0;
  uint32_t swap_id_1;
  uint32_t flags;
  uint32_t trace_id;
};

static_assert(sizeof(SwapBuffers) == 20, "size of SwapBuffers should be 20");
static_assert(offsetof(SwapBuffers, header) == 0,
              "offset of SwapBuffers header should be 0");
static_assert(offsetof(SwapBuffers, swap_id_0) == 4,
              "offset of SwapBuffers swap_id_0 should be 4");
static_assert(offsetof(SwapBuffers, swap_id_1) == 8,
              "offset of SwapBuffers swap_id_1 should be 8");
static_assert(offsetof(SwapBuffers, flags) == 12,
              "offset of SwapBuffers flags should be 12");

struct GetMaxValueInBufferCHROMIUM {
  typedef GetMaxValueInBufferCHROMIUM ValueType;
  static const CommandId kCmdId = kGetMaxValueInBufferCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLuint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _buffer_id,
            GLsizei _count,
            GLenum _type,
            GLuint _offset,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    buffer_id = _buffer_id;
    count = _count;
    type = _type;
    offset = _offset;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _buffer_id,
            GLsizei _count,
            GLenum _type,
            GLuint _offset,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_buffer_id, _count, _type, _offset,
                                       _result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t buffer_id;
  int32_t count;
  uint32_t type;
  uint32_t offset;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetMaxValueInBufferCHROMIUM) == 28,
              "size of GetMaxValueInBufferCHROMIUM should be 28");
static_assert(offsetof(GetMaxValueInBufferCHROMIUM, header) == 0,
              "offset of GetMaxValueInBufferCHROMIUM header should be 0");
static_assert(offsetof(GetMaxValueInBufferCHROMIUM, buffer_id) == 4,
              "offset of GetMaxValueInBufferCHROMIUM buffer_id should be 4");
static_assert(offsetof(GetMaxValueInBufferCHROMIUM, count) == 8,
              "offset of GetMaxValueInBufferCHROMIUM count should be 8");
static_assert(offsetof(GetMaxValueInBufferCHROMIUM, type) == 12,
              "offset of GetMaxValueInBufferCHROMIUM type should be 12");
static_assert(offsetof(GetMaxValueInBufferCHROMIUM, offset) == 16,
              "offset of GetMaxValueInBufferCHROMIUM offset should be 16");
static_assert(
    offsetof(GetMaxValueInBufferCHROMIUM, result_shm_id) == 20,
    "offset of GetMaxValueInBufferCHROMIUM result_shm_id should be 20");
static_assert(
    offsetof(GetMaxValueInBufferCHROMIUM, result_shm_offset) == 24,
    "offset of GetMaxValueInBufferCHROMIUM result_shm_offset should be 24");

struct EnableFeatureCHROMIUM {
  typedef EnableFeatureCHROMIUM ValueType;
  static const CommandId kCmdId = kEnableFeatureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    bucket_id = _bucket_id;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _bucket_id,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_bucket_id, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t bucket_id;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(EnableFeatureCHROMIUM) == 16,
              "size of EnableFeatureCHROMIUM should be 16");
static_assert(offsetof(EnableFeatureCHROMIUM, header) == 0,
              "offset of EnableFeatureCHROMIUM header should be 0");
static_assert(offsetof(EnableFeatureCHROMIUM, bucket_id) == 4,
              "offset of EnableFeatureCHROMIUM bucket_id should be 4");
static_assert(offsetof(EnableFeatureCHROMIUM, result_shm_id) == 8,
              "offset of EnableFeatureCHROMIUM result_shm_id should be 8");
static_assert(offsetof(EnableFeatureCHROMIUM, result_shm_offset) == 12,
              "offset of EnableFeatureCHROMIUM result_shm_offset should be 12");

struct MapBufferRange {
  typedef MapBufferRange ValueType;
  static const CommandId kCmdId = kMapBufferRange;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLintptr _offset,
            GLsizeiptr _size,
            GLbitfield _access,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    target = _target;
    offset = _offset;
    size = _size;
    access = _access;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLintptr _offset,
            GLsizeiptr _size,
            GLbitfield _access,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _offset, _size, _access,
                                       _data_shm_id, _data_shm_offset,
                                       _result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t offset;
  int32_t size;
  uint32_t access;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(MapBufferRange) == 36,
              "size of MapBufferRange should be 36");
static_assert(offsetof(MapBufferRange, header) == 0,
              "offset of MapBufferRange header should be 0");
static_assert(offsetof(MapBufferRange, target) == 4,
              "offset of MapBufferRange target should be 4");
static_assert(offsetof(MapBufferRange, offset) == 8,
              "offset of MapBufferRange offset should be 8");
static_assert(offsetof(MapBufferRange, size) == 12,
              "offset of MapBufferRange size should be 12");
static_assert(offsetof(MapBufferRange, access) == 16,
              "offset of MapBufferRange access should be 16");
static_assert(offsetof(MapBufferRange, data_shm_id) == 20,
              "offset of MapBufferRange data_shm_id should be 20");
static_assert(offsetof(MapBufferRange, data_shm_offset) == 24,
              "offset of MapBufferRange data_shm_offset should be 24");
static_assert(offsetof(MapBufferRange, result_shm_id) == 28,
              "offset of MapBufferRange result_shm_id should be 28");
static_assert(offsetof(MapBufferRange, result_shm_offset) == 32,
              "offset of MapBufferRange result_shm_offset should be 32");

struct UnmapBuffer {
  typedef UnmapBuffer ValueType;
  static const CommandId kCmdId = kUnmapBuffer;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target) {
    SetHeader();
    target = _target;
  }

  void* Set(void* cmd, GLenum _target) {
    static_cast<ValueType*>(cmd)->Init(_target);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
};

static_assert(sizeof(UnmapBuffer) == 8, "size of UnmapBuffer should be 8");
static_assert(offsetof(UnmapBuffer, header) == 0,
              "offset of UnmapBuffer header should be 0");
static_assert(offsetof(UnmapBuffer, target) == 4,
              "offset of UnmapBuffer target should be 4");

struct FlushMappedBufferRange {
  typedef FlushMappedBufferRange ValueType;
  static const CommandId kCmdId = kFlushMappedBufferRange;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLintptr _offset, GLsizeiptr _size) {
    SetHeader();
    target = _target;
    offset = _offset;
    size = _size;
  }

  void* Set(void* cmd, GLenum _target, GLintptr _offset, GLsizeiptr _size) {
    static_cast<ValueType*>(cmd)->Init(_target, _offset, _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t offset;
  int32_t size;
};

static_assert(sizeof(FlushMappedBufferRange) == 16,
              "size of FlushMappedBufferRange should be 16");
static_assert(offsetof(FlushMappedBufferRange, header) == 0,
              "offset of FlushMappedBufferRange header should be 0");
static_assert(offsetof(FlushMappedBufferRange, target) == 4,
              "offset of FlushMappedBufferRange target should be 4");
static_assert(offsetof(FlushMappedBufferRange, offset) == 8,
              "offset of FlushMappedBufferRange offset should be 8");
static_assert(offsetof(FlushMappedBufferRange, size) == 12,
              "offset of FlushMappedBufferRange size should be 12");

struct ResizeCHROMIUM {
  typedef ResizeCHROMIUM ValueType;
  static const CommandId kCmdId = kResizeCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _width,
            GLuint _height,
            GLfloat _scale_factor,
            GLenum _color_space,
            GLboolean _alpha) {
    SetHeader();
    width = _width;
    height = _height;
    scale_factor = _scale_factor;
    color_space = _color_space;
    alpha = _alpha;
  }

  void* Set(void* cmd,
            GLuint _width,
            GLuint _height,
            GLfloat _scale_factor,
            GLenum _color_space,
            GLboolean _alpha) {
    static_cast<ValueType*>(cmd)->Init(_width, _height, _scale_factor,
                                       _color_space, _alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t width;
  uint32_t height;
  float scale_factor;
  uint32_t color_space;
  uint32_t alpha;
};

static_assert(sizeof(ResizeCHROMIUM) == 24,
              "size of ResizeCHROMIUM should be 24");
static_assert(offsetof(ResizeCHROMIUM, header) == 0,
              "offset of ResizeCHROMIUM header should be 0");
static_assert(offsetof(ResizeCHROMIUM, width) == 4,
              "offset of ResizeCHROMIUM width should be 4");
static_assert(offsetof(ResizeCHROMIUM, height) == 8,
              "offset of ResizeCHROMIUM height should be 8");
static_assert(offsetof(ResizeCHROMIUM, scale_factor) == 12,
              "offset of ResizeCHROMIUM scale_factor should be 12");
static_assert(offsetof(ResizeCHROMIUM, color_space) == 16,
              "offset of ResizeCHROMIUM color_space should be 16");
static_assert(offsetof(ResizeCHROMIUM, alpha) == 20,
              "offset of ResizeCHROMIUM alpha should be 20");

struct GetRequestableExtensionsCHROMIUM {
  typedef GetRequestableExtensionsCHROMIUM ValueType;
  static const CommandId kCmdId = kGetRequestableExtensionsCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _bucket_id) {
    SetHeader();
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t bucket_id;
};

static_assert(sizeof(GetRequestableExtensionsCHROMIUM) == 8,
              "size of GetRequestableExtensionsCHROMIUM should be 8");
static_assert(offsetof(GetRequestableExtensionsCHROMIUM, header) == 0,
              "offset of GetRequestableExtensionsCHROMIUM header should be 0");
static_assert(
    offsetof(GetRequestableExtensionsCHROMIUM, bucket_id) == 4,
    "offset of GetRequestableExtensionsCHROMIUM bucket_id should be 4");

struct RequestExtensionCHROMIUM {
  typedef RequestExtensionCHROMIUM ValueType;
  static const CommandId kCmdId = kRequestExtensionCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _bucket_id) {
    SetHeader();
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t bucket_id;
};

static_assert(sizeof(RequestExtensionCHROMIUM) == 8,
              "size of RequestExtensionCHROMIUM should be 8");
static_assert(offsetof(RequestExtensionCHROMIUM, header) == 0,
              "offset of RequestExtensionCHROMIUM header should be 0");
static_assert(offsetof(RequestExtensionCHROMIUM, bucket_id) == 4,
              "offset of RequestExtensionCHROMIUM bucket_id should be 4");

struct GetProgramInfoCHROMIUM {
  typedef GetProgramInfoCHROMIUM ValueType;
  static const CommandId kCmdId = kGetProgramInfoCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  struct Result {
    uint32_t link_status;
    uint32_t num_attribs;
    uint32_t num_uniforms;
  };

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, uint32_t _bucket_id) {
    SetHeader();
    program = _program;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _program, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t bucket_id;
};

static_assert(sizeof(GetProgramInfoCHROMIUM) == 12,
              "size of GetProgramInfoCHROMIUM should be 12");
static_assert(offsetof(GetProgramInfoCHROMIUM, header) == 0,
              "offset of GetProgramInfoCHROMIUM header should be 0");
static_assert(offsetof(GetProgramInfoCHROMIUM, program) == 4,
              "offset of GetProgramInfoCHROMIUM program should be 4");
static_assert(offsetof(GetProgramInfoCHROMIUM, bucket_id) == 8,
              "offset of GetProgramInfoCHROMIUM bucket_id should be 8");
static_assert(offsetof(GetProgramInfoCHROMIUM::Result, link_status) == 0,
              "offset of GetProgramInfoCHROMIUM Result link_status should be "
              "0");
static_assert(offsetof(GetProgramInfoCHROMIUM::Result, num_attribs) == 4,
              "offset of GetProgramInfoCHROMIUM Result num_attribs should be "
              "4");
static_assert(offsetof(GetProgramInfoCHROMIUM::Result, num_uniforms) == 8,
              "offset of GetProgramInfoCHROMIUM Result num_uniforms should be "
              "8");

struct GetUniformBlocksCHROMIUM {
  typedef GetUniformBlocksCHROMIUM ValueType;
  static const CommandId kCmdId = kGetUniformBlocksCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, uint32_t _bucket_id) {
    SetHeader();
    program = _program;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _program, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t bucket_id;
};

static_assert(sizeof(GetUniformBlocksCHROMIUM) == 12,
              "size of GetUniformBlocksCHROMIUM should be 12");
static_assert(offsetof(GetUniformBlocksCHROMIUM, header) == 0,
              "offset of GetUniformBlocksCHROMIUM header should be 0");
static_assert(offsetof(GetUniformBlocksCHROMIUM, program) == 4,
              "offset of GetUniformBlocksCHROMIUM program should be 4");
static_assert(offsetof(GetUniformBlocksCHROMIUM, bucket_id) == 8,
              "offset of GetUniformBlocksCHROMIUM bucket_id should be 8");

struct GetTransformFeedbackVaryingsCHROMIUM {
  typedef GetTransformFeedbackVaryingsCHROMIUM ValueType;
  static const CommandId kCmdId = kGetTransformFeedbackVaryingsCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, uint32_t _bucket_id) {
    SetHeader();
    program = _program;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _program, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t bucket_id;
};

static_assert(sizeof(GetTransformFeedbackVaryingsCHROMIUM) == 12,
              "size of GetTransformFeedbackVaryingsCHROMIUM should be 12");
static_assert(
    offsetof(GetTransformFeedbackVaryingsCHROMIUM, header) == 0,
    "offset of GetTransformFeedbackVaryingsCHROMIUM header should be 0");
static_assert(
    offsetof(GetTransformFeedbackVaryingsCHROMIUM, program) == 4,
    "offset of GetTransformFeedbackVaryingsCHROMIUM program should be 4");
static_assert(
    offsetof(GetTransformFeedbackVaryingsCHROMIUM, bucket_id) == 8,
    "offset of GetTransformFeedbackVaryingsCHROMIUM bucket_id should be 8");

struct GetUniformsES3CHROMIUM {
  typedef GetUniformsES3CHROMIUM ValueType;
  static const CommandId kCmdId = kGetUniformsES3CHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, uint32_t _bucket_id) {
    SetHeader();
    program = _program;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _program, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t bucket_id;
};

static_assert(sizeof(GetUniformsES3CHROMIUM) == 12,
              "size of GetUniformsES3CHROMIUM should be 12");
static_assert(offsetof(GetUniformsES3CHROMIUM, header) == 0,
              "offset of GetUniformsES3CHROMIUM header should be 0");
static_assert(offsetof(GetUniformsES3CHROMIUM, program) == 4,
              "offset of GetUniformsES3CHROMIUM program should be 4");
static_assert(offsetof(GetUniformsES3CHROMIUM, bucket_id) == 8,
              "offset of GetUniformsES3CHROMIUM bucket_id should be 8");

struct DescheduleUntilFinishedCHROMIUM {
  typedef DescheduleUntilFinishedCHROMIUM ValueType;
  static const CommandId kCmdId = kDescheduleUntilFinishedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(DescheduleUntilFinishedCHROMIUM) == 4,
              "size of DescheduleUntilFinishedCHROMIUM should be 4");
static_assert(offsetof(DescheduleUntilFinishedCHROMIUM, header) == 0,
              "offset of DescheduleUntilFinishedCHROMIUM header should be 0");

struct GetTranslatedShaderSourceANGLE {
  typedef GetTranslatedShaderSourceANGLE ValueType;
  static const CommandId kCmdId = kGetTranslatedShaderSourceANGLE;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _shader, uint32_t _bucket_id) {
    SetHeader();
    shader = _shader;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd, GLuint _shader, uint32_t _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_shader, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t shader;
  uint32_t bucket_id;
};

static_assert(sizeof(GetTranslatedShaderSourceANGLE) == 12,
              "size of GetTranslatedShaderSourceANGLE should be 12");
static_assert(offsetof(GetTranslatedShaderSourceANGLE, header) == 0,
              "offset of GetTranslatedShaderSourceANGLE header should be 0");
static_assert(offsetof(GetTranslatedShaderSourceANGLE, shader) == 4,
              "offset of GetTranslatedShaderSourceANGLE shader should be 4");
static_assert(offsetof(GetTranslatedShaderSourceANGLE, bucket_id) == 8,
              "offset of GetTranslatedShaderSourceANGLE bucket_id should be 8");

struct PostSubBufferCHROMIUM {
  typedef PostSubBufferCHROMIUM ValueType;
  static const CommandId kCmdId = kPostSubBufferCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint64 _swap_id,
            GLint _x,
            GLint _y,
            GLint _width,
            GLint _height,
            GLbitfield _flags) {
    SetHeader();
    GLES2Util::MapUint64ToTwoUint32(static_cast<uint64_t>(_swap_id), &swap_id_0,
                                    &swap_id_1);
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    flags = _flags;
  }

  void* Set(void* cmd,
            GLuint64 _swap_id,
            GLint _x,
            GLint _y,
            GLint _width,
            GLint _height,
            GLbitfield _flags) {
    static_cast<ValueType*>(cmd)->Init(_swap_id, _x, _y, _width, _height,
                                       _flags);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 swap_id() const volatile {
    return static_cast<GLuint64>(
        GLES2Util::MapTwoUint32ToUint64(swap_id_0, swap_id_1));
  }

  gpu::CommandHeader header;
  uint32_t swap_id_0;
  uint32_t swap_id_1;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  uint32_t flags;
};

static_assert(sizeof(PostSubBufferCHROMIUM) == 32,
              "size of PostSubBufferCHROMIUM should be 32");
static_assert(offsetof(PostSubBufferCHROMIUM, header) == 0,
              "offset of PostSubBufferCHROMIUM header should be 0");
static_assert(offsetof(PostSubBufferCHROMIUM, swap_id_0) == 4,
              "offset of PostSubBufferCHROMIUM swap_id_0 should be 4");
static_assert(offsetof(PostSubBufferCHROMIUM, swap_id_1) == 8,
              "offset of PostSubBufferCHROMIUM swap_id_1 should be 8");
static_assert(offsetof(PostSubBufferCHROMIUM, x) == 12,
              "offset of PostSubBufferCHROMIUM x should be 12");
static_assert(offsetof(PostSubBufferCHROMIUM, y) == 16,
              "offset of PostSubBufferCHROMIUM y should be 16");
static_assert(offsetof(PostSubBufferCHROMIUM, width) == 20,
              "offset of PostSubBufferCHROMIUM width should be 20");
static_assert(offsetof(PostSubBufferCHROMIUM, height) == 24,
              "offset of PostSubBufferCHROMIUM height should be 24");
static_assert(offsetof(PostSubBufferCHROMIUM, flags) == 28,
              "offset of PostSubBufferCHROMIUM flags should be 28");

struct CopyTextureCHROMIUM {
  typedef CopyTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kCopyTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _source_id,
            GLint _source_level,
            GLenum _dest_target,
            GLuint _dest_id,
            GLint _dest_level,
            GLint _internalformat,
            GLenum _dest_type,
            GLboolean _unpack_flip_y,
            GLboolean _unpack_premultiply_alpha,
            GLboolean _unpack_unmultiply_alpha) {
    SetHeader();
    source_id = _source_id;
    source_level = _source_level;
    dest_target = _dest_target;
    dest_id = _dest_id;
    dest_level = _dest_level;
    internalformat = _internalformat;
    dest_type = _dest_type;
    unpack_flip_y = _unpack_flip_y;
    unpack_premultiply_alpha = _unpack_premultiply_alpha;
    unpack_unmultiply_alpha = _unpack_unmultiply_alpha;
  }

  void* Set(void* cmd,
            GLuint _source_id,
            GLint _source_level,
            GLenum _dest_target,
            GLuint _dest_id,
            GLint _dest_level,
            GLint _internalformat,
            GLenum _dest_type,
            GLboolean _unpack_flip_y,
            GLboolean _unpack_premultiply_alpha,
            GLboolean _unpack_unmultiply_alpha) {
    static_cast<ValueType*>(cmd)->Init(
        _source_id, _source_level, _dest_target, _dest_id, _dest_level,
        _internalformat, _dest_type, _unpack_flip_y, _unpack_premultiply_alpha,
        _unpack_unmultiply_alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t source_id;
  int32_t source_level;
  uint32_t dest_target;
  uint32_t dest_id;
  int32_t dest_level;
  int32_t internalformat;
  uint32_t dest_type;
  uint32_t unpack_flip_y;
  uint32_t unpack_premultiply_alpha;
  uint32_t unpack_unmultiply_alpha;
};

static_assert(sizeof(CopyTextureCHROMIUM) == 44,
              "size of CopyTextureCHROMIUM should be 44");
static_assert(offsetof(CopyTextureCHROMIUM, header) == 0,
              "offset of CopyTextureCHROMIUM header should be 0");
static_assert(offsetof(CopyTextureCHROMIUM, source_id) == 4,
              "offset of CopyTextureCHROMIUM source_id should be 4");
static_assert(offsetof(CopyTextureCHROMIUM, source_level) == 8,
              "offset of CopyTextureCHROMIUM source_level should be 8");
static_assert(offsetof(CopyTextureCHROMIUM, dest_target) == 12,
              "offset of CopyTextureCHROMIUM dest_target should be 12");
static_assert(offsetof(CopyTextureCHROMIUM, dest_id) == 16,
              "offset of CopyTextureCHROMIUM dest_id should be 16");
static_assert(offsetof(CopyTextureCHROMIUM, dest_level) == 20,
              "offset of CopyTextureCHROMIUM dest_level should be 20");
static_assert(offsetof(CopyTextureCHROMIUM, internalformat) == 24,
              "offset of CopyTextureCHROMIUM internalformat should be 24");
static_assert(offsetof(CopyTextureCHROMIUM, dest_type) == 28,
              "offset of CopyTextureCHROMIUM dest_type should be 28");
static_assert(offsetof(CopyTextureCHROMIUM, unpack_flip_y) == 32,
              "offset of CopyTextureCHROMIUM unpack_flip_y should be 32");
static_assert(
    offsetof(CopyTextureCHROMIUM, unpack_premultiply_alpha) == 36,
    "offset of CopyTextureCHROMIUM unpack_premultiply_alpha should be 36");
static_assert(
    offsetof(CopyTextureCHROMIUM, unpack_unmultiply_alpha) == 40,
    "offset of CopyTextureCHROMIUM unpack_unmultiply_alpha should be 40");

struct CopySubTextureCHROMIUM {
  typedef CopySubTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kCopySubTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _source_id,
            GLint _source_level,
            GLenum _dest_target,
            GLuint _dest_id,
            GLint _dest_level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLboolean _unpack_flip_y,
            GLboolean _unpack_premultiply_alpha,
            GLboolean _unpack_unmultiply_alpha) {
    SetHeader();
    source_id = _source_id;
    source_level = _source_level;
    dest_target = _dest_target;
    dest_id = _dest_id;
    dest_level = _dest_level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    unpack_flip_y = _unpack_flip_y;
    unpack_premultiply_alpha = _unpack_premultiply_alpha;
    unpack_unmultiply_alpha = _unpack_unmultiply_alpha;
  }

  void* Set(void* cmd,
            GLuint _source_id,
            GLint _source_level,
            GLenum _dest_target,
            GLuint _dest_id,
            GLint _dest_level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLboolean _unpack_flip_y,
            GLboolean _unpack_premultiply_alpha,
            GLboolean _unpack_unmultiply_alpha) {
    static_cast<ValueType*>(cmd)->Init(
        _source_id, _source_level, _dest_target, _dest_id, _dest_level,
        _xoffset, _yoffset, _x, _y, _width, _height, _unpack_flip_y,
        _unpack_premultiply_alpha, _unpack_unmultiply_alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t source_id;
  int32_t source_level;
  uint32_t dest_target;
  uint32_t dest_id;
  int32_t dest_level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  uint32_t unpack_flip_y;
  uint32_t unpack_premultiply_alpha;
  uint32_t unpack_unmultiply_alpha;
};

static_assert(sizeof(CopySubTextureCHROMIUM) == 60,
              "size of CopySubTextureCHROMIUM should be 60");
static_assert(offsetof(CopySubTextureCHROMIUM, header) == 0,
              "offset of CopySubTextureCHROMIUM header should be 0");
static_assert(offsetof(CopySubTextureCHROMIUM, source_id) == 4,
              "offset of CopySubTextureCHROMIUM source_id should be 4");
static_assert(offsetof(CopySubTextureCHROMIUM, source_level) == 8,
              "offset of CopySubTextureCHROMIUM source_level should be 8");
static_assert(offsetof(CopySubTextureCHROMIUM, dest_target) == 12,
              "offset of CopySubTextureCHROMIUM dest_target should be 12");
static_assert(offsetof(CopySubTextureCHROMIUM, dest_id) == 16,
              "offset of CopySubTextureCHROMIUM dest_id should be 16");
static_assert(offsetof(CopySubTextureCHROMIUM, dest_level) == 20,
              "offset of CopySubTextureCHROMIUM dest_level should be 20");
static_assert(offsetof(CopySubTextureCHROMIUM, xoffset) == 24,
              "offset of CopySubTextureCHROMIUM xoffset should be 24");
static_assert(offsetof(CopySubTextureCHROMIUM, yoffset) == 28,
              "offset of CopySubTextureCHROMIUM yoffset should be 28");
static_assert(offsetof(CopySubTextureCHROMIUM, x) == 32,
              "offset of CopySubTextureCHROMIUM x should be 32");
static_assert(offsetof(CopySubTextureCHROMIUM, y) == 36,
              "offset of CopySubTextureCHROMIUM y should be 36");
static_assert(offsetof(CopySubTextureCHROMIUM, width) == 40,
              "offset of CopySubTextureCHROMIUM width should be 40");
static_assert(offsetof(CopySubTextureCHROMIUM, height) == 44,
              "offset of CopySubTextureCHROMIUM height should be 44");
static_assert(offsetof(CopySubTextureCHROMIUM, unpack_flip_y) == 48,
              "offset of CopySubTextureCHROMIUM unpack_flip_y should be 48");
static_assert(
    offsetof(CopySubTextureCHROMIUM, unpack_premultiply_alpha) == 52,
    "offset of CopySubTextureCHROMIUM unpack_premultiply_alpha should be 52");
static_assert(
    offsetof(CopySubTextureCHROMIUM, unpack_unmultiply_alpha) == 56,
    "offset of CopySubTextureCHROMIUM unpack_unmultiply_alpha should be 56");

struct DrawArraysInstancedANGLE {
  typedef DrawArraysInstancedANGLE ValueType;
  static const CommandId kCmdId = kDrawArraysInstancedANGLE;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode, GLint _first, GLsizei _count, GLsizei _primcount) {
    SetHeader();
    mode = _mode;
    first = _first;
    count = _count;
    primcount = _primcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            GLint _first,
            GLsizei _count,
            GLsizei _primcount) {
    static_cast<ValueType*>(cmd)->Init(_mode, _first, _count, _primcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  int32_t first;
  int32_t count;
  int32_t primcount;
};

static_assert(sizeof(DrawArraysInstancedANGLE) == 20,
              "size of DrawArraysInstancedANGLE should be 20");
static_assert(offsetof(DrawArraysInstancedANGLE, header) == 0,
              "offset of DrawArraysInstancedANGLE header should be 0");
static_assert(offsetof(DrawArraysInstancedANGLE, mode) == 4,
              "offset of DrawArraysInstancedANGLE mode should be 4");
static_assert(offsetof(DrawArraysInstancedANGLE, first) == 8,
              "offset of DrawArraysInstancedANGLE first should be 8");
static_assert(offsetof(DrawArraysInstancedANGLE, count) == 12,
              "offset of DrawArraysInstancedANGLE count should be 12");
static_assert(offsetof(DrawArraysInstancedANGLE, primcount) == 16,
              "offset of DrawArraysInstancedANGLE primcount should be 16");

struct DrawArraysInstancedBaseInstanceANGLE {
  typedef DrawArraysInstancedBaseInstanceANGLE ValueType;
  static const CommandId kCmdId = kDrawArraysInstancedBaseInstanceANGLE;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            GLint _first,
            GLsizei _count,
            GLsizei _primcount,
            GLuint _baseinstance) {
    SetHeader();
    mode = _mode;
    first = _first;
    count = _count;
    primcount = _primcount;
    baseinstance = _baseinstance;
  }

  void* Set(void* cmd,
            GLenum _mode,
            GLint _first,
            GLsizei _count,
            GLsizei _primcount,
            GLuint _baseinstance) {
    static_cast<ValueType*>(cmd)->Init(_mode, _first, _count, _primcount,
                                       _baseinstance);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  int32_t first;
  int32_t count;
  int32_t primcount;
  uint32_t baseinstance;
};

static_assert(sizeof(DrawArraysInstancedBaseInstanceANGLE) == 24,
              "size of DrawArraysInstancedBaseInstanceANGLE should be 24");
static_assert(
    offsetof(DrawArraysInstancedBaseInstanceANGLE, header) == 0,
    "offset of DrawArraysInstancedBaseInstanceANGLE header should be 0");
static_assert(
    offsetof(DrawArraysInstancedBaseInstanceANGLE, mode) == 4,
    "offset of DrawArraysInstancedBaseInstanceANGLE mode should be 4");
static_assert(
    offsetof(DrawArraysInstancedBaseInstanceANGLE, first) == 8,
    "offset of DrawArraysInstancedBaseInstanceANGLE first should be 8");
static_assert(
    offsetof(DrawArraysInstancedBaseInstanceANGLE, count) == 12,
    "offset of DrawArraysInstancedBaseInstanceANGLE count should be 12");
static_assert(
    offsetof(DrawArraysInstancedBaseInstanceANGLE, primcount) == 16,
    "offset of DrawArraysInstancedBaseInstanceANGLE primcount should be 16");
static_assert(
    offsetof(DrawArraysInstancedBaseInstanceANGLE, baseinstance) == 20,
    "offset of DrawArraysInstancedBaseInstanceANGLE baseinstance should be 20");

struct DrawElementsInstancedANGLE {
  typedef DrawElementsInstancedANGLE ValueType;
  static const CommandId kCmdId = kDrawElementsInstancedANGLE;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            GLsizei _count,
            GLenum _type,
            GLuint _index_offset,
            GLsizei _primcount) {
    SetHeader();
    mode = _mode;
    count = _count;
    type = _type;
    index_offset = _index_offset;
    primcount = _primcount;
  }

  void* Set(void* cmd,
            GLenum _mode,
            GLsizei _count,
            GLenum _type,
            GLuint _index_offset,
            GLsizei _primcount) {
    static_cast<ValueType*>(cmd)->Init(_mode, _count, _type, _index_offset,
                                       _primcount);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  int32_t count;
  uint32_t type;
  uint32_t index_offset;
  int32_t primcount;
};

static_assert(sizeof(DrawElementsInstancedANGLE) == 24,
              "size of DrawElementsInstancedANGLE should be 24");
static_assert(offsetof(DrawElementsInstancedANGLE, header) == 0,
              "offset of DrawElementsInstancedANGLE header should be 0");
static_assert(offsetof(DrawElementsInstancedANGLE, mode) == 4,
              "offset of DrawElementsInstancedANGLE mode should be 4");
static_assert(offsetof(DrawElementsInstancedANGLE, count) == 8,
              "offset of DrawElementsInstancedANGLE count should be 8");
static_assert(offsetof(DrawElementsInstancedANGLE, type) == 12,
              "offset of DrawElementsInstancedANGLE type should be 12");
static_assert(offsetof(DrawElementsInstancedANGLE, index_offset) == 16,
              "offset of DrawElementsInstancedANGLE index_offset should be 16");
static_assert(offsetof(DrawElementsInstancedANGLE, primcount) == 20,
              "offset of DrawElementsInstancedANGLE primcount should be 20");

struct DrawElementsInstancedBaseVertexBaseInstanceANGLE {
  typedef DrawElementsInstancedBaseVertexBaseInstanceANGLE ValueType;
  static const CommandId kCmdId =
      kDrawElementsInstancedBaseVertexBaseInstanceANGLE;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _mode,
            GLsizei _count,
            GLenum _type,
            GLuint _index_offset,
            GLsizei _primcount,
            GLint _basevertex,
            GLuint _baseinstance) {
    SetHeader();
    mode = _mode;
    count = _count;
    type = _type;
    index_offset = _index_offset;
    primcount = _primcount;
    basevertex = _basevertex;
    baseinstance = _baseinstance;
  }

  void* Set(void* cmd,
            GLenum _mode,
            GLsizei _count,
            GLenum _type,
            GLuint _index_offset,
            GLsizei _primcount,
            GLint _basevertex,
            GLuint _baseinstance) {
    static_cast<ValueType*>(cmd)->Init(_mode, _count, _type, _index_offset,
                                       _primcount, _basevertex, _baseinstance);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  int32_t count;
  uint32_t type;
  uint32_t index_offset;
  int32_t primcount;
  int32_t basevertex;
  uint32_t baseinstance;
};

static_assert(
    sizeof(DrawElementsInstancedBaseVertexBaseInstanceANGLE) == 32,
    "size of DrawElementsInstancedBaseVertexBaseInstanceANGLE should be 32");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       header) == 0,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE "
              "header should be 0");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       mode) == 4,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE mode "
              "should be 4");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       count) == 8,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE "
              "count should be 8");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       type) == 12,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE type "
              "should be 12");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       index_offset) == 16,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE "
              "index_offset should be 16");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       primcount) == 20,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE "
              "primcount should be 20");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       basevertex) == 24,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE "
              "basevertex should be 24");
static_assert(offsetof(DrawElementsInstancedBaseVertexBaseInstanceANGLE,
                       baseinstance) == 28,
              "offset of DrawElementsInstancedBaseVertexBaseInstanceANGLE "
              "baseinstance should be 28");

struct VertexAttribDivisorANGLE {
  typedef VertexAttribDivisorANGLE ValueType;
  static const CommandId kCmdId = kVertexAttribDivisorANGLE;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _index, GLuint _divisor) {
    SetHeader();
    index = _index;
    divisor = _divisor;
  }

  void* Set(void* cmd, GLuint _index, GLuint _divisor) {
    static_cast<ValueType*>(cmd)->Init(_index, _divisor);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t index;
  uint32_t divisor;
};

static_assert(sizeof(VertexAttribDivisorANGLE) == 12,
              "size of VertexAttribDivisorANGLE should be 12");
static_assert(offsetof(VertexAttribDivisorANGLE, header) == 0,
              "offset of VertexAttribDivisorANGLE header should be 0");
static_assert(offsetof(VertexAttribDivisorANGLE, index) == 4,
              "offset of VertexAttribDivisorANGLE index should be 4");
static_assert(offsetof(VertexAttribDivisorANGLE, divisor) == 8,
              "offset of VertexAttribDivisorANGLE divisor should be 8");

struct ProduceTextureDirectCHROMIUMImmediate {
  typedef ProduceTextureDirectCHROMIUMImmediate ValueType;
  static const CommandId kCmdId = kProduceTextureDirectCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _texture, GLbyte* _mailbox) {
    SetHeader();
    texture = _texture;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _texture, GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(_texture, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t texture;
};

static_assert(sizeof(ProduceTextureDirectCHROMIUMImmediate) == 8,
              "size of ProduceTextureDirectCHROMIUMImmediate should be 8");
static_assert(
    offsetof(ProduceTextureDirectCHROMIUMImmediate, header) == 0,
    "offset of ProduceTextureDirectCHROMIUMImmediate header should be 0");
static_assert(
    offsetof(ProduceTextureDirectCHROMIUMImmediate, texture) == 4,
    "offset of ProduceTextureDirectCHROMIUMImmediate texture should be 4");

struct CreateAndConsumeTextureINTERNALImmediate {
  typedef CreateAndConsumeTextureINTERNALImmediate ValueType;
  static const CommandId kCmdId = kCreateAndConsumeTextureINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _texture, const GLbyte* _mailbox) {
    SetHeader();
    texture = _texture;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _texture, const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(_texture, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t texture;
};

static_assert(sizeof(CreateAndConsumeTextureINTERNALImmediate) == 8,
              "size of CreateAndConsumeTextureINTERNALImmediate should be 8");
static_assert(
    offsetof(CreateAndConsumeTextureINTERNALImmediate, header) == 0,
    "offset of CreateAndConsumeTextureINTERNALImmediate header should be 0");
static_assert(
    offsetof(CreateAndConsumeTextureINTERNALImmediate, texture) == 4,
    "offset of CreateAndConsumeTextureINTERNALImmediate texture should be 4");

struct BindUniformLocationCHROMIUMBucket {
  typedef BindUniformLocationCHROMIUMBucket ValueType;
  static const CommandId kCmdId = kBindUniformLocationCHROMIUMBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, GLint _location, uint32_t _name_bucket_id) {
    SetHeader();
    program = _program;
    location = _location;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLint _location,
            uint32_t _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _location, _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  int32_t location;
  uint32_t name_bucket_id;
};

static_assert(sizeof(BindUniformLocationCHROMIUMBucket) == 16,
              "size of BindUniformLocationCHROMIUMBucket should be 16");
static_assert(offsetof(BindUniformLocationCHROMIUMBucket, header) == 0,
              "offset of BindUniformLocationCHROMIUMBucket header should be 0");
static_assert(
    offsetof(BindUniformLocationCHROMIUMBucket, program) == 4,
    "offset of BindUniformLocationCHROMIUMBucket program should be 4");
static_assert(
    offsetof(BindUniformLocationCHROMIUMBucket, location) == 8,
    "offset of BindUniformLocationCHROMIUMBucket location should be 8");
static_assert(
    offsetof(BindUniformLocationCHROMIUMBucket, name_bucket_id) == 12,
    "offset of BindUniformLocationCHROMIUMBucket name_bucket_id should be 12");

struct BindTexImage2DCHROMIUM {
  typedef BindTexImage2DCHROMIUM ValueType;
  static const CommandId kCmdId = kBindTexImage2DCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLint _imageId) {
    SetHeader();
    target = _target;
    imageId = _imageId;
  }

  void* Set(void* cmd, GLenum _target, GLint _imageId) {
    static_cast<ValueType*>(cmd)->Init(_target, _imageId);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t imageId;
};

static_assert(sizeof(BindTexImage2DCHROMIUM) == 12,
              "size of BindTexImage2DCHROMIUM should be 12");
static_assert(offsetof(BindTexImage2DCHROMIUM, header) == 0,
              "offset of BindTexImage2DCHROMIUM header should be 0");
static_assert(offsetof(BindTexImage2DCHROMIUM, target) == 4,
              "offset of BindTexImage2DCHROMIUM target should be 4");
static_assert(offsetof(BindTexImage2DCHROMIUM, imageId) == 8,
              "offset of BindTexImage2DCHROMIUM imageId should be 8");

struct BindTexImage2DWithInternalformatCHROMIUM {
  typedef BindTexImage2DWithInternalformatCHROMIUM ValueType;
  static const CommandId kCmdId = kBindTexImage2DWithInternalformatCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLenum _internalformat, GLint _imageId) {
    SetHeader();
    target = _target;
    internalformat = _internalformat;
    imageId = _imageId;
  }

  void* Set(void* cmd, GLenum _target, GLenum _internalformat, GLint _imageId) {
    static_cast<ValueType*>(cmd)->Init(_target, _internalformat, _imageId);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t internalformat;
  int32_t imageId;
};

static_assert(sizeof(BindTexImage2DWithInternalformatCHROMIUM) == 16,
              "size of BindTexImage2DWithInternalformatCHROMIUM should be 16");
static_assert(
    offsetof(BindTexImage2DWithInternalformatCHROMIUM, header) == 0,
    "offset of BindTexImage2DWithInternalformatCHROMIUM header should be 0");
static_assert(
    offsetof(BindTexImage2DWithInternalformatCHROMIUM, target) == 4,
    "offset of BindTexImage2DWithInternalformatCHROMIUM target should be 4");
static_assert(offsetof(BindTexImage2DWithInternalformatCHROMIUM,
                       internalformat) == 8,
              "offset of BindTexImage2DWithInternalformatCHROMIUM "
              "internalformat should be 8");
static_assert(
    offsetof(BindTexImage2DWithInternalformatCHROMIUM, imageId) == 12,
    "offset of BindTexImage2DWithInternalformatCHROMIUM imageId should be 12");

struct ReleaseTexImage2DCHROMIUM {
  typedef ReleaseTexImage2DCHROMIUM ValueType;
  static const CommandId kCmdId = kReleaseTexImage2DCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLint _imageId) {
    SetHeader();
    target = _target;
    imageId = _imageId;
  }

  void* Set(void* cmd, GLenum _target, GLint _imageId) {
    static_cast<ValueType*>(cmd)->Init(_target, _imageId);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t imageId;
};

static_assert(sizeof(ReleaseTexImage2DCHROMIUM) == 12,
              "size of ReleaseTexImage2DCHROMIUM should be 12");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, header) == 0,
              "offset of ReleaseTexImage2DCHROMIUM header should be 0");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, target) == 4,
              "offset of ReleaseTexImage2DCHROMIUM target should be 4");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, imageId) == 8,
              "offset of ReleaseTexImage2DCHROMIUM imageId should be 8");

struct TraceBeginCHROMIUM {
  typedef TraceBeginCHROMIUM ValueType;
  static const CommandId kCmdId = kTraceBeginCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _category_bucket_id, GLuint _name_bucket_id) {
    SetHeader();
    category_bucket_id = _category_bucket_id;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd, GLuint _category_bucket_id, GLuint _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_category_bucket_id, _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t category_bucket_id;
  uint32_t name_bucket_id;
};

static_assert(sizeof(TraceBeginCHROMIUM) == 12,
              "size of TraceBeginCHROMIUM should be 12");
static_assert(offsetof(TraceBeginCHROMIUM, header) == 0,
              "offset of TraceBeginCHROMIUM header should be 0");
static_assert(offsetof(TraceBeginCHROMIUM, category_bucket_id) == 4,
              "offset of TraceBeginCHROMIUM category_bucket_id should be 4");
static_assert(offsetof(TraceBeginCHROMIUM, name_bucket_id) == 8,
              "offset of TraceBeginCHROMIUM name_bucket_id should be 8");

struct TraceEndCHROMIUM {
  typedef TraceEndCHROMIUM ValueType;
  static const CommandId kCmdId = kTraceEndCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(TraceEndCHROMIUM) == 4,
              "size of TraceEndCHROMIUM should be 4");
static_assert(offsetof(TraceEndCHROMIUM, header) == 0,
              "offset of TraceEndCHROMIUM header should be 0");

struct DiscardFramebufferEXTImmediate {
  typedef DiscardFramebufferEXTImmediate ValueType;
  static const CommandId kCmdId = kDiscardFramebufferEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLenum) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLenum _target, GLsizei _count, const GLenum* _attachments) {
    SetHeader(_count);
    target = _target;
    count = _count;
    memcpy(ImmediateDataAddress(this), _attachments, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _count,
            const GLenum* _attachments) {
    static_cast<ValueType*>(cmd)->Init(_target, _count, _attachments);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t count;
};

static_assert(sizeof(DiscardFramebufferEXTImmediate) == 12,
              "size of DiscardFramebufferEXTImmediate should be 12");
static_assert(offsetof(DiscardFramebufferEXTImmediate, header) == 0,
              "offset of DiscardFramebufferEXTImmediate header should be 0");
static_assert(offsetof(DiscardFramebufferEXTImmediate, target) == 4,
              "offset of DiscardFramebufferEXTImmediate target should be 4");
static_assert(offsetof(DiscardFramebufferEXTImmediate, count) == 8,
              "offset of DiscardFramebufferEXTImmediate count should be 8");

struct LoseContextCHROMIUM {
  typedef LoseContextCHROMIUM ValueType;
  static const CommandId kCmdId = kLoseContextCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _current, GLenum _other) {
    SetHeader();
    current = _current;
    other = _other;
  }

  void* Set(void* cmd, GLenum _current, GLenum _other) {
    static_cast<ValueType*>(cmd)->Init(_current, _other);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t current;
  uint32_t other;
};

static_assert(sizeof(LoseContextCHROMIUM) == 12,
              "size of LoseContextCHROMIUM should be 12");
static_assert(offsetof(LoseContextCHROMIUM, header) == 0,
              "offset of LoseContextCHROMIUM header should be 0");
static_assert(offsetof(LoseContextCHROMIUM, current) == 4,
              "offset of LoseContextCHROMIUM current should be 4");
static_assert(offsetof(LoseContextCHROMIUM, other) == 8,
              "offset of LoseContextCHROMIUM other should be 8");

struct UnpremultiplyAndDitherCopyCHROMIUM {
  typedef UnpremultiplyAndDitherCopyCHROMIUM ValueType;
  static const CommandId kCmdId = kUnpremultiplyAndDitherCopyCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _source_id,
            GLuint _dest_id,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    source_id = _source_id;
    dest_id = _dest_id;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLuint _source_id,
            GLuint _dest_id,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_source_id, _dest_id, _x, _y, _width,
                                       _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t source_id;
  uint32_t dest_id;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(UnpremultiplyAndDitherCopyCHROMIUM) == 28,
              "size of UnpremultiplyAndDitherCopyCHROMIUM should be 28");
static_assert(
    offsetof(UnpremultiplyAndDitherCopyCHROMIUM, header) == 0,
    "offset of UnpremultiplyAndDitherCopyCHROMIUM header should be 0");
static_assert(
    offsetof(UnpremultiplyAndDitherCopyCHROMIUM, source_id) == 4,
    "offset of UnpremultiplyAndDitherCopyCHROMIUM source_id should be 4");
static_assert(
    offsetof(UnpremultiplyAndDitherCopyCHROMIUM, dest_id) == 8,
    "offset of UnpremultiplyAndDitherCopyCHROMIUM dest_id should be 8");
static_assert(offsetof(UnpremultiplyAndDitherCopyCHROMIUM, x) == 12,
              "offset of UnpremultiplyAndDitherCopyCHROMIUM x should be 12");
static_assert(offsetof(UnpremultiplyAndDitherCopyCHROMIUM, y) == 16,
              "offset of UnpremultiplyAndDitherCopyCHROMIUM y should be 16");
static_assert(
    offsetof(UnpremultiplyAndDitherCopyCHROMIUM, width) == 20,
    "offset of UnpremultiplyAndDitherCopyCHROMIUM width should be 20");
static_assert(
    offsetof(UnpremultiplyAndDitherCopyCHROMIUM, height) == 24,
    "offset of UnpremultiplyAndDitherCopyCHROMIUM height should be 24");

struct DrawBuffersEXTImmediate {
  typedef DrawBuffersEXTImmediate ValueType;
  static const CommandId kCmdId = kDrawBuffersEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLenum) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _count, const GLenum* _bufs) {
    SetHeader(_count);
    count = _count;
    memcpy(ImmediateDataAddress(this), _bufs, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLsizei _count, const GLenum* _bufs) {
    static_cast<ValueType*>(cmd)->Init(_count, _bufs);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t count;
};

static_assert(sizeof(DrawBuffersEXTImmediate) == 8,
              "size of DrawBuffersEXTImmediate should be 8");
static_assert(offsetof(DrawBuffersEXTImmediate, header) == 0,
              "offset of DrawBuffersEXTImmediate header should be 0");
static_assert(offsetof(DrawBuffersEXTImmediate, count) == 4,
              "offset of DrawBuffersEXTImmediate count should be 4");

struct DiscardBackbufferCHROMIUM {
  typedef DiscardBackbufferCHROMIUM ValueType;
  static const CommandId kCmdId = kDiscardBackbufferCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(DiscardBackbufferCHROMIUM) == 4,
              "size of DiscardBackbufferCHROMIUM should be 4");
static_assert(offsetof(DiscardBackbufferCHROMIUM, header) == 0,
              "offset of DiscardBackbufferCHROMIUM header should be 0");

struct ScheduleOverlayPlaneCHROMIUM {
  typedef ScheduleOverlayPlaneCHROMIUM ValueType;
  static const CommandId kCmdId = kScheduleOverlayPlaneCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _plane_z_order,
            GLenum _plane_transform,
            GLuint _overlay_texture_id,
            GLint _bounds_x,
            GLint _bounds_y,
            GLint _bounds_width,
            GLint _bounds_height,
            GLfloat _uv_x,
            GLfloat _uv_y,
            GLfloat _uv_width,
            GLfloat _uv_height,
            GLboolean _enable_blend,
            GLuint _gpu_fence_id) {
    SetHeader();
    plane_z_order = _plane_z_order;
    plane_transform = _plane_transform;
    overlay_texture_id = _overlay_texture_id;
    bounds_x = _bounds_x;
    bounds_y = _bounds_y;
    bounds_width = _bounds_width;
    bounds_height = _bounds_height;
    uv_x = _uv_x;
    uv_y = _uv_y;
    uv_width = _uv_width;
    uv_height = _uv_height;
    enable_blend = _enable_blend;
    gpu_fence_id = _gpu_fence_id;
  }

  void* Set(void* cmd,
            GLint _plane_z_order,
            GLenum _plane_transform,
            GLuint _overlay_texture_id,
            GLint _bounds_x,
            GLint _bounds_y,
            GLint _bounds_width,
            GLint _bounds_height,
            GLfloat _uv_x,
            GLfloat _uv_y,
            GLfloat _uv_width,
            GLfloat _uv_height,
            GLboolean _enable_blend,
            GLuint _gpu_fence_id) {
    static_cast<ValueType*>(cmd)->Init(
        _plane_z_order, _plane_transform, _overlay_texture_id, _bounds_x,
        _bounds_y, _bounds_width, _bounds_height, _uv_x, _uv_y, _uv_width,
        _uv_height, _enable_blend, _gpu_fence_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t plane_z_order;
  uint32_t plane_transform;
  uint32_t overlay_texture_id;
  int32_t bounds_x;
  int32_t bounds_y;
  int32_t bounds_width;
  int32_t bounds_height;
  float uv_x;
  float uv_y;
  float uv_width;
  float uv_height;
  uint32_t enable_blend;
  uint32_t gpu_fence_id;
};

static_assert(sizeof(ScheduleOverlayPlaneCHROMIUM) == 56,
              "size of ScheduleOverlayPlaneCHROMIUM should be 56");
static_assert(offsetof(ScheduleOverlayPlaneCHROMIUM, header) == 0,
              "offset of ScheduleOverlayPlaneCHROMIUM header should be 0");
static_assert(
    offsetof(ScheduleOverlayPlaneCHROMIUM, plane_z_order) == 4,
    "offset of ScheduleOverlayPlaneCHROMIUM plane_z_order should be 4");
static_assert(
    offsetof(ScheduleOverlayPlaneCHROMIUM, plane_transform) == 8,
    "offset of ScheduleOverlayPlaneCHROMIUM plane_transform should be 8");
static_assert(
    offsetof(ScheduleOverlayPlaneCHROMIUM, overlay_texture_id) == 12,
    "offset of ScheduleOverlayPlaneCHROMIUM overlay_texture_id should be 12");
static_assert(offsetof(ScheduleOverlayPlaneCHROMIUM, bounds_x) == 16,
              "offset of ScheduleOverlayPlaneCHROMIUM bounds_x should be 16");
static_assert(offsetof(ScheduleOverlayPlaneCHROMIUM, bounds_y) == 20,
              "offset of ScheduleOverlayPlaneCHROMIUM bounds_y should be 20");
static_assert(
    offsetof(ScheduleOverlayPlaneCHROMIUM, bounds_width) == 24,
    "offset of ScheduleOverlayPlaneCHROMIUM bounds_width should be 24");
static_assert(
    offsetof(ScheduleOverlayPlaneCHROMIUM, bounds_height) == 28,
    "offset of ScheduleOverlayPlaneCHROMIUM bounds_height should be 28");
static_assert(offsetof(ScheduleOverlayPlaneCHROMIUM, uv_x) == 32,
              "offset of ScheduleOverlayPlaneCHROMIUM uv_x should be 32");
static_assert(offsetof(ScheduleOverlayPlaneCHROMIUM, uv_y) == 36,
              "offset of ScheduleOverlayPlaneCHROMIUM uv_y should be 36");
static_assert(offsetof(ScheduleOverlayPlaneCHROMIUM, uv_width) == 40,
              "offset of ScheduleOverlayPlaneCHROMIUM uv_width should be 40");
static_assert(offsetof(ScheduleOverlayPlaneCHROMIUM, uv_height) == 44,
              "offset of ScheduleOverlayPlaneCHROMIUM uv_height should be 44");
static_assert(
    offsetof(ScheduleOverlayPlaneCHROMIUM, enable_blend) == 48,
    "offset of ScheduleOverlayPlaneCHROMIUM enable_blend should be 48");
static_assert(
    offsetof(ScheduleOverlayPlaneCHROMIUM, gpu_fence_id) == 52,
    "offset of ScheduleOverlayPlaneCHROMIUM gpu_fence_id should be 52");

struct ScheduleCALayerSharedStateCHROMIUM {
  typedef ScheduleCALayerSharedStateCHROMIUM ValueType;
  static const CommandId kCmdId = kScheduleCALayerSharedStateCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLfloat _opacity,
            GLboolean _is_clipped,
            GLint _sorting_context_id,
            GLuint _shm_id,
            GLuint _shm_offset) {
    SetHeader();
    opacity = _opacity;
    is_clipped = _is_clipped;
    sorting_context_id = _sorting_context_id;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
  }

  void* Set(void* cmd,
            GLfloat _opacity,
            GLboolean _is_clipped,
            GLint _sorting_context_id,
            GLuint _shm_id,
            GLuint _shm_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _opacity, _is_clipped, _sorting_context_id, _shm_id, _shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  float opacity;
  uint32_t is_clipped;
  int32_t sorting_context_id;
  uint32_t shm_id;
  uint32_t shm_offset;
};

static_assert(sizeof(ScheduleCALayerSharedStateCHROMIUM) == 24,
              "size of ScheduleCALayerSharedStateCHROMIUM should be 24");
static_assert(
    offsetof(ScheduleCALayerSharedStateCHROMIUM, header) == 0,
    "offset of ScheduleCALayerSharedStateCHROMIUM header should be 0");
static_assert(
    offsetof(ScheduleCALayerSharedStateCHROMIUM, opacity) == 4,
    "offset of ScheduleCALayerSharedStateCHROMIUM opacity should be 4");
static_assert(
    offsetof(ScheduleCALayerSharedStateCHROMIUM, is_clipped) == 8,
    "offset of ScheduleCALayerSharedStateCHROMIUM is_clipped should be 8");
static_assert(offsetof(ScheduleCALayerSharedStateCHROMIUM,
                       sorting_context_id) == 12,
              "offset of ScheduleCALayerSharedStateCHROMIUM sorting_context_id "
              "should be 12");
static_assert(
    offsetof(ScheduleCALayerSharedStateCHROMIUM, shm_id) == 16,
    "offset of ScheduleCALayerSharedStateCHROMIUM shm_id should be 16");
static_assert(
    offsetof(ScheduleCALayerSharedStateCHROMIUM, shm_offset) == 20,
    "offset of ScheduleCALayerSharedStateCHROMIUM shm_offset should be 20");

struct ScheduleCALayerCHROMIUM {
  typedef ScheduleCALayerCHROMIUM ValueType;
  static const CommandId kCmdId = kScheduleCALayerCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _contents_texture_id,
            GLuint _background_color,
            GLuint _edge_aa_mask,
            GLuint _filter,
            GLuint _shm_id,
            GLuint _shm_offset) {
    SetHeader();
    contents_texture_id = _contents_texture_id;
    background_color = _background_color;
    edge_aa_mask = _edge_aa_mask;
    filter = _filter;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
  }

  void* Set(void* cmd,
            GLuint _contents_texture_id,
            GLuint _background_color,
            GLuint _edge_aa_mask,
            GLuint _filter,
            GLuint _shm_id,
            GLuint _shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_contents_texture_id, _background_color,
                                       _edge_aa_mask, _filter, _shm_id,
                                       _shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t contents_texture_id;
  uint32_t background_color;
  uint32_t edge_aa_mask;
  uint32_t filter;
  uint32_t shm_id;
  uint32_t shm_offset;
};

static_assert(sizeof(ScheduleCALayerCHROMIUM) == 28,
              "size of ScheduleCALayerCHROMIUM should be 28");
static_assert(offsetof(ScheduleCALayerCHROMIUM, header) == 0,
              "offset of ScheduleCALayerCHROMIUM header should be 0");
static_assert(
    offsetof(ScheduleCALayerCHROMIUM, contents_texture_id) == 4,
    "offset of ScheduleCALayerCHROMIUM contents_texture_id should be 4");
static_assert(offsetof(ScheduleCALayerCHROMIUM, background_color) == 8,
              "offset of ScheduleCALayerCHROMIUM background_color should be 8");
static_assert(offsetof(ScheduleCALayerCHROMIUM, edge_aa_mask) == 12,
              "offset of ScheduleCALayerCHROMIUM edge_aa_mask should be 12");
static_assert(offsetof(ScheduleCALayerCHROMIUM, filter) == 16,
              "offset of ScheduleCALayerCHROMIUM filter should be 16");
static_assert(offsetof(ScheduleCALayerCHROMIUM, shm_id) == 20,
              "offset of ScheduleCALayerCHROMIUM shm_id should be 20");
static_assert(offsetof(ScheduleCALayerCHROMIUM, shm_offset) == 24,
              "offset of ScheduleCALayerCHROMIUM shm_offset should be 24");

struct ScheduleCALayerInUseQueryCHROMIUMImmediate {
  typedef ScheduleCALayerInUseQueryCHROMIUMImmediate ValueType;
  static const CommandId kCmdId = kScheduleCALayerInUseQueryCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _count, const GLuint* _textures) {
    SetHeader(_count);
    count = _count;
    memcpy(ImmediateDataAddress(this), _textures, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLsizei _count, const GLuint* _textures) {
    static_cast<ValueType*>(cmd)->Init(_count, _textures);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t count;
};

static_assert(sizeof(ScheduleCALayerInUseQueryCHROMIUMImmediate) == 8,
              "size of ScheduleCALayerInUseQueryCHROMIUMImmediate should be 8");
static_assert(
    offsetof(ScheduleCALayerInUseQueryCHROMIUMImmediate, header) == 0,
    "offset of ScheduleCALayerInUseQueryCHROMIUMImmediate header should be 0");
static_assert(
    offsetof(ScheduleCALayerInUseQueryCHROMIUMImmediate, count) == 4,
    "offset of ScheduleCALayerInUseQueryCHROMIUMImmediate count should be 4");

struct CommitOverlayPlanesCHROMIUM {
  typedef CommitOverlayPlanesCHROMIUM ValueType;
  static const CommandId kCmdId = kCommitOverlayPlanesCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint64 _swap_id, GLbitfield _flags) {
    SetHeader();
    GLES2Util::MapUint64ToTwoUint32(static_cast<uint64_t>(_swap_id), &swap_id_0,
                                    &swap_id_1);
    flags = _flags;
  }

  void* Set(void* cmd, GLuint64 _swap_id, GLbitfield _flags) {
    static_cast<ValueType*>(cmd)->Init(_swap_id, _flags);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 swap_id() const volatile {
    return static_cast<GLuint64>(
        GLES2Util::MapTwoUint32ToUint64(swap_id_0, swap_id_1));
  }

  gpu::CommandHeader header;
  uint32_t swap_id_0;
  uint32_t swap_id_1;
  uint32_t flags;
};

static_assert(sizeof(CommitOverlayPlanesCHROMIUM) == 16,
              "size of CommitOverlayPlanesCHROMIUM should be 16");
static_assert(offsetof(CommitOverlayPlanesCHROMIUM, header) == 0,
              "offset of CommitOverlayPlanesCHROMIUM header should be 0");
static_assert(offsetof(CommitOverlayPlanesCHROMIUM, swap_id_0) == 4,
              "offset of CommitOverlayPlanesCHROMIUM swap_id_0 should be 4");
static_assert(offsetof(CommitOverlayPlanesCHROMIUM, swap_id_1) == 8,
              "offset of CommitOverlayPlanesCHROMIUM swap_id_1 should be 8");
static_assert(offsetof(CommitOverlayPlanesCHROMIUM, flags) == 12,
              "offset of CommitOverlayPlanesCHROMIUM flags should be 12");

struct FlushDriverCachesCHROMIUM {
  typedef FlushDriverCachesCHROMIUM ValueType;
  static const CommandId kCmdId = kFlushDriverCachesCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(FlushDriverCachesCHROMIUM) == 4,
              "size of FlushDriverCachesCHROMIUM should be 4");
static_assert(offsetof(FlushDriverCachesCHROMIUM, header) == 0,
              "offset of FlushDriverCachesCHROMIUM header should be 0");

struct ScheduleDCLayerCHROMIUM {
  typedef ScheduleDCLayerCHROMIUM ValueType;
  static const CommandId kCmdId = kScheduleDCLayerCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_0,
            GLuint _texture_1,
            GLint _z_order,
            GLint _content_x,
            GLint _content_y,
            GLint _content_width,
            GLint _content_height,
            GLint _quad_x,
            GLint _quad_y,
            GLint _quad_width,
            GLint _quad_height,
            GLfloat _transform_c1r1,
            GLfloat _transform_c2r1,
            GLfloat _transform_c1r2,
            GLfloat _transform_c2r2,
            GLfloat _transform_tx,
            GLfloat _transform_ty,
            GLboolean _is_clipped,
            GLint _clip_x,
            GLint _clip_y,
            GLint _clip_width,
            GLint _clip_height,
            GLuint _protected_video_type) {
    SetHeader();
    texture_0 = _texture_0;
    texture_1 = _texture_1;
    z_order = _z_order;
    content_x = _content_x;
    content_y = _content_y;
    content_width = _content_width;
    content_height = _content_height;
    quad_x = _quad_x;
    quad_y = _quad_y;
    quad_width = _quad_width;
    quad_height = _quad_height;
    transform_c1r1 = _transform_c1r1;
    transform_c2r1 = _transform_c2r1;
    transform_c1r2 = _transform_c1r2;
    transform_c2r2 = _transform_c2r2;
    transform_tx = _transform_tx;
    transform_ty = _transform_ty;
    is_clipped = _is_clipped;
    clip_x = _clip_x;
    clip_y = _clip_y;
    clip_width = _clip_width;
    clip_height = _clip_height;
    protected_video_type = _protected_video_type;
  }

  void* Set(void* cmd,
            GLuint _texture_0,
            GLuint _texture_1,
            GLint _z_order,
            GLint _content_x,
            GLint _content_y,
            GLint _content_width,
            GLint _content_height,
            GLint _quad_x,
            GLint _quad_y,
            GLint _quad_width,
            GLint _quad_height,
            GLfloat _transform_c1r1,
            GLfloat _transform_c2r1,
            GLfloat _transform_c1r2,
            GLfloat _transform_c2r2,
            GLfloat _transform_tx,
            GLfloat _transform_ty,
            GLboolean _is_clipped,
            GLint _clip_x,
            GLint _clip_y,
            GLint _clip_width,
            GLint _clip_height,
            GLuint _protected_video_type) {
    static_cast<ValueType*>(cmd)->Init(
        _texture_0, _texture_1, _z_order, _content_x, _content_y,
        _content_width, _content_height, _quad_x, _quad_y, _quad_width,
        _quad_height, _transform_c1r1, _transform_c2r1, _transform_c1r2,
        _transform_c2r2, _transform_tx, _transform_ty, _is_clipped, _clip_x,
        _clip_y, _clip_width, _clip_height, _protected_video_type);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_0;
  uint32_t texture_1;
  int32_t z_order;
  int32_t content_x;
  int32_t content_y;
  int32_t content_width;
  int32_t content_height;
  int32_t quad_x;
  int32_t quad_y;
  int32_t quad_width;
  int32_t quad_height;
  float transform_c1r1;
  float transform_c2r1;
  float transform_c1r2;
  float transform_c2r2;
  float transform_tx;
  float transform_ty;
  uint32_t is_clipped;
  int32_t clip_x;
  int32_t clip_y;
  int32_t clip_width;
  int32_t clip_height;
  uint32_t protected_video_type;
};

static_assert(sizeof(ScheduleDCLayerCHROMIUM) == 96,
              "size of ScheduleDCLayerCHROMIUM should be 96");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, header) == 0,
              "offset of ScheduleDCLayerCHROMIUM header should be 0");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, texture_0) == 4,
              "offset of ScheduleDCLayerCHROMIUM texture_0 should be 4");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, texture_1) == 8,
              "offset of ScheduleDCLayerCHROMIUM texture_1 should be 8");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, z_order) == 12,
              "offset of ScheduleDCLayerCHROMIUM z_order should be 12");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, content_x) == 16,
              "offset of ScheduleDCLayerCHROMIUM content_x should be 16");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, content_y) == 20,
              "offset of ScheduleDCLayerCHROMIUM content_y should be 20");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, content_width) == 24,
              "offset of ScheduleDCLayerCHROMIUM content_width should be 24");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, content_height) == 28,
              "offset of ScheduleDCLayerCHROMIUM content_height should be 28");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, quad_x) == 32,
              "offset of ScheduleDCLayerCHROMIUM quad_x should be 32");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, quad_y) == 36,
              "offset of ScheduleDCLayerCHROMIUM quad_y should be 36");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, quad_width) == 40,
              "offset of ScheduleDCLayerCHROMIUM quad_width should be 40");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, quad_height) == 44,
              "offset of ScheduleDCLayerCHROMIUM quad_height should be 44");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, transform_c1r1) == 48,
              "offset of ScheduleDCLayerCHROMIUM transform_c1r1 should be 48");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, transform_c2r1) == 52,
              "offset of ScheduleDCLayerCHROMIUM transform_c2r1 should be 52");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, transform_c1r2) == 56,
              "offset of ScheduleDCLayerCHROMIUM transform_c1r2 should be 56");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, transform_c2r2) == 60,
              "offset of ScheduleDCLayerCHROMIUM transform_c2r2 should be 60");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, transform_tx) == 64,
              "offset of ScheduleDCLayerCHROMIUM transform_tx should be 64");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, transform_ty) == 68,
              "offset of ScheduleDCLayerCHROMIUM transform_ty should be 68");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, is_clipped) == 72,
              "offset of ScheduleDCLayerCHROMIUM is_clipped should be 72");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, clip_x) == 76,
              "offset of ScheduleDCLayerCHROMIUM clip_x should be 76");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, clip_y) == 80,
              "offset of ScheduleDCLayerCHROMIUM clip_y should be 80");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, clip_width) == 84,
              "offset of ScheduleDCLayerCHROMIUM clip_width should be 84");
static_assert(offsetof(ScheduleDCLayerCHROMIUM, clip_height) == 88,
              "offset of ScheduleDCLayerCHROMIUM clip_height should be 88");
static_assert(
    offsetof(ScheduleDCLayerCHROMIUM, protected_video_type) == 92,
    "offset of ScheduleDCLayerCHROMIUM protected_video_type should be 92");

struct SetActiveURLCHROMIUM {
  typedef SetActiveURLCHROMIUM ValueType;
  static const CommandId kCmdId = kSetActiveURLCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _url_bucket_id) {
    SetHeader();
    url_bucket_id = _url_bucket_id;
  }

  void* Set(void* cmd, GLuint _url_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_url_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t url_bucket_id;
};

static_assert(sizeof(SetActiveURLCHROMIUM) == 8,
              "size of SetActiveURLCHROMIUM should be 8");
static_assert(offsetof(SetActiveURLCHROMIUM, header) == 0,
              "offset of SetActiveURLCHROMIUM header should be 0");
static_assert(offsetof(SetActiveURLCHROMIUM, url_bucket_id) == 4,
              "offset of SetActiveURLCHROMIUM url_bucket_id should be 4");

struct MatrixLoadfCHROMIUMImmediate {
  typedef MatrixLoadfCHROMIUMImmediate ValueType;
  static const CommandId kCmdId = kMatrixLoadfCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLenum _matrixMode, const GLfloat* _m) {
    SetHeader();
    matrixMode = _matrixMode;
    memcpy(ImmediateDataAddress(this), _m, ComputeDataSize());
  }

  void* Set(void* cmd, GLenum _matrixMode, const GLfloat* _m) {
    static_cast<ValueType*>(cmd)->Init(_matrixMode, _m);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t matrixMode;
};

static_assert(sizeof(MatrixLoadfCHROMIUMImmediate) == 8,
              "size of MatrixLoadfCHROMIUMImmediate should be 8");
static_assert(offsetof(MatrixLoadfCHROMIUMImmediate, header) == 0,
              "offset of MatrixLoadfCHROMIUMImmediate header should be 0");
static_assert(offsetof(MatrixLoadfCHROMIUMImmediate, matrixMode) == 4,
              "offset of MatrixLoadfCHROMIUMImmediate matrixMode should be 4");

struct MatrixLoadIdentityCHROMIUM {
  typedef MatrixLoadIdentityCHROMIUM ValueType;
  static const CommandId kCmdId = kMatrixLoadIdentityCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _matrixMode) {
    SetHeader();
    matrixMode = _matrixMode;
  }

  void* Set(void* cmd, GLenum _matrixMode) {
    static_cast<ValueType*>(cmd)->Init(_matrixMode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t matrixMode;
};

static_assert(sizeof(MatrixLoadIdentityCHROMIUM) == 8,
              "size of MatrixLoadIdentityCHROMIUM should be 8");
static_assert(offsetof(MatrixLoadIdentityCHROMIUM, header) == 0,
              "offset of MatrixLoadIdentityCHROMIUM header should be 0");
static_assert(offsetof(MatrixLoadIdentityCHROMIUM, matrixMode) == 4,
              "offset of MatrixLoadIdentityCHROMIUM matrixMode should be 4");

struct GenPathsCHROMIUM {
  typedef GenPathsCHROMIUM ValueType;
  static const CommandId kCmdId = kGenPathsCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _first_client_id, GLsizei _range) {
    SetHeader();
    first_client_id = _first_client_id;
    range = _range;
  }

  void* Set(void* cmd, GLuint _first_client_id, GLsizei _range) {
    static_cast<ValueType*>(cmd)->Init(_first_client_id, _range);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t first_client_id;
  int32_t range;
};

static_assert(sizeof(GenPathsCHROMIUM) == 12,
              "size of GenPathsCHROMIUM should be 12");
static_assert(offsetof(GenPathsCHROMIUM, header) == 0,
              "offset of GenPathsCHROMIUM header should be 0");
static_assert(offsetof(GenPathsCHROMIUM, first_client_id) == 4,
              "offset of GenPathsCHROMIUM first_client_id should be 4");
static_assert(offsetof(GenPathsCHROMIUM, range) == 8,
              "offset of GenPathsCHROMIUM range should be 8");

struct DeletePathsCHROMIUM {
  typedef DeletePathsCHROMIUM ValueType;
  static const CommandId kCmdId = kDeletePathsCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _first_client_id, GLsizei _range) {
    SetHeader();
    first_client_id = _first_client_id;
    range = _range;
  }

  void* Set(void* cmd, GLuint _first_client_id, GLsizei _range) {
    static_cast<ValueType*>(cmd)->Init(_first_client_id, _range);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t first_client_id;
  int32_t range;
};

static_assert(sizeof(DeletePathsCHROMIUM) == 12,
              "size of DeletePathsCHROMIUM should be 12");
static_assert(offsetof(DeletePathsCHROMIUM, header) == 0,
              "offset of DeletePathsCHROMIUM header should be 0");
static_assert(offsetof(DeletePathsCHROMIUM, first_client_id) == 4,
              "offset of DeletePathsCHROMIUM first_client_id should be 4");
static_assert(offsetof(DeletePathsCHROMIUM, range) == 8,
              "offset of DeletePathsCHROMIUM range should be 8");

struct IsPathCHROMIUM {
  typedef IsPathCHROMIUM ValueType;
  static const CommandId kCmdId = kIsPathCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef uint32_t Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    SetHeader();
    path = _path;
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _path,
            uint32_t _result_shm_id,
            uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_path, _result_shm_id,
                                       _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(IsPathCHROMIUM) == 16,
              "size of IsPathCHROMIUM should be 16");
static_assert(offsetof(IsPathCHROMIUM, header) == 0,
              "offset of IsPathCHROMIUM header should be 0");
static_assert(offsetof(IsPathCHROMIUM, path) == 4,
              "offset of IsPathCHROMIUM path should be 4");
static_assert(offsetof(IsPathCHROMIUM, result_shm_id) == 8,
              "offset of IsPathCHROMIUM result_shm_id should be 8");
static_assert(offsetof(IsPathCHROMIUM, result_shm_offset) == 12,
              "offset of IsPathCHROMIUM result_shm_offset should be 12");

struct PathCommandsCHROMIUM {
  typedef PathCommandsCHROMIUM ValueType;
  static const CommandId kCmdId = kPathCommandsCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path,
            GLsizei _numCommands,
            uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            GLsizei _numCoords,
            GLenum _coordType,
            uint32_t _coords_shm_id,
            uint32_t _coords_shm_offset) {
    SetHeader();
    path = _path;
    numCommands = _numCommands;
    commands_shm_id = _commands_shm_id;
    commands_shm_offset = _commands_shm_offset;
    numCoords = _numCoords;
    coordType = _coordType;
    coords_shm_id = _coords_shm_id;
    coords_shm_offset = _coords_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _path,
            GLsizei _numCommands,
            uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            GLsizei _numCoords,
            GLenum _coordType,
            uint32_t _coords_shm_id,
            uint32_t _coords_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _path, _numCommands, _commands_shm_id, _commands_shm_offset, _numCoords,
        _coordType, _coords_shm_id, _coords_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  int32_t numCommands;
  uint32_t commands_shm_id;
  uint32_t commands_shm_offset;
  int32_t numCoords;
  uint32_t coordType;
  uint32_t coords_shm_id;
  uint32_t coords_shm_offset;
};

static_assert(sizeof(PathCommandsCHROMIUM) == 36,
              "size of PathCommandsCHROMIUM should be 36");
static_assert(offsetof(PathCommandsCHROMIUM, header) == 0,
              "offset of PathCommandsCHROMIUM header should be 0");
static_assert(offsetof(PathCommandsCHROMIUM, path) == 4,
              "offset of PathCommandsCHROMIUM path should be 4");
static_assert(offsetof(PathCommandsCHROMIUM, numCommands) == 8,
              "offset of PathCommandsCHROMIUM numCommands should be 8");
static_assert(offsetof(PathCommandsCHROMIUM, commands_shm_id) == 12,
              "offset of PathCommandsCHROMIUM commands_shm_id should be 12");
static_assert(
    offsetof(PathCommandsCHROMIUM, commands_shm_offset) == 16,
    "offset of PathCommandsCHROMIUM commands_shm_offset should be 16");
static_assert(offsetof(PathCommandsCHROMIUM, numCoords) == 20,
              "offset of PathCommandsCHROMIUM numCoords should be 20");
static_assert(offsetof(PathCommandsCHROMIUM, coordType) == 24,
              "offset of PathCommandsCHROMIUM coordType should be 24");
static_assert(offsetof(PathCommandsCHROMIUM, coords_shm_id) == 28,
              "offset of PathCommandsCHROMIUM coords_shm_id should be 28");
static_assert(offsetof(PathCommandsCHROMIUM, coords_shm_offset) == 32,
              "offset of PathCommandsCHROMIUM coords_shm_offset should be 32");

struct PathParameterfCHROMIUM {
  typedef PathParameterfCHROMIUM ValueType;
  static const CommandId kCmdId = kPathParameterfCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLenum _pname, GLfloat _value) {
    SetHeader();
    path = _path;
    pname = _pname;
    value = _value;
  }

  void* Set(void* cmd, GLuint _path, GLenum _pname, GLfloat _value) {
    static_cast<ValueType*>(cmd)->Init(_path, _pname, _value);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  uint32_t pname;
  float value;
};

static_assert(sizeof(PathParameterfCHROMIUM) == 16,
              "size of PathParameterfCHROMIUM should be 16");
static_assert(offsetof(PathParameterfCHROMIUM, header) == 0,
              "offset of PathParameterfCHROMIUM header should be 0");
static_assert(offsetof(PathParameterfCHROMIUM, path) == 4,
              "offset of PathParameterfCHROMIUM path should be 4");
static_assert(offsetof(PathParameterfCHROMIUM, pname) == 8,
              "offset of PathParameterfCHROMIUM pname should be 8");
static_assert(offsetof(PathParameterfCHROMIUM, value) == 12,
              "offset of PathParameterfCHROMIUM value should be 12");

struct PathParameteriCHROMIUM {
  typedef PathParameteriCHROMIUM ValueType;
  static const CommandId kCmdId = kPathParameteriCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLenum _pname, GLint _value) {
    SetHeader();
    path = _path;
    pname = _pname;
    value = _value;
  }

  void* Set(void* cmd, GLuint _path, GLenum _pname, GLint _value) {
    static_cast<ValueType*>(cmd)->Init(_path, _pname, _value);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  uint32_t pname;
  int32_t value;
};

static_assert(sizeof(PathParameteriCHROMIUM) == 16,
              "size of PathParameteriCHROMIUM should be 16");
static_assert(offsetof(PathParameteriCHROMIUM, header) == 0,
              "offset of PathParameteriCHROMIUM header should be 0");
static_assert(offsetof(PathParameteriCHROMIUM, path) == 4,
              "offset of PathParameteriCHROMIUM path should be 4");
static_assert(offsetof(PathParameteriCHROMIUM, pname) == 8,
              "offset of PathParameteriCHROMIUM pname should be 8");
static_assert(offsetof(PathParameteriCHROMIUM, value) == 12,
              "offset of PathParameteriCHROMIUM value should be 12");

struct PathStencilFuncCHROMIUM {
  typedef PathStencilFuncCHROMIUM ValueType;
  static const CommandId kCmdId = kPathStencilFuncCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _func, GLint _ref, GLuint _mask) {
    SetHeader();
    func = _func;
    ref = _ref;
    mask = _mask;
  }

  void* Set(void* cmd, GLenum _func, GLint _ref, GLuint _mask) {
    static_cast<ValueType*>(cmd)->Init(_func, _ref, _mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t func;
  int32_t ref;
  uint32_t mask;
};

static_assert(sizeof(PathStencilFuncCHROMIUM) == 16,
              "size of PathStencilFuncCHROMIUM should be 16");
static_assert(offsetof(PathStencilFuncCHROMIUM, header) == 0,
              "offset of PathStencilFuncCHROMIUM header should be 0");
static_assert(offsetof(PathStencilFuncCHROMIUM, func) == 4,
              "offset of PathStencilFuncCHROMIUM func should be 4");
static_assert(offsetof(PathStencilFuncCHROMIUM, ref) == 8,
              "offset of PathStencilFuncCHROMIUM ref should be 8");
static_assert(offsetof(PathStencilFuncCHROMIUM, mask) == 12,
              "offset of PathStencilFuncCHROMIUM mask should be 12");

struct StencilFillPathCHROMIUM {
  typedef StencilFillPathCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilFillPathCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLenum _fillMode, GLuint _mask) {
    SetHeader();
    path = _path;
    fillMode = _fillMode;
    mask = _mask;
  }

  void* Set(void* cmd, GLuint _path, GLenum _fillMode, GLuint _mask) {
    static_cast<ValueType*>(cmd)->Init(_path, _fillMode, _mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  uint32_t fillMode;
  uint32_t mask;
};

static_assert(sizeof(StencilFillPathCHROMIUM) == 16,
              "size of StencilFillPathCHROMIUM should be 16");
static_assert(offsetof(StencilFillPathCHROMIUM, header) == 0,
              "offset of StencilFillPathCHROMIUM header should be 0");
static_assert(offsetof(StencilFillPathCHROMIUM, path) == 4,
              "offset of StencilFillPathCHROMIUM path should be 4");
static_assert(offsetof(StencilFillPathCHROMIUM, fillMode) == 8,
              "offset of StencilFillPathCHROMIUM fillMode should be 8");
static_assert(offsetof(StencilFillPathCHROMIUM, mask) == 12,
              "offset of StencilFillPathCHROMIUM mask should be 12");

struct StencilStrokePathCHROMIUM {
  typedef StencilStrokePathCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilStrokePathCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLint _reference, GLuint _mask) {
    SetHeader();
    path = _path;
    reference = _reference;
    mask = _mask;
  }

  void* Set(void* cmd, GLuint _path, GLint _reference, GLuint _mask) {
    static_cast<ValueType*>(cmd)->Init(_path, _reference, _mask);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  int32_t reference;
  uint32_t mask;
};

static_assert(sizeof(StencilStrokePathCHROMIUM) == 16,
              "size of StencilStrokePathCHROMIUM should be 16");
static_assert(offsetof(StencilStrokePathCHROMIUM, header) == 0,
              "offset of StencilStrokePathCHROMIUM header should be 0");
static_assert(offsetof(StencilStrokePathCHROMIUM, path) == 4,
              "offset of StencilStrokePathCHROMIUM path should be 4");
static_assert(offsetof(StencilStrokePathCHROMIUM, reference) == 8,
              "offset of StencilStrokePathCHROMIUM reference should be 8");
static_assert(offsetof(StencilStrokePathCHROMIUM, mask) == 12,
              "offset of StencilStrokePathCHROMIUM mask should be 12");

struct CoverFillPathCHROMIUM {
  typedef CoverFillPathCHROMIUM ValueType;
  static const CommandId kCmdId = kCoverFillPathCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLenum _coverMode) {
    SetHeader();
    path = _path;
    coverMode = _coverMode;
  }

  void* Set(void* cmd, GLuint _path, GLenum _coverMode) {
    static_cast<ValueType*>(cmd)->Init(_path, _coverMode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  uint32_t coverMode;
};

static_assert(sizeof(CoverFillPathCHROMIUM) == 12,
              "size of CoverFillPathCHROMIUM should be 12");
static_assert(offsetof(CoverFillPathCHROMIUM, header) == 0,
              "offset of CoverFillPathCHROMIUM header should be 0");
static_assert(offsetof(CoverFillPathCHROMIUM, path) == 4,
              "offset of CoverFillPathCHROMIUM path should be 4");
static_assert(offsetof(CoverFillPathCHROMIUM, coverMode) == 8,
              "offset of CoverFillPathCHROMIUM coverMode should be 8");

struct CoverStrokePathCHROMIUM {
  typedef CoverStrokePathCHROMIUM ValueType;
  static const CommandId kCmdId = kCoverStrokePathCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLenum _coverMode) {
    SetHeader();
    path = _path;
    coverMode = _coverMode;
  }

  void* Set(void* cmd, GLuint _path, GLenum _coverMode) {
    static_cast<ValueType*>(cmd)->Init(_path, _coverMode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  uint32_t coverMode;
};

static_assert(sizeof(CoverStrokePathCHROMIUM) == 12,
              "size of CoverStrokePathCHROMIUM should be 12");
static_assert(offsetof(CoverStrokePathCHROMIUM, header) == 0,
              "offset of CoverStrokePathCHROMIUM header should be 0");
static_assert(offsetof(CoverStrokePathCHROMIUM, path) == 4,
              "offset of CoverStrokePathCHROMIUM path should be 4");
static_assert(offsetof(CoverStrokePathCHROMIUM, coverMode) == 8,
              "offset of CoverStrokePathCHROMIUM coverMode should be 8");

struct StencilThenCoverFillPathCHROMIUM {
  typedef StencilThenCoverFillPathCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilThenCoverFillPathCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLenum _fillMode, GLuint _mask, GLenum _coverMode) {
    SetHeader();
    path = _path;
    fillMode = _fillMode;
    mask = _mask;
    coverMode = _coverMode;
  }

  void* Set(void* cmd,
            GLuint _path,
            GLenum _fillMode,
            GLuint _mask,
            GLenum _coverMode) {
    static_cast<ValueType*>(cmd)->Init(_path, _fillMode, _mask, _coverMode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  uint32_t fillMode;
  uint32_t mask;
  uint32_t coverMode;
};

static_assert(sizeof(StencilThenCoverFillPathCHROMIUM) == 20,
              "size of StencilThenCoverFillPathCHROMIUM should be 20");
static_assert(offsetof(StencilThenCoverFillPathCHROMIUM, header) == 0,
              "offset of StencilThenCoverFillPathCHROMIUM header should be 0");
static_assert(offsetof(StencilThenCoverFillPathCHROMIUM, path) == 4,
              "offset of StencilThenCoverFillPathCHROMIUM path should be 4");
static_assert(
    offsetof(StencilThenCoverFillPathCHROMIUM, fillMode) == 8,
    "offset of StencilThenCoverFillPathCHROMIUM fillMode should be 8");
static_assert(offsetof(StencilThenCoverFillPathCHROMIUM, mask) == 12,
              "offset of StencilThenCoverFillPathCHROMIUM mask should be 12");
static_assert(
    offsetof(StencilThenCoverFillPathCHROMIUM, coverMode) == 16,
    "offset of StencilThenCoverFillPathCHROMIUM coverMode should be 16");

struct StencilThenCoverStrokePathCHROMIUM {
  typedef StencilThenCoverStrokePathCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilThenCoverStrokePathCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _path, GLint _reference, GLuint _mask, GLenum _coverMode) {
    SetHeader();
    path = _path;
    reference = _reference;
    mask = _mask;
    coverMode = _coverMode;
  }

  void* Set(void* cmd,
            GLuint _path,
            GLint _reference,
            GLuint _mask,
            GLenum _coverMode) {
    static_cast<ValueType*>(cmd)->Init(_path, _reference, _mask, _coverMode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t path;
  int32_t reference;
  uint32_t mask;
  uint32_t coverMode;
};

static_assert(sizeof(StencilThenCoverStrokePathCHROMIUM) == 20,
              "size of StencilThenCoverStrokePathCHROMIUM should be 20");
static_assert(
    offsetof(StencilThenCoverStrokePathCHROMIUM, header) == 0,
    "offset of StencilThenCoverStrokePathCHROMIUM header should be 0");
static_assert(offsetof(StencilThenCoverStrokePathCHROMIUM, path) == 4,
              "offset of StencilThenCoverStrokePathCHROMIUM path should be 4");
static_assert(
    offsetof(StencilThenCoverStrokePathCHROMIUM, reference) == 8,
    "offset of StencilThenCoverStrokePathCHROMIUM reference should be 8");
static_assert(offsetof(StencilThenCoverStrokePathCHROMIUM, mask) == 12,
              "offset of StencilThenCoverStrokePathCHROMIUM mask should be 12");
static_assert(
    offsetof(StencilThenCoverStrokePathCHROMIUM, coverMode) == 16,
    "offset of StencilThenCoverStrokePathCHROMIUM coverMode should be 16");

struct StencilFillPathInstancedCHROMIUM {
  typedef StencilFillPathInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilFillPathInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _fillMode,
            GLuint _mask,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    SetHeader();
    numPaths = _numPaths;
    pathNameType = _pathNameType;
    paths_shm_id = _paths_shm_id;
    paths_shm_offset = _paths_shm_offset;
    pathBase = _pathBase;
    fillMode = _fillMode;
    mask = _mask;
    transformType = _transformType;
    transformValues_shm_id = _transformValues_shm_id;
    transformValues_shm_offset = _transformValues_shm_offset;
  }

  void* Set(void* cmd,
            GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _fillMode,
            GLuint _mask,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _numPaths, _pathNameType, _paths_shm_id, _paths_shm_offset, _pathBase,
        _fillMode, _mask, _transformType, _transformValues_shm_id,
        _transformValues_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t numPaths;
  uint32_t pathNameType;
  uint32_t paths_shm_id;
  uint32_t paths_shm_offset;
  uint32_t pathBase;
  uint32_t fillMode;
  uint32_t mask;
  uint32_t transformType;
  uint32_t transformValues_shm_id;
  uint32_t transformValues_shm_offset;
};

static_assert(sizeof(StencilFillPathInstancedCHROMIUM) == 44,
              "size of StencilFillPathInstancedCHROMIUM should be 44");
static_assert(offsetof(StencilFillPathInstancedCHROMIUM, header) == 0,
              "offset of StencilFillPathInstancedCHROMIUM header should be 0");
static_assert(
    offsetof(StencilFillPathInstancedCHROMIUM, numPaths) == 4,
    "offset of StencilFillPathInstancedCHROMIUM numPaths should be 4");
static_assert(
    offsetof(StencilFillPathInstancedCHROMIUM, pathNameType) == 8,
    "offset of StencilFillPathInstancedCHROMIUM pathNameType should be 8");
static_assert(
    offsetof(StencilFillPathInstancedCHROMIUM, paths_shm_id) == 12,
    "offset of StencilFillPathInstancedCHROMIUM paths_shm_id should be 12");
static_assert(
    offsetof(StencilFillPathInstancedCHROMIUM, paths_shm_offset) == 16,
    "offset of StencilFillPathInstancedCHROMIUM paths_shm_offset should be 16");
static_assert(
    offsetof(StencilFillPathInstancedCHROMIUM, pathBase) == 20,
    "offset of StencilFillPathInstancedCHROMIUM pathBase should be 20");
static_assert(
    offsetof(StencilFillPathInstancedCHROMIUM, fillMode) == 24,
    "offset of StencilFillPathInstancedCHROMIUM fillMode should be 24");
static_assert(offsetof(StencilFillPathInstancedCHROMIUM, mask) == 28,
              "offset of StencilFillPathInstancedCHROMIUM mask should be 28");
static_assert(
    offsetof(StencilFillPathInstancedCHROMIUM, transformType) == 32,
    "offset of StencilFillPathInstancedCHROMIUM transformType should be 32");
static_assert(offsetof(StencilFillPathInstancedCHROMIUM,
                       transformValues_shm_id) == 36,
              "offset of StencilFillPathInstancedCHROMIUM "
              "transformValues_shm_id should be 36");
static_assert(offsetof(StencilFillPathInstancedCHROMIUM,
                       transformValues_shm_offset) == 40,
              "offset of StencilFillPathInstancedCHROMIUM "
              "transformValues_shm_offset should be 40");

struct StencilStrokePathInstancedCHROMIUM {
  typedef StencilStrokePathInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilStrokePathInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLint _reference,
            GLuint _mask,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    SetHeader();
    numPaths = _numPaths;
    pathNameType = _pathNameType;
    paths_shm_id = _paths_shm_id;
    paths_shm_offset = _paths_shm_offset;
    pathBase = _pathBase;
    reference = _reference;
    mask = _mask;
    transformType = _transformType;
    transformValues_shm_id = _transformValues_shm_id;
    transformValues_shm_offset = _transformValues_shm_offset;
  }

  void* Set(void* cmd,
            GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLint _reference,
            GLuint _mask,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _numPaths, _pathNameType, _paths_shm_id, _paths_shm_offset, _pathBase,
        _reference, _mask, _transformType, _transformValues_shm_id,
        _transformValues_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t numPaths;
  uint32_t pathNameType;
  uint32_t paths_shm_id;
  uint32_t paths_shm_offset;
  uint32_t pathBase;
  int32_t reference;
  uint32_t mask;
  uint32_t transformType;
  uint32_t transformValues_shm_id;
  uint32_t transformValues_shm_offset;
};

static_assert(sizeof(StencilStrokePathInstancedCHROMIUM) == 44,
              "size of StencilStrokePathInstancedCHROMIUM should be 44");
static_assert(
    offsetof(StencilStrokePathInstancedCHROMIUM, header) == 0,
    "offset of StencilStrokePathInstancedCHROMIUM header should be 0");
static_assert(
    offsetof(StencilStrokePathInstancedCHROMIUM, numPaths) == 4,
    "offset of StencilStrokePathInstancedCHROMIUM numPaths should be 4");
static_assert(
    offsetof(StencilStrokePathInstancedCHROMIUM, pathNameType) == 8,
    "offset of StencilStrokePathInstancedCHROMIUM pathNameType should be 8");
static_assert(
    offsetof(StencilStrokePathInstancedCHROMIUM, paths_shm_id) == 12,
    "offset of StencilStrokePathInstancedCHROMIUM paths_shm_id should be 12");
static_assert(offsetof(StencilStrokePathInstancedCHROMIUM, paths_shm_offset) ==
                  16,
              "offset of StencilStrokePathInstancedCHROMIUM paths_shm_offset "
              "should be 16");
static_assert(
    offsetof(StencilStrokePathInstancedCHROMIUM, pathBase) == 20,
    "offset of StencilStrokePathInstancedCHROMIUM pathBase should be 20");
static_assert(
    offsetof(StencilStrokePathInstancedCHROMIUM, reference) == 24,
    "offset of StencilStrokePathInstancedCHROMIUM reference should be 24");
static_assert(offsetof(StencilStrokePathInstancedCHROMIUM, mask) == 28,
              "offset of StencilStrokePathInstancedCHROMIUM mask should be 28");
static_assert(
    offsetof(StencilStrokePathInstancedCHROMIUM, transformType) == 32,
    "offset of StencilStrokePathInstancedCHROMIUM transformType should be 32");
static_assert(offsetof(StencilStrokePathInstancedCHROMIUM,
                       transformValues_shm_id) == 36,
              "offset of StencilStrokePathInstancedCHROMIUM "
              "transformValues_shm_id should be 36");
static_assert(offsetof(StencilStrokePathInstancedCHROMIUM,
                       transformValues_shm_offset) == 40,
              "offset of StencilStrokePathInstancedCHROMIUM "
              "transformValues_shm_offset should be 40");

struct CoverFillPathInstancedCHROMIUM {
  typedef CoverFillPathInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kCoverFillPathInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    SetHeader();
    numPaths = _numPaths;
    pathNameType = _pathNameType;
    paths_shm_id = _paths_shm_id;
    paths_shm_offset = _paths_shm_offset;
    pathBase = _pathBase;
    coverMode = _coverMode;
    transformType = _transformType;
    transformValues_shm_id = _transformValues_shm_id;
    transformValues_shm_offset = _transformValues_shm_offset;
  }

  void* Set(void* cmd,
            GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_numPaths, _pathNameType, _paths_shm_id,
                                       _paths_shm_offset, _pathBase, _coverMode,
                                       _transformType, _transformValues_shm_id,
                                       _transformValues_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t numPaths;
  uint32_t pathNameType;
  uint32_t paths_shm_id;
  uint32_t paths_shm_offset;
  uint32_t pathBase;
  uint32_t coverMode;
  uint32_t transformType;
  uint32_t transformValues_shm_id;
  uint32_t transformValues_shm_offset;
};

static_assert(sizeof(CoverFillPathInstancedCHROMIUM) == 40,
              "size of CoverFillPathInstancedCHROMIUM should be 40");
static_assert(offsetof(CoverFillPathInstancedCHROMIUM, header) == 0,
              "offset of CoverFillPathInstancedCHROMIUM header should be 0");
static_assert(offsetof(CoverFillPathInstancedCHROMIUM, numPaths) == 4,
              "offset of CoverFillPathInstancedCHROMIUM numPaths should be 4");
static_assert(
    offsetof(CoverFillPathInstancedCHROMIUM, pathNameType) == 8,
    "offset of CoverFillPathInstancedCHROMIUM pathNameType should be 8");
static_assert(
    offsetof(CoverFillPathInstancedCHROMIUM, paths_shm_id) == 12,
    "offset of CoverFillPathInstancedCHROMIUM paths_shm_id should be 12");
static_assert(
    offsetof(CoverFillPathInstancedCHROMIUM, paths_shm_offset) == 16,
    "offset of CoverFillPathInstancedCHROMIUM paths_shm_offset should be 16");
static_assert(offsetof(CoverFillPathInstancedCHROMIUM, pathBase) == 20,
              "offset of CoverFillPathInstancedCHROMIUM pathBase should be 20");
static_assert(
    offsetof(CoverFillPathInstancedCHROMIUM, coverMode) == 24,
    "offset of CoverFillPathInstancedCHROMIUM coverMode should be 24");
static_assert(
    offsetof(CoverFillPathInstancedCHROMIUM, transformType) == 28,
    "offset of CoverFillPathInstancedCHROMIUM transformType should be 28");
static_assert(offsetof(CoverFillPathInstancedCHROMIUM,
                       transformValues_shm_id) == 32,
              "offset of CoverFillPathInstancedCHROMIUM transformValues_shm_id "
              "should be 32");
static_assert(offsetof(CoverFillPathInstancedCHROMIUM,
                       transformValues_shm_offset) == 36,
              "offset of CoverFillPathInstancedCHROMIUM "
              "transformValues_shm_offset should be 36");

struct CoverStrokePathInstancedCHROMIUM {
  typedef CoverStrokePathInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kCoverStrokePathInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    SetHeader();
    numPaths = _numPaths;
    pathNameType = _pathNameType;
    paths_shm_id = _paths_shm_id;
    paths_shm_offset = _paths_shm_offset;
    pathBase = _pathBase;
    coverMode = _coverMode;
    transformType = _transformType;
    transformValues_shm_id = _transformValues_shm_id;
    transformValues_shm_offset = _transformValues_shm_offset;
  }

  void* Set(void* cmd,
            GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_numPaths, _pathNameType, _paths_shm_id,
                                       _paths_shm_offset, _pathBase, _coverMode,
                                       _transformType, _transformValues_shm_id,
                                       _transformValues_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t numPaths;
  uint32_t pathNameType;
  uint32_t paths_shm_id;
  uint32_t paths_shm_offset;
  uint32_t pathBase;
  uint32_t coverMode;
  uint32_t transformType;
  uint32_t transformValues_shm_id;
  uint32_t transformValues_shm_offset;
};

static_assert(sizeof(CoverStrokePathInstancedCHROMIUM) == 40,
              "size of CoverStrokePathInstancedCHROMIUM should be 40");
static_assert(offsetof(CoverStrokePathInstancedCHROMIUM, header) == 0,
              "offset of CoverStrokePathInstancedCHROMIUM header should be 0");
static_assert(
    offsetof(CoverStrokePathInstancedCHROMIUM, numPaths) == 4,
    "offset of CoverStrokePathInstancedCHROMIUM numPaths should be 4");
static_assert(
    offsetof(CoverStrokePathInstancedCHROMIUM, pathNameType) == 8,
    "offset of CoverStrokePathInstancedCHROMIUM pathNameType should be 8");
static_assert(
    offsetof(CoverStrokePathInstancedCHROMIUM, paths_shm_id) == 12,
    "offset of CoverStrokePathInstancedCHROMIUM paths_shm_id should be 12");
static_assert(
    offsetof(CoverStrokePathInstancedCHROMIUM, paths_shm_offset) == 16,
    "offset of CoverStrokePathInstancedCHROMIUM paths_shm_offset should be 16");
static_assert(
    offsetof(CoverStrokePathInstancedCHROMIUM, pathBase) == 20,
    "offset of CoverStrokePathInstancedCHROMIUM pathBase should be 20");
static_assert(
    offsetof(CoverStrokePathInstancedCHROMIUM, coverMode) == 24,
    "offset of CoverStrokePathInstancedCHROMIUM coverMode should be 24");
static_assert(
    offsetof(CoverStrokePathInstancedCHROMIUM, transformType) == 28,
    "offset of CoverStrokePathInstancedCHROMIUM transformType should be 28");
static_assert(offsetof(CoverStrokePathInstancedCHROMIUM,
                       transformValues_shm_id) == 32,
              "offset of CoverStrokePathInstancedCHROMIUM "
              "transformValues_shm_id should be 32");
static_assert(offsetof(CoverStrokePathInstancedCHROMIUM,
                       transformValues_shm_offset) == 36,
              "offset of CoverStrokePathInstancedCHROMIUM "
              "transformValues_shm_offset should be 36");

struct StencilThenCoverFillPathInstancedCHROMIUM {
  typedef StencilThenCoverFillPathInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilThenCoverFillPathInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _fillMode,
            GLuint _mask,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    SetHeader();
    numPaths = _numPaths;
    pathNameType = _pathNameType;
    paths_shm_id = _paths_shm_id;
    paths_shm_offset = _paths_shm_offset;
    pathBase = _pathBase;
    fillMode = _fillMode;
    mask = _mask;
    coverMode = _coverMode;
    transformType = _transformType;
    transformValues_shm_id = _transformValues_shm_id;
    transformValues_shm_offset = _transformValues_shm_offset;
  }

  void* Set(void* cmd,
            GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLenum _fillMode,
            GLuint _mask,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _numPaths, _pathNameType, _paths_shm_id, _paths_shm_offset, _pathBase,
        _fillMode, _mask, _coverMode, _transformType, _transformValues_shm_id,
        _transformValues_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t numPaths;
  uint32_t pathNameType;
  uint32_t paths_shm_id;
  uint32_t paths_shm_offset;
  uint32_t pathBase;
  uint32_t fillMode;
  uint32_t mask;
  uint32_t coverMode;
  uint32_t transformType;
  uint32_t transformValues_shm_id;
  uint32_t transformValues_shm_offset;
};

static_assert(sizeof(StencilThenCoverFillPathInstancedCHROMIUM) == 48,
              "size of StencilThenCoverFillPathInstancedCHROMIUM should be 48");
static_assert(
    offsetof(StencilThenCoverFillPathInstancedCHROMIUM, header) == 0,
    "offset of StencilThenCoverFillPathInstancedCHROMIUM header should be 0");
static_assert(
    offsetof(StencilThenCoverFillPathInstancedCHROMIUM, numPaths) == 4,
    "offset of StencilThenCoverFillPathInstancedCHROMIUM numPaths should be 4");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM,
                       pathNameType) == 8,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM "
              "pathNameType should be 8");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM,
                       paths_shm_id) == 12,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM "
              "paths_shm_id should be 12");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM,
                       paths_shm_offset) == 16,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM "
              "paths_shm_offset should be 16");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM, pathBase) ==
                  20,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM pathBase "
              "should be 20");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM, fillMode) ==
                  24,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM fillMode "
              "should be 24");
static_assert(
    offsetof(StencilThenCoverFillPathInstancedCHROMIUM, mask) == 28,
    "offset of StencilThenCoverFillPathInstancedCHROMIUM mask should be 28");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM, coverMode) ==
                  32,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM coverMode "
              "should be 32");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM,
                       transformType) == 36,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM "
              "transformType should be 36");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM,
                       transformValues_shm_id) == 40,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM "
              "transformValues_shm_id should be 40");
static_assert(offsetof(StencilThenCoverFillPathInstancedCHROMIUM,
                       transformValues_shm_offset) == 44,
              "offset of StencilThenCoverFillPathInstancedCHROMIUM "
              "transformValues_shm_offset should be 44");

struct StencilThenCoverStrokePathInstancedCHROMIUM {
  typedef StencilThenCoverStrokePathInstancedCHROMIUM ValueType;
  static const CommandId kCmdId = kStencilThenCoverStrokePathInstancedCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLint _reference,
            GLuint _mask,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    SetHeader();
    numPaths = _numPaths;
    pathNameType = _pathNameType;
    paths_shm_id = _paths_shm_id;
    paths_shm_offset = _paths_shm_offset;
    pathBase = _pathBase;
    reference = _reference;
    mask = _mask;
    coverMode = _coverMode;
    transformType = _transformType;
    transformValues_shm_id = _transformValues_shm_id;
    transformValues_shm_offset = _transformValues_shm_offset;
  }

  void* Set(void* cmd,
            GLsizei _numPaths,
            GLenum _pathNameType,
            uint32_t _paths_shm_id,
            uint32_t _paths_shm_offset,
            GLuint _pathBase,
            GLint _reference,
            GLuint _mask,
            GLenum _coverMode,
            GLenum _transformType,
            uint32_t _transformValues_shm_id,
            uint32_t _transformValues_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(
        _numPaths, _pathNameType, _paths_shm_id, _paths_shm_offset, _pathBase,
        _reference, _mask, _coverMode, _transformType, _transformValues_shm_id,
        _transformValues_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t numPaths;
  uint32_t pathNameType;
  uint32_t paths_shm_id;
  uint32_t paths_shm_offset;
  uint32_t pathBase;
  int32_t reference;
  uint32_t mask;
  uint32_t coverMode;
  uint32_t transformType;
  uint32_t transformValues_shm_id;
  uint32_t transformValues_shm_offset;
};

static_assert(
    sizeof(StencilThenCoverStrokePathInstancedCHROMIUM) == 48,
    "size of StencilThenCoverStrokePathInstancedCHROMIUM should be 48");
static_assert(
    offsetof(StencilThenCoverStrokePathInstancedCHROMIUM, header) == 0,
    "offset of StencilThenCoverStrokePathInstancedCHROMIUM header should be 0");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM, numPaths) ==
                  4,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM numPaths "
              "should be 4");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       pathNameType) == 8,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM "
              "pathNameType should be 8");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       paths_shm_id) == 12,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM "
              "paths_shm_id should be 12");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       paths_shm_offset) == 16,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM "
              "paths_shm_offset should be 16");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM, pathBase) ==
                  20,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM pathBase "
              "should be 20");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       reference) == 24,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM reference "
              "should be 24");
static_assert(
    offsetof(StencilThenCoverStrokePathInstancedCHROMIUM, mask) == 28,
    "offset of StencilThenCoverStrokePathInstancedCHROMIUM mask should be 28");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       coverMode) == 32,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM coverMode "
              "should be 32");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       transformType) == 36,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM "
              "transformType should be 36");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       transformValues_shm_id) == 40,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM "
              "transformValues_shm_id should be 40");
static_assert(offsetof(StencilThenCoverStrokePathInstancedCHROMIUM,
                       transformValues_shm_offset) == 44,
              "offset of StencilThenCoverStrokePathInstancedCHROMIUM "
              "transformValues_shm_offset should be 44");

struct BindFragmentInputLocationCHROMIUMBucket {
  typedef BindFragmentInputLocationCHROMIUMBucket ValueType;
  static const CommandId kCmdId = kBindFragmentInputLocationCHROMIUMBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, GLint _location, uint32_t _name_bucket_id) {
    SetHeader();
    program = _program;
    location = _location;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLint _location,
            uint32_t _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _location, _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  int32_t location;
  uint32_t name_bucket_id;
};

static_assert(sizeof(BindFragmentInputLocationCHROMIUMBucket) == 16,
              "size of BindFragmentInputLocationCHROMIUMBucket should be 16");
static_assert(
    offsetof(BindFragmentInputLocationCHROMIUMBucket, header) == 0,
    "offset of BindFragmentInputLocationCHROMIUMBucket header should be 0");
static_assert(
    offsetof(BindFragmentInputLocationCHROMIUMBucket, program) == 4,
    "offset of BindFragmentInputLocationCHROMIUMBucket program should be 4");
static_assert(
    offsetof(BindFragmentInputLocationCHROMIUMBucket, location) == 8,
    "offset of BindFragmentInputLocationCHROMIUMBucket location should be 8");
static_assert(offsetof(BindFragmentInputLocationCHROMIUMBucket,
                       name_bucket_id) == 12,
              "offset of BindFragmentInputLocationCHROMIUMBucket "
              "name_bucket_id should be 12");

struct ProgramPathFragmentInputGenCHROMIUM {
  typedef ProgramPathFragmentInputGenCHROMIUM ValueType;
  static const CommandId kCmdId = kProgramPathFragmentInputGenCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLint _location,
            GLenum _genMode,
            GLint _components,
            uint32_t _coeffs_shm_id,
            uint32_t _coeffs_shm_offset) {
    SetHeader();
    program = _program;
    location = _location;
    genMode = _genMode;
    components = _components;
    coeffs_shm_id = _coeffs_shm_id;
    coeffs_shm_offset = _coeffs_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLint _location,
            GLenum _genMode,
            GLint _components,
            uint32_t _coeffs_shm_id,
            uint32_t _coeffs_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _location, _genMode,
                                       _components, _coeffs_shm_id,
                                       _coeffs_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  int32_t location;
  uint32_t genMode;
  int32_t components;
  uint32_t coeffs_shm_id;
  uint32_t coeffs_shm_offset;
};

static_assert(sizeof(ProgramPathFragmentInputGenCHROMIUM) == 28,
              "size of ProgramPathFragmentInputGenCHROMIUM should be 28");
static_assert(
    offsetof(ProgramPathFragmentInputGenCHROMIUM, header) == 0,
    "offset of ProgramPathFragmentInputGenCHROMIUM header should be 0");
static_assert(
    offsetof(ProgramPathFragmentInputGenCHROMIUM, program) == 4,
    "offset of ProgramPathFragmentInputGenCHROMIUM program should be 4");
static_assert(
    offsetof(ProgramPathFragmentInputGenCHROMIUM, location) == 8,
    "offset of ProgramPathFragmentInputGenCHROMIUM location should be 8");
static_assert(
    offsetof(ProgramPathFragmentInputGenCHROMIUM, genMode) == 12,
    "offset of ProgramPathFragmentInputGenCHROMIUM genMode should be 12");
static_assert(
    offsetof(ProgramPathFragmentInputGenCHROMIUM, components) == 16,
    "offset of ProgramPathFragmentInputGenCHROMIUM components should be 16");
static_assert(
    offsetof(ProgramPathFragmentInputGenCHROMIUM, coeffs_shm_id) == 20,
    "offset of ProgramPathFragmentInputGenCHROMIUM coeffs_shm_id should be 20");
static_assert(offsetof(ProgramPathFragmentInputGenCHROMIUM,
                       coeffs_shm_offset) == 24,
              "offset of ProgramPathFragmentInputGenCHROMIUM coeffs_shm_offset "
              "should be 24");

struct ContextVisibilityHintCHROMIUM {
  typedef ContextVisibilityHintCHROMIUM ValueType;
  static const CommandId kCmdId = kContextVisibilityHintCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLboolean _visibility) {
    SetHeader();
    visibility = _visibility;
  }

  void* Set(void* cmd, GLboolean _visibility) {
    static_cast<ValueType*>(cmd)->Init(_visibility);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t visibility;
};

static_assert(sizeof(ContextVisibilityHintCHROMIUM) == 8,
              "size of ContextVisibilityHintCHROMIUM should be 8");
static_assert(offsetof(ContextVisibilityHintCHROMIUM, header) == 0,
              "offset of ContextVisibilityHintCHROMIUM header should be 0");
static_assert(offsetof(ContextVisibilityHintCHROMIUM, visibility) == 4,
              "offset of ContextVisibilityHintCHROMIUM visibility should be 4");

struct CoverageModulationCHROMIUM {
  typedef CoverageModulationCHROMIUM ValueType;
  static const CommandId kCmdId = kCoverageModulationCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _components) {
    SetHeader();
    components = _components;
  }

  void* Set(void* cmd, GLenum _components) {
    static_cast<ValueType*>(cmd)->Init(_components);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t components;
};

static_assert(sizeof(CoverageModulationCHROMIUM) == 8,
              "size of CoverageModulationCHROMIUM should be 8");
static_assert(offsetof(CoverageModulationCHROMIUM, header) == 0,
              "offset of CoverageModulationCHROMIUM header should be 0");
static_assert(offsetof(CoverageModulationCHROMIUM, components) == 4,
              "offset of CoverageModulationCHROMIUM components should be 4");

struct BlendBarrierKHR {
  typedef BlendBarrierKHR ValueType;
  static const CommandId kCmdId = kBlendBarrierKHR;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(BlendBarrierKHR) == 4,
              "size of BlendBarrierKHR should be 4");
static_assert(offsetof(BlendBarrierKHR, header) == 0,
              "offset of BlendBarrierKHR header should be 0");

struct BindFragDataLocationIndexedEXTBucket {
  typedef BindFragDataLocationIndexedEXTBucket ValueType;
  static const CommandId kCmdId = kBindFragDataLocationIndexedEXTBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            GLuint _colorNumber,
            GLuint _index,
            uint32_t _name_bucket_id) {
    SetHeader();
    program = _program;
    colorNumber = _colorNumber;
    index = _index;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _colorNumber,
            GLuint _index,
            uint32_t _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _colorNumber, _index,
                                       _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t colorNumber;
  uint32_t index;
  uint32_t name_bucket_id;
};

static_assert(sizeof(BindFragDataLocationIndexedEXTBucket) == 20,
              "size of BindFragDataLocationIndexedEXTBucket should be 20");
static_assert(
    offsetof(BindFragDataLocationIndexedEXTBucket, header) == 0,
    "offset of BindFragDataLocationIndexedEXTBucket header should be 0");
static_assert(
    offsetof(BindFragDataLocationIndexedEXTBucket, program) == 4,
    "offset of BindFragDataLocationIndexedEXTBucket program should be 4");
static_assert(
    offsetof(BindFragDataLocationIndexedEXTBucket, colorNumber) == 8,
    "offset of BindFragDataLocationIndexedEXTBucket colorNumber should be 8");
static_assert(
    offsetof(BindFragDataLocationIndexedEXTBucket, index) == 12,
    "offset of BindFragDataLocationIndexedEXTBucket index should be 12");
static_assert(offsetof(BindFragDataLocationIndexedEXTBucket, name_bucket_id) ==
                  16,
              "offset of BindFragDataLocationIndexedEXTBucket name_bucket_id "
              "should be 16");

struct BindFragDataLocationEXTBucket {
  typedef BindFragDataLocationEXTBucket ValueType;
  static const CommandId kCmdId = kBindFragDataLocationEXTBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program, GLuint _colorNumber, uint32_t _name_bucket_id) {
    SetHeader();
    program = _program;
    colorNumber = _colorNumber;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd,
            GLuint _program,
            GLuint _colorNumber,
            uint32_t _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_program, _colorNumber, _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t colorNumber;
  uint32_t name_bucket_id;
};

static_assert(sizeof(BindFragDataLocationEXTBucket) == 16,
              "size of BindFragDataLocationEXTBucket should be 16");
static_assert(offsetof(BindFragDataLocationEXTBucket, header) == 0,
              "offset of BindFragDataLocationEXTBucket header should be 0");
static_assert(offsetof(BindFragDataLocationEXTBucket, program) == 4,
              "offset of BindFragDataLocationEXTBucket program should be 4");
static_assert(
    offsetof(BindFragDataLocationEXTBucket, colorNumber) == 8,
    "offset of BindFragDataLocationEXTBucket colorNumber should be 8");
static_assert(
    offsetof(BindFragDataLocationEXTBucket, name_bucket_id) == 12,
    "offset of BindFragDataLocationEXTBucket name_bucket_id should be 12");

struct GetFragDataIndexEXT {
  typedef GetFragDataIndexEXT ValueType;
  static const CommandId kCmdId = kGetFragDataIndexEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLint Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _index_shm_id,
            uint32_t _index_shm_offset) {
    SetHeader();
    program = _program;
    name_bucket_id = _name_bucket_id;
    index_shm_id = _index_shm_id;
    index_shm_offset = _index_shm_offset;
  }

  void* Set(void* cmd,
            GLuint _program,
            uint32_t _name_bucket_id,
            uint32_t _index_shm_id,
            uint32_t _index_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_program, _name_bucket_id, _index_shm_id,
                                       _index_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t program;
  uint32_t name_bucket_id;
  uint32_t index_shm_id;
  uint32_t index_shm_offset;
};

static_assert(sizeof(GetFragDataIndexEXT) == 20,
              "size of GetFragDataIndexEXT should be 20");
static_assert(offsetof(GetFragDataIndexEXT, header) == 0,
              "offset of GetFragDataIndexEXT header should be 0");
static_assert(offsetof(GetFragDataIndexEXT, program) == 4,
              "offset of GetFragDataIndexEXT program should be 4");
static_assert(offsetof(GetFragDataIndexEXT, name_bucket_id) == 8,
              "offset of GetFragDataIndexEXT name_bucket_id should be 8");
static_assert(offsetof(GetFragDataIndexEXT, index_shm_id) == 12,
              "offset of GetFragDataIndexEXT index_shm_id should be 12");
static_assert(offsetof(GetFragDataIndexEXT, index_shm_offset) == 16,
              "offset of GetFragDataIndexEXT index_shm_offset should be 16");

struct UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate {
  typedef UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate ValueType;
  static const CommandId kCmdId =
      kUniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLfloat) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLint _location, GLboolean _transpose, const GLfloat* _transform) {
    SetHeader();
    location = _location;
    transpose = _transpose;
    memcpy(ImmediateDataAddress(this), _transform, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLint _location,
            GLboolean _transpose,
            const GLfloat* _transform) {
    static_cast<ValueType*>(cmd)->Init(_location, _transpose, _transform);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t location;
  uint32_t transpose;
};

static_assert(sizeof(UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate) ==
                  12,
              "size of UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate "
              "should be 12");
static_assert(offsetof(UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate,
                       header) == 0,
              "offset of UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate "
              "header should be 0");
static_assert(offsetof(UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate,
                       location) == 4,
              "offset of UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate "
              "location should be 4");
static_assert(offsetof(UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate,
                       transpose) == 8,
              "offset of UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate "
              "transpose should be 8");

struct OverlayPromotionHintCHROMIUM {
  typedef OverlayPromotionHintCHROMIUM ValueType;
  static const CommandId kCmdId = kOverlayPromotionHintCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture,
            GLboolean _promotion_hint,
            GLint _display_x,
            GLint _display_y,
            GLint _display_width,
            GLint _display_height) {
    SetHeader();
    texture = _texture;
    promotion_hint = _promotion_hint;
    display_x = _display_x;
    display_y = _display_y;
    display_width = _display_width;
    display_height = _display_height;
  }

  void* Set(void* cmd,
            GLuint _texture,
            GLboolean _promotion_hint,
            GLint _display_x,
            GLint _display_y,
            GLint _display_width,
            GLint _display_height) {
    static_cast<ValueType*>(cmd)->Init(_texture, _promotion_hint, _display_x,
                                       _display_y, _display_width,
                                       _display_height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture;
  uint32_t promotion_hint;
  int32_t display_x;
  int32_t display_y;
  int32_t display_width;
  int32_t display_height;
};

static_assert(sizeof(OverlayPromotionHintCHROMIUM) == 28,
              "size of OverlayPromotionHintCHROMIUM should be 28");
static_assert(offsetof(OverlayPromotionHintCHROMIUM, header) == 0,
              "offset of OverlayPromotionHintCHROMIUM header should be 0");
static_assert(offsetof(OverlayPromotionHintCHROMIUM, texture) == 4,
              "offset of OverlayPromotionHintCHROMIUM texture should be 4");
static_assert(
    offsetof(OverlayPromotionHintCHROMIUM, promotion_hint) == 8,
    "offset of OverlayPromotionHintCHROMIUM promotion_hint should be 8");
static_assert(offsetof(OverlayPromotionHintCHROMIUM, display_x) == 12,
              "offset of OverlayPromotionHintCHROMIUM display_x should be 12");
static_assert(offsetof(OverlayPromotionHintCHROMIUM, display_y) == 16,
              "offset of OverlayPromotionHintCHROMIUM display_y should be 16");
static_assert(
    offsetof(OverlayPromotionHintCHROMIUM, display_width) == 20,
    "offset of OverlayPromotionHintCHROMIUM display_width should be 20");
static_assert(
    offsetof(OverlayPromotionHintCHROMIUM, display_height) == 24,
    "offset of OverlayPromotionHintCHROMIUM display_height should be 24");

struct SwapBuffersWithBoundsCHROMIUMImmediate {
  typedef SwapBuffersWithBoundsCHROMIUMImmediate ValueType;
  static const CommandId kCmdId = kSwapBuffersWithBoundsCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLint) * 4 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLuint64 _swap_id,
            GLsizei _count,
            const GLint* _rects,
            GLbitfield _flags) {
    SetHeader(_count);
    GLES2Util::MapUint64ToTwoUint32(static_cast<uint64_t>(_swap_id), &swap_id_0,
                                    &swap_id_1);
    count = _count;
    flags = _flags;
    memcpy(ImmediateDataAddress(this), _rects, ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLuint64 _swap_id,
            GLsizei _count,
            const GLint* _rects,
            GLbitfield _flags) {
    static_cast<ValueType*>(cmd)->Init(_swap_id, _count, _rects, _flags);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  GLuint64 swap_id() const volatile {
    return static_cast<GLuint64>(
        GLES2Util::MapTwoUint32ToUint64(swap_id_0, swap_id_1));
  }

  gpu::CommandHeader header;
  uint32_t swap_id_0;
  uint32_t swap_id_1;
  int32_t count;
  uint32_t flags;
};

static_assert(sizeof(SwapBuffersWithBoundsCHROMIUMImmediate) == 20,
              "size of SwapBuffersWithBoundsCHROMIUMImmediate should be 20");
static_assert(
    offsetof(SwapBuffersWithBoundsCHROMIUMImmediate, header) == 0,
    "offset of SwapBuffersWithBoundsCHROMIUMImmediate header should be 0");
static_assert(
    offsetof(SwapBuffersWithBoundsCHROMIUMImmediate, swap_id_0) == 4,
    "offset of SwapBuffersWithBoundsCHROMIUMImmediate swap_id_0 should be 4");
static_assert(
    offsetof(SwapBuffersWithBoundsCHROMIUMImmediate, swap_id_1) == 8,
    "offset of SwapBuffersWithBoundsCHROMIUMImmediate swap_id_1 should be 8");
static_assert(
    offsetof(SwapBuffersWithBoundsCHROMIUMImmediate, count) == 12,
    "offset of SwapBuffersWithBoundsCHROMIUMImmediate count should be 12");
static_assert(
    offsetof(SwapBuffersWithBoundsCHROMIUMImmediate, flags) == 16,
    "offset of SwapBuffersWithBoundsCHROMIUMImmediate flags should be 16");

struct SetDrawRectangleCHROMIUM {
  typedef SetDrawRectangleCHROMIUM ValueType;
  static const CommandId kCmdId = kSetDrawRectangleCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _x, GLint _y, GLint _width, GLint _height) {
    SetHeader();
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd, GLint _x, GLint _y, GLint _width, GLint _height) {
    static_cast<ValueType*>(cmd)->Init(_x, _y, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(SetDrawRectangleCHROMIUM) == 20,
              "size of SetDrawRectangleCHROMIUM should be 20");
static_assert(offsetof(SetDrawRectangleCHROMIUM, header) == 0,
              "offset of SetDrawRectangleCHROMIUM header should be 0");
static_assert(offsetof(SetDrawRectangleCHROMIUM, x) == 4,
              "offset of SetDrawRectangleCHROMIUM x should be 4");
static_assert(offsetof(SetDrawRectangleCHROMIUM, y) == 8,
              "offset of SetDrawRectangleCHROMIUM y should be 8");
static_assert(offsetof(SetDrawRectangleCHROMIUM, width) == 12,
              "offset of SetDrawRectangleCHROMIUM width should be 12");
static_assert(offsetof(SetDrawRectangleCHROMIUM, height) == 16,
              "offset of SetDrawRectangleCHROMIUM height should be 16");

struct SetEnableDCLayersCHROMIUM {
  typedef SetEnableDCLayersCHROMIUM ValueType;
  static const CommandId kCmdId = kSetEnableDCLayersCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLboolean _enabled) {
    SetHeader();
    enabled = _enabled;
  }

  void* Set(void* cmd, GLboolean _enabled) {
    static_cast<ValueType*>(cmd)->Init(_enabled);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t enabled;
};

static_assert(sizeof(SetEnableDCLayersCHROMIUM) == 8,
              "size of SetEnableDCLayersCHROMIUM should be 8");
static_assert(offsetof(SetEnableDCLayersCHROMIUM, header) == 0,
              "offset of SetEnableDCLayersCHROMIUM header should be 0");
static_assert(offsetof(SetEnableDCLayersCHROMIUM, enabled) == 4,
              "offset of SetEnableDCLayersCHROMIUM enabled should be 4");

struct InitializeDiscardableTextureCHROMIUM {
  typedef InitializeDiscardableTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kInitializeDiscardableTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id, uint32_t _shm_id, uint32_t _shm_offset) {
    SetHeader();
    texture_id = _texture_id;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
  }

  void* Set(void* cmd,
            GLuint _texture_id,
            uint32_t _shm_id,
            uint32_t _shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _shm_id, _shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t shm_id;
  uint32_t shm_offset;
};

static_assert(sizeof(InitializeDiscardableTextureCHROMIUM) == 16,
              "size of InitializeDiscardableTextureCHROMIUM should be 16");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, header) == 0,
    "offset of InitializeDiscardableTextureCHROMIUM header should be 0");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, texture_id) == 4,
    "offset of InitializeDiscardableTextureCHROMIUM texture_id should be 4");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, shm_id) == 8,
    "offset of InitializeDiscardableTextureCHROMIUM shm_id should be 8");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, shm_offset) == 12,
    "offset of InitializeDiscardableTextureCHROMIUM shm_offset should be 12");

struct UnlockDiscardableTextureCHROMIUM {
  typedef UnlockDiscardableTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kUnlockDiscardableTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id) {
    SetHeader();
    texture_id = _texture_id;
  }

  void* Set(void* cmd, GLuint _texture_id) {
    static_cast<ValueType*>(cmd)->Init(_texture_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
};

static_assert(sizeof(UnlockDiscardableTextureCHROMIUM) == 8,
              "size of UnlockDiscardableTextureCHROMIUM should be 8");
static_assert(offsetof(UnlockDiscardableTextureCHROMIUM, header) == 0,
              "offset of UnlockDiscardableTextureCHROMIUM header should be 0");
static_assert(
    offsetof(UnlockDiscardableTextureCHROMIUM, texture_id) == 4,
    "offset of UnlockDiscardableTextureCHROMIUM texture_id should be 4");

struct LockDiscardableTextureCHROMIUM {
  typedef LockDiscardableTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kLockDiscardableTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id) {
    SetHeader();
    texture_id = _texture_id;
  }

  void* Set(void* cmd, GLuint _texture_id) {
    static_cast<ValueType*>(cmd)->Init(_texture_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
};

static_assert(sizeof(LockDiscardableTextureCHROMIUM) == 8,
              "size of LockDiscardableTextureCHROMIUM should be 8");
static_assert(offsetof(LockDiscardableTextureCHROMIUM, header) == 0,
              "offset of LockDiscardableTextureCHROMIUM header should be 0");
static_assert(
    offsetof(LockDiscardableTextureCHROMIUM, texture_id) == 4,
    "offset of LockDiscardableTextureCHROMIUM texture_id should be 4");

struct TexStorage2DImageCHROMIUM {
  typedef TexStorage2DImageCHROMIUM ValueType;
  static const CommandId kCmdId = kTexStorage2DImageCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    internalFormat = _internalFormat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _internalFormat, _width,
                                       _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t internalFormat;
  int32_t width;
  int32_t height;
  static const uint32_t bufferUsage = GL_SCANOUT_CHROMIUM;
};

static_assert(sizeof(TexStorage2DImageCHROMIUM) == 20,
              "size of TexStorage2DImageCHROMIUM should be 20");
static_assert(offsetof(TexStorage2DImageCHROMIUM, header) == 0,
              "offset of TexStorage2DImageCHROMIUM header should be 0");
static_assert(offsetof(TexStorage2DImageCHROMIUM, target) == 4,
              "offset of TexStorage2DImageCHROMIUM target should be 4");
static_assert(offsetof(TexStorage2DImageCHROMIUM, internalFormat) == 8,
              "offset of TexStorage2DImageCHROMIUM internalFormat should be 8");
static_assert(offsetof(TexStorage2DImageCHROMIUM, width) == 12,
              "offset of TexStorage2DImageCHROMIUM width should be 12");
static_assert(offsetof(TexStorage2DImageCHROMIUM, height) == 16,
              "offset of TexStorage2DImageCHROMIUM height should be 16");

struct SetColorSpaceMetadataCHROMIUM {
  typedef SetColorSpaceMetadataCHROMIUM ValueType;
  static const CommandId kCmdId = kSetColorSpaceMetadataCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id,
            GLuint _shm_id,
            GLuint _shm_offset,
            GLsizei _color_space_size) {
    SetHeader();
    texture_id = _texture_id;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
    color_space_size = _color_space_size;
  }

  void* Set(void* cmd,
            GLuint _texture_id,
            GLuint _shm_id,
            GLuint _shm_offset,
            GLsizei _color_space_size) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _shm_id, _shm_offset,
                                       _color_space_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t shm_id;
  uint32_t shm_offset;
  int32_t color_space_size;
};

static_assert(sizeof(SetColorSpaceMetadataCHROMIUM) == 20,
              "size of SetColorSpaceMetadataCHROMIUM should be 20");
static_assert(offsetof(SetColorSpaceMetadataCHROMIUM, header) == 0,
              "offset of SetColorSpaceMetadataCHROMIUM header should be 0");
static_assert(offsetof(SetColorSpaceMetadataCHROMIUM, texture_id) == 4,
              "offset of SetColorSpaceMetadataCHROMIUM texture_id should be 4");
static_assert(offsetof(SetColorSpaceMetadataCHROMIUM, shm_id) == 8,
              "offset of SetColorSpaceMetadataCHROMIUM shm_id should be 8");
static_assert(
    offsetof(SetColorSpaceMetadataCHROMIUM, shm_offset) == 12,
    "offset of SetColorSpaceMetadataCHROMIUM shm_offset should be 12");
static_assert(
    offsetof(SetColorSpaceMetadataCHROMIUM, color_space_size) == 16,
    "offset of SetColorSpaceMetadataCHROMIUM color_space_size should be 16");

struct WindowRectanglesEXTImmediate {
  typedef WindowRectanglesEXTImmediate ValueType;
  static const CommandId kCmdId = kWindowRectanglesEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLint) * 4 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLenum _mode, GLsizei _count, const GLint* _box) {
    SetHeader(_count);
    mode = _mode;
    count = _count;
    memcpy(ImmediateDataAddress(this), _box, ComputeDataSize(_count));
  }

  void* Set(void* cmd, GLenum _mode, GLsizei _count, const GLint* _box) {
    static_cast<ValueType*>(cmd)->Init(_mode, _count, _box);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t mode;
  int32_t count;
};

static_assert(sizeof(WindowRectanglesEXTImmediate) == 12,
              "size of WindowRectanglesEXTImmediate should be 12");
static_assert(offsetof(WindowRectanglesEXTImmediate, header) == 0,
              "offset of WindowRectanglesEXTImmediate header should be 0");
static_assert(offsetof(WindowRectanglesEXTImmediate, mode) == 4,
              "offset of WindowRectanglesEXTImmediate mode should be 4");
static_assert(offsetof(WindowRectanglesEXTImmediate, count) == 8,
              "offset of WindowRectanglesEXTImmediate count should be 8");

struct CreateGpuFenceINTERNAL {
  typedef CreateGpuFenceINTERNAL ValueType;
  static const CommandId kCmdId = kCreateGpuFenceINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _gpu_fence_id) {
    SetHeader();
    gpu_fence_id = _gpu_fence_id;
  }

  void* Set(void* cmd, GLuint _gpu_fence_id) {
    static_cast<ValueType*>(cmd)->Init(_gpu_fence_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t gpu_fence_id;
};

static_assert(sizeof(CreateGpuFenceINTERNAL) == 8,
              "size of CreateGpuFenceINTERNAL should be 8");
static_assert(offsetof(CreateGpuFenceINTERNAL, header) == 0,
              "offset of CreateGpuFenceINTERNAL header should be 0");
static_assert(offsetof(CreateGpuFenceINTERNAL, gpu_fence_id) == 4,
              "offset of CreateGpuFenceINTERNAL gpu_fence_id should be 4");

struct WaitGpuFenceCHROMIUM {
  typedef WaitGpuFenceCHROMIUM ValueType;
  static const CommandId kCmdId = kWaitGpuFenceCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _gpu_fence_id) {
    SetHeader();
    gpu_fence_id = _gpu_fence_id;
  }

  void* Set(void* cmd, GLuint _gpu_fence_id) {
    static_cast<ValueType*>(cmd)->Init(_gpu_fence_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t gpu_fence_id;
};

static_assert(sizeof(WaitGpuFenceCHROMIUM) == 8,
              "size of WaitGpuFenceCHROMIUM should be 8");
static_assert(offsetof(WaitGpuFenceCHROMIUM, header) == 0,
              "offset of WaitGpuFenceCHROMIUM header should be 0");
static_assert(offsetof(WaitGpuFenceCHROMIUM, gpu_fence_id) == 4,
              "offset of WaitGpuFenceCHROMIUM gpu_fence_id should be 4");

struct DestroyGpuFenceCHROMIUM {
  typedef DestroyGpuFenceCHROMIUM ValueType;
  static const CommandId kCmdId = kDestroyGpuFenceCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _gpu_fence_id) {
    SetHeader();
    gpu_fence_id = _gpu_fence_id;
  }

  void* Set(void* cmd, GLuint _gpu_fence_id) {
    static_cast<ValueType*>(cmd)->Init(_gpu_fence_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t gpu_fence_id;
};

static_assert(sizeof(DestroyGpuFenceCHROMIUM) == 8,
              "size of DestroyGpuFenceCHROMIUM should be 8");
static_assert(offsetof(DestroyGpuFenceCHROMIUM, header) == 0,
              "offset of DestroyGpuFenceCHROMIUM header should be 0");
static_assert(offsetof(DestroyGpuFenceCHROMIUM, gpu_fence_id) == 4,
              "offset of DestroyGpuFenceCHROMIUM gpu_fence_id should be 4");

struct SetReadbackBufferShadowAllocationINTERNAL {
  typedef SetReadbackBufferShadowAllocationINTERNAL ValueType;
  static const CommandId kCmdId = kSetReadbackBufferShadowAllocationINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _buffer_id,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _size) {
    SetHeader();
    buffer_id = _buffer_id;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
    size = _size;
  }

  void* Set(void* cmd,
            GLuint _buffer_id,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _size) {
    static_cast<ValueType*>(cmd)->Init(_buffer_id, _shm_id, _shm_offset, _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t buffer_id;
  int32_t shm_id;
  uint32_t shm_offset;
  uint32_t size;
};

static_assert(sizeof(SetReadbackBufferShadowAllocationINTERNAL) == 20,
              "size of SetReadbackBufferShadowAllocationINTERNAL should be 20");
static_assert(
    offsetof(SetReadbackBufferShadowAllocationINTERNAL, header) == 0,
    "offset of SetReadbackBufferShadowAllocationINTERNAL header should be 0");
static_assert(offsetof(SetReadbackBufferShadowAllocationINTERNAL, buffer_id) ==
                  4,
              "offset of SetReadbackBufferShadowAllocationINTERNAL buffer_id "
              "should be 4");
static_assert(
    offsetof(SetReadbackBufferShadowAllocationINTERNAL, shm_id) == 8,
    "offset of SetReadbackBufferShadowAllocationINTERNAL shm_id should be 8");
static_assert(offsetof(SetReadbackBufferShadowAllocationINTERNAL, shm_offset) ==
                  12,
              "offset of SetReadbackBufferShadowAllocationINTERNAL shm_offset "
              "should be 12");
static_assert(
    offsetof(SetReadbackBufferShadowAllocationINTERNAL, size) == 16,
    "offset of SetReadbackBufferShadowAllocationINTERNAL size should be 16");

struct FramebufferTextureMultiviewOVR {
  typedef FramebufferTextureMultiviewOVR ValueType;
  static const CommandId kCmdId = kFramebufferTextureMultiviewOVR;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _attachment,
            GLuint _texture,
            GLint _level,
            GLint _baseViewIndex,
            GLsizei _numViews) {
    SetHeader();
    target = _target;
    attachment = _attachment;
    texture = _texture;
    level = _level;
    baseViewIndex = _baseViewIndex;
    numViews = _numViews;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _attachment,
            GLuint _texture,
            GLint _level,
            GLint _baseViewIndex,
            GLsizei _numViews) {
    static_cast<ValueType*>(cmd)->Init(_target, _attachment, _texture, _level,
                                       _baseViewIndex, _numViews);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t attachment;
  uint32_t texture;
  int32_t level;
  int32_t baseViewIndex;
  int32_t numViews;
};

static_assert(sizeof(FramebufferTextureMultiviewOVR) == 28,
              "size of FramebufferTextureMultiviewOVR should be 28");
static_assert(offsetof(FramebufferTextureMultiviewOVR, header) == 0,
              "offset of FramebufferTextureMultiviewOVR header should be 0");
static_assert(offsetof(FramebufferTextureMultiviewOVR, target) == 4,
              "offset of FramebufferTextureMultiviewOVR target should be 4");
static_assert(
    offsetof(FramebufferTextureMultiviewOVR, attachment) == 8,
    "offset of FramebufferTextureMultiviewOVR attachment should be 8");
static_assert(offsetof(FramebufferTextureMultiviewOVR, texture) == 12,
              "offset of FramebufferTextureMultiviewOVR texture should be 12");
static_assert(offsetof(FramebufferTextureMultiviewOVR, level) == 16,
              "offset of FramebufferTextureMultiviewOVR level should be 16");
static_assert(
    offsetof(FramebufferTextureMultiviewOVR, baseViewIndex) == 20,
    "offset of FramebufferTextureMultiviewOVR baseViewIndex should be 20");
static_assert(offsetof(FramebufferTextureMultiviewOVR, numViews) == 24,
              "offset of FramebufferTextureMultiviewOVR numViews should be 24");

struct MaxShaderCompilerThreadsKHR {
  typedef MaxShaderCompilerThreadsKHR ValueType;
  static const CommandId kCmdId = kMaxShaderCompilerThreadsKHR;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _count) {
    SetHeader();
    count = _count;
  }

  void* Set(void* cmd, GLuint _count) {
    static_cast<ValueType*>(cmd)->Init(_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t count;
};

static_assert(sizeof(MaxShaderCompilerThreadsKHR) == 8,
              "size of MaxShaderCompilerThreadsKHR should be 8");
static_assert(offsetof(MaxShaderCompilerThreadsKHR, header) == 0,
              "offset of MaxShaderCompilerThreadsKHR header should be 0");
static_assert(offsetof(MaxShaderCompilerThreadsKHR, count) == 4,
              "offset of MaxShaderCompilerThreadsKHR count should be 4");

struct CreateAndTexStorage2DSharedImageINTERNALImmediate {
  typedef CreateAndTexStorage2DSharedImageINTERNALImmediate ValueType;
  static const CommandId kCmdId =
      kCreateAndTexStorage2DSharedImageINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _texture, GLenum _internalformat, const GLbyte* _mailbox) {
    SetHeader();
    texture = _texture;
    internalformat = _internalformat;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _texture,
            GLenum _internalformat,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(_texture, _internalformat, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t texture;
  uint32_t internalformat;
};

static_assert(
    sizeof(CreateAndTexStorage2DSharedImageINTERNALImmediate) == 12,
    "size of CreateAndTexStorage2DSharedImageINTERNALImmediate should be 12");
static_assert(offsetof(CreateAndTexStorage2DSharedImageINTERNALImmediate,
                       header) == 0,
              "offset of CreateAndTexStorage2DSharedImageINTERNALImmediate "
              "header should be 0");
static_assert(offsetof(CreateAndTexStorage2DSharedImageINTERNALImmediate,
                       texture) == 4,
              "offset of CreateAndTexStorage2DSharedImageINTERNALImmediate "
              "texture should be 4");
static_assert(offsetof(CreateAndTexStorage2DSharedImageINTERNALImmediate,
                       internalformat) == 8,
              "offset of CreateAndTexStorage2DSharedImageINTERNALImmediate "
              "internalformat should be 8");

struct BeginSharedImageAccessDirectCHROMIUM {
  typedef BeginSharedImageAccessDirectCHROMIUM ValueType;
  static const CommandId kCmdId = kBeginSharedImageAccessDirectCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture, GLenum _mode) {
    SetHeader();
    texture = _texture;
    mode = _mode;
  }

  void* Set(void* cmd, GLuint _texture, GLenum _mode) {
    static_cast<ValueType*>(cmd)->Init(_texture, _mode);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture;
  uint32_t mode;
};

static_assert(sizeof(BeginSharedImageAccessDirectCHROMIUM) == 12,
              "size of BeginSharedImageAccessDirectCHROMIUM should be 12");
static_assert(
    offsetof(BeginSharedImageAccessDirectCHROMIUM, header) == 0,
    "offset of BeginSharedImageAccessDirectCHROMIUM header should be 0");
static_assert(
    offsetof(BeginSharedImageAccessDirectCHROMIUM, texture) == 4,
    "offset of BeginSharedImageAccessDirectCHROMIUM texture should be 4");
static_assert(
    offsetof(BeginSharedImageAccessDirectCHROMIUM, mode) == 8,
    "offset of BeginSharedImageAccessDirectCHROMIUM mode should be 8");

struct EndSharedImageAccessDirectCHROMIUM {
  typedef EndSharedImageAccessDirectCHROMIUM ValueType;
  static const CommandId kCmdId = kEndSharedImageAccessDirectCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture) {
    SetHeader();
    texture = _texture;
  }

  void* Set(void* cmd, GLuint _texture) {
    static_cast<ValueType*>(cmd)->Init(_texture);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture;
};

static_assert(sizeof(EndSharedImageAccessDirectCHROMIUM) == 8,
              "size of EndSharedImageAccessDirectCHROMIUM should be 8");
static_assert(
    offsetof(EndSharedImageAccessDirectCHROMIUM, header) == 0,
    "offset of EndSharedImageAccessDirectCHROMIUM header should be 0");
static_assert(
    offsetof(EndSharedImageAccessDirectCHROMIUM, texture) == 4,
    "offset of EndSharedImageAccessDirectCHROMIUM texture should be 4");

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_AUTOGEN_H_
