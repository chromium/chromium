// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_

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

struct BeginRasterCHROMIUMImmediate {
  typedef BeginRasterCHROMIUMImmediate ValueType;
  static const CommandId kCmdId = kBeginRasterCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLfloat _r,
            GLfloat _g,
            GLfloat _b,
            GLfloat _a,
            GLboolean _needs_clear,
            GLuint _msaa_sample_count,
            gpu::raster::MsaaMode _msaa_mode,
            GLboolean _can_use_lcd_text,
            GLboolean _visible,
            GLfloat _hdr_headroom,
            const GLbyte* _mailbox) {
    SetHeader();
    r = _r;
    g = _g;
    b = _b;
    a = _a;
    needs_clear = _needs_clear;
    msaa_sample_count = _msaa_sample_count;
    msaa_mode = _msaa_mode;
    can_use_lcd_text = _can_use_lcd_text;
    visible = _visible;
    hdr_headroom = _hdr_headroom;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLfloat _r,
            GLfloat _g,
            GLfloat _b,
            GLfloat _a,
            GLboolean _needs_clear,
            GLuint _msaa_sample_count,
            gpu::raster::MsaaMode _msaa_mode,
            GLboolean _can_use_lcd_text,
            GLboolean _visible,
            GLfloat _hdr_headroom,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(
        _r, _g, _b, _a, _needs_clear, _msaa_sample_count, _msaa_mode,
        _can_use_lcd_text, _visible, _hdr_headroom, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  float r;
  float g;
  float b;
  float a;
  uint32_t needs_clear;
  uint32_t msaa_sample_count;
  uint32_t msaa_mode;
  uint32_t can_use_lcd_text;
  uint32_t visible;
  float hdr_headroom;
};

static_assert(sizeof(BeginRasterCHROMIUMImmediate) == 44,
              "size of BeginRasterCHROMIUMImmediate should be 44");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, header) == 0,
              "offset of BeginRasterCHROMIUMImmediate header should be 0");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, r) == 4,
              "offset of BeginRasterCHROMIUMImmediate r should be 4");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, g) == 8,
              "offset of BeginRasterCHROMIUMImmediate g should be 8");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, b) == 12,
              "offset of BeginRasterCHROMIUMImmediate b should be 12");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, a) == 16,
              "offset of BeginRasterCHROMIUMImmediate a should be 16");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, needs_clear) == 20,
    "offset of BeginRasterCHROMIUMImmediate needs_clear should be 20");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, msaa_sample_count) == 24,
    "offset of BeginRasterCHROMIUMImmediate msaa_sample_count should be 24");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, msaa_mode) == 28,
              "offset of BeginRasterCHROMIUMImmediate msaa_mode should be 28");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, can_use_lcd_text) == 32,
    "offset of BeginRasterCHROMIUMImmediate can_use_lcd_text should be 32");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, visible) == 36,
              "offset of BeginRasterCHROMIUMImmediate visible should be 36");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, hdr_headroom) == 40,
    "offset of BeginRasterCHROMIUMImmediate hdr_headroom should be 40");

struct RasterCHROMIUM {
  typedef RasterCHROMIUM ValueType;
  static const CommandId kCmdId = kRasterCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _raster_shm_id,
            GLuint _raster_shm_offset,
            GLsizeiptr _raster_shm_size,
            GLuint _font_shm_id,
            GLuint _font_shm_offset,
            GLsizeiptr _font_shm_size) {
    SetHeader();
    raster_shm_id = _raster_shm_id;
    raster_shm_offset = _raster_shm_offset;
    raster_shm_size = _raster_shm_size;
    font_shm_id = _font_shm_id;
    font_shm_offset = _font_shm_offset;
    font_shm_size = _font_shm_size;
  }

  void* Set(void* cmd,
            GLuint _raster_shm_id,
            GLuint _raster_shm_offset,
            GLsizeiptr _raster_shm_size,
            GLuint _font_shm_id,
            GLuint _font_shm_offset,
            GLsizeiptr _font_shm_size) {
    static_cast<ValueType*>(cmd)->Init(_raster_shm_id, _raster_shm_offset,
                                       _raster_shm_size, _font_shm_id,
                                       _font_shm_offset, _font_shm_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t raster_shm_id;
  uint32_t raster_shm_offset;
  int32_t raster_shm_size;
  uint32_t font_shm_id;
  uint32_t font_shm_offset;
  int32_t font_shm_size;
};

static_assert(sizeof(RasterCHROMIUM) == 28,
              "size of RasterCHROMIUM should be 28");
static_assert(offsetof(RasterCHROMIUM, header) == 0,
              "offset of RasterCHROMIUM header should be 0");
static_assert(offsetof(RasterCHROMIUM, raster_shm_id) == 4,
              "offset of RasterCHROMIUM raster_shm_id should be 4");
static_assert(offsetof(RasterCHROMIUM, raster_shm_offset) == 8,
              "offset of RasterCHROMIUM raster_shm_offset should be 8");
static_assert(offsetof(RasterCHROMIUM, raster_shm_size) == 12,
              "offset of RasterCHROMIUM raster_shm_size should be 12");
static_assert(offsetof(RasterCHROMIUM, font_shm_id) == 16,
              "offset of RasterCHROMIUM font_shm_id should be 16");
static_assert(offsetof(RasterCHROMIUM, font_shm_offset) == 20,
              "offset of RasterCHROMIUM font_shm_offset should be 20");
static_assert(offsetof(RasterCHROMIUM, font_shm_size) == 24,
              "offset of RasterCHROMIUM font_shm_size should be 24");

struct EndRasterCHROMIUM {
  typedef EndRasterCHROMIUM ValueType;
  static const CommandId kCmdId = kEndRasterCHROMIUM;
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

static_assert(sizeof(EndRasterCHROMIUM) == 4,
              "size of EndRasterCHROMIUM should be 4");
static_assert(offsetof(EndRasterCHROMIUM, header) == 0,
              "offset of EndRasterCHROMIUM header should be 0");

struct CreateTransferCacheEntryINTERNAL {
  typedef CreateTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kCreateTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type,
            GLuint _entry_id,
            GLuint _handle_shm_id,
            GLuint _handle_shm_offset,
            GLuint _data_shm_id,
            GLuint _data_shm_offset,
            GLuint _data_size) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
    handle_shm_id = _handle_shm_id;
    handle_shm_offset = _handle_shm_offset;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
    data_size = _data_size;
  }

  void* Set(void* cmd,
            GLuint _entry_type,
            GLuint _entry_id,
            GLuint _handle_shm_id,
            GLuint _handle_shm_offset,
            GLuint _data_shm_id,
            GLuint _data_shm_offset,
            GLuint _data_size) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id, _handle_shm_id,
                                       _handle_shm_offset, _data_shm_id,
                                       _data_shm_offset, _data_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
  uint32_t handle_shm_id;
  uint32_t handle_shm_offset;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  uint32_t data_size;
};

static_assert(sizeof(CreateTransferCacheEntryINTERNAL) == 32,
              "size of CreateTransferCacheEntryINTERNAL should be 32");
static_assert(offsetof(CreateTransferCacheEntryINTERNAL, header) == 0,
              "offset of CreateTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of CreateTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of CreateTransferCacheEntryINTERNAL entry_id should be 8");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, handle_shm_id) == 12,
    "offset of CreateTransferCacheEntryINTERNAL handle_shm_id should be 12");
static_assert(offsetof(CreateTransferCacheEntryINTERNAL, handle_shm_offset) ==
                  16,
              "offset of CreateTransferCacheEntryINTERNAL handle_shm_offset "
              "should be 16");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_shm_id) == 20,
    "offset of CreateTransferCacheEntryINTERNAL data_shm_id should be 20");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_shm_offset) == 24,
    "offset of CreateTransferCacheEntryINTERNAL data_shm_offset should be 24");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_size) == 28,
    "offset of CreateTransferCacheEntryINTERNAL data_size should be 28");

struct DeleteTransferCacheEntryINTERNAL {
  typedef DeleteTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kDeleteTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type, GLuint _entry_id) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
  }

  void* Set(void* cmd, GLuint _entry_type, GLuint _entry_id) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
};

static_assert(sizeof(DeleteTransferCacheEntryINTERNAL) == 12,
              "size of DeleteTransferCacheEntryINTERNAL should be 12");
static_assert(offsetof(DeleteTransferCacheEntryINTERNAL, header) == 0,
              "offset of DeleteTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(DeleteTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of DeleteTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(DeleteTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of DeleteTransferCacheEntryINTERNAL entry_id should be 8");

struct UnlockTransferCacheEntryINTERNAL {
  typedef UnlockTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kUnlockTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type, GLuint _entry_id) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
  }

  void* Set(void* cmd, GLuint _entry_type, GLuint _entry_id) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
};

static_assert(sizeof(UnlockTransferCacheEntryINTERNAL) == 12,
              "size of UnlockTransferCacheEntryINTERNAL should be 12");
static_assert(offsetof(UnlockTransferCacheEntryINTERNAL, header) == 0,
              "offset of UnlockTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(UnlockTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of UnlockTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(UnlockTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of UnlockTransferCacheEntryINTERNAL entry_id should be 8");

struct DeletePaintCachePathsINTERNALImmediate {
  typedef DeletePaintCachePathsINTERNALImmediate ValueType;
  static const CommandId kCmdId = kDeletePaintCachePathsINTERNALImmediate;
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

static_assert(sizeof(DeletePaintCachePathsINTERNALImmediate) == 8,
              "size of DeletePaintCachePathsINTERNALImmediate should be 8");
static_assert(
    offsetof(DeletePaintCachePathsINTERNALImmediate, header) == 0,
    "offset of DeletePaintCachePathsINTERNALImmediate header should be 0");
static_assert(offsetof(DeletePaintCachePathsINTERNALImmediate, n) == 4,
              "offset of DeletePaintCachePathsINTERNALImmediate n should be 4");

struct DeletePaintCachePathsINTERNAL {
  typedef DeletePaintCachePathsINTERNAL ValueType;
  static const CommandId kCmdId = kDeletePaintCachePathsINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizei _n, uint32_t _ids_shm_id, uint32_t _ids_shm_offset) {
    SetHeader();
    n = _n;
    ids_shm_id = _ids_shm_id;
    ids_shm_offset = _ids_shm_offset;
  }

  void* Set(void* cmd,
            GLsizei _n,
            uint32_t _ids_shm_id,
            uint32_t _ids_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_n, _ids_shm_id, _ids_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t n;
  uint32_t ids_shm_id;
  uint32_t ids_shm_offset;
};

static_assert(sizeof(DeletePaintCachePathsINTERNAL) == 16,
              "size of DeletePaintCachePathsINTERNAL should be 16");
static_assert(offsetof(DeletePaintCachePathsINTERNAL, header) == 0,
              "offset of DeletePaintCachePathsINTERNAL header should be 0");
static_assert(offsetof(DeletePaintCachePathsINTERNAL, n) == 4,
              "offset of DeletePaintCachePathsINTERNAL n should be 4");
static_assert(offsetof(DeletePaintCachePathsINTERNAL, ids_shm_id) == 8,
              "offset of DeletePaintCachePathsINTERNAL ids_shm_id should be 8");
static_assert(
    offsetof(DeletePaintCachePathsINTERNAL, ids_shm_offset) == 12,
    "offset of DeletePaintCachePathsINTERNAL ids_shm_offset should be 12");

struct ClearPaintCacheINTERNAL {
  typedef ClearPaintCacheINTERNAL ValueType;
  static const CommandId kCmdId = kClearPaintCacheINTERNAL;
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

static_assert(sizeof(ClearPaintCacheINTERNAL) == 4,
              "size of ClearPaintCacheINTERNAL should be 4");
static_assert(offsetof(ClearPaintCacheINTERNAL, header) == 0,
              "offset of ClearPaintCacheINTERNAL header should be 0");

struct CopySharedImageINTERNALImmediate {
  typedef CopySharedImageINTERNALImmediate ValueType;
  static const CommandId kCmdId = kCopySharedImageINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 32);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLboolean _unpack_flip_y,
            const GLbyte* _mailboxes) {
    SetHeader();
    xoffset = _xoffset;
    yoffset = _yoffset;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    unpack_flip_y = _unpack_flip_y;
    memcpy(ImmediateDataAddress(this), _mailboxes, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLboolean _unpack_flip_y,
            const GLbyte* _mailboxes) {
    static_cast<ValueType*>(cmd)->Init(_xoffset, _yoffset, _x, _y, _width,
                                       _height, _unpack_flip_y, _mailboxes);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t xoffset;
  int32_t yoffset;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  uint32_t unpack_flip_y;
};

static_assert(sizeof(CopySharedImageINTERNALImmediate) == 32,
              "size of CopySharedImageINTERNALImmediate should be 32");
static_assert(offsetof(CopySharedImageINTERNALImmediate, header) == 0,
              "offset of CopySharedImageINTERNALImmediate header should be 0");
static_assert(offsetof(CopySharedImageINTERNALImmediate, xoffset) == 4,
              "offset of CopySharedImageINTERNALImmediate xoffset should be 4");
static_assert(offsetof(CopySharedImageINTERNALImmediate, yoffset) == 8,
              "offset of CopySharedImageINTERNALImmediate yoffset should be 8");
static_assert(offsetof(CopySharedImageINTERNALImmediate, x) == 12,
              "offset of CopySharedImageINTERNALImmediate x should be 12");
static_assert(offsetof(CopySharedImageINTERNALImmediate, y) == 16,
              "offset of CopySharedImageINTERNALImmediate y should be 16");
static_assert(offsetof(CopySharedImageINTERNALImmediate, width) == 20,
              "offset of CopySharedImageINTERNALImmediate width should be 20");
static_assert(offsetof(CopySharedImageINTERNALImmediate, height) == 24,
              "offset of CopySharedImageINTERNALImmediate height should be 24");
static_assert(
    offsetof(CopySharedImageINTERNALImmediate, unpack_flip_y) == 28,
    "offset of CopySharedImageINTERNALImmediate unpack_flip_y should be 28");

struct WritePixelsINTERNALImmediate {
  typedef WritePixelsINTERNALImmediate ValueType;
  static const CommandId kCmdId = kWritePixelsINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLint _x_offset,
            GLint _y_offset,
            GLuint _src_width,
            GLuint _src_height,
            GLuint _src_row_bytes,
            GLuint _src_sk_color_type,
            GLuint _src_sk_alpha_type,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _pixels_offset,
            const GLbyte* _mailbox) {
    SetHeader();
    x_offset = _x_offset;
    y_offset = _y_offset;
    src_width = _src_width;
    src_height = _src_height;
    src_row_bytes = _src_row_bytes;
    src_sk_color_type = _src_sk_color_type;
    src_sk_alpha_type = _src_sk_alpha_type;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
    pixels_offset = _pixels_offset;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLint _x_offset,
            GLint _y_offset,
            GLuint _src_width,
            GLuint _src_height,
            GLuint _src_row_bytes,
            GLuint _src_sk_color_type,
            GLuint _src_sk_alpha_type,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _pixels_offset,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(
        _x_offset, _y_offset, _src_width, _src_height, _src_row_bytes,
        _src_sk_color_type, _src_sk_alpha_type, _shm_id, _shm_offset,
        _pixels_offset, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t x_offset;
  int32_t y_offset;
  uint32_t src_width;
  uint32_t src_height;
  uint32_t src_row_bytes;
  uint32_t src_sk_color_type;
  uint32_t src_sk_alpha_type;
  int32_t shm_id;
  uint32_t shm_offset;
  uint32_t pixels_offset;
};

static_assert(sizeof(WritePixelsINTERNALImmediate) == 44,
              "size of WritePixelsINTERNALImmediate should be 44");
static_assert(offsetof(WritePixelsINTERNALImmediate, header) == 0,
              "offset of WritePixelsINTERNALImmediate header should be 0");
static_assert(offsetof(WritePixelsINTERNALImmediate, x_offset) == 4,
              "offset of WritePixelsINTERNALImmediate x_offset should be 4");
static_assert(offsetof(WritePixelsINTERNALImmediate, y_offset) == 8,
              "offset of WritePixelsINTERNALImmediate y_offset should be 8");
static_assert(offsetof(WritePixelsINTERNALImmediate, src_width) == 12,
              "offset of WritePixelsINTERNALImmediate src_width should be 12");
static_assert(offsetof(WritePixelsINTERNALImmediate, src_height) == 16,
              "offset of WritePixelsINTERNALImmediate src_height should be 16");
static_assert(
    offsetof(WritePixelsINTERNALImmediate, src_row_bytes) == 20,
    "offset of WritePixelsINTERNALImmediate src_row_bytes should be 20");
static_assert(
    offsetof(WritePixelsINTERNALImmediate, src_sk_color_type) == 24,
    "offset of WritePixelsINTERNALImmediate src_sk_color_type should be 24");
static_assert(
    offsetof(WritePixelsINTERNALImmediate, src_sk_alpha_type) == 28,
    "offset of WritePixelsINTERNALImmediate src_sk_alpha_type should be 28");
static_assert(offsetof(WritePixelsINTERNALImmediate, shm_id) == 32,
              "offset of WritePixelsINTERNALImmediate shm_id should be 32");
static_assert(offsetof(WritePixelsINTERNALImmediate, shm_offset) == 36,
              "offset of WritePixelsINTERNALImmediate shm_offset should be 36");
static_assert(
    offsetof(WritePixelsINTERNALImmediate, pixels_offset) == 40,
    "offset of WritePixelsINTERNALImmediate pixels_offset should be 40");

struct WritePixelsYUVINTERNALImmediate {
  typedef WritePixelsYUVINTERNALImmediate ValueType;
  static const CommandId kCmdId = kWritePixelsYUVINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _src_width,
            GLuint _src_height,
            GLuint _src_row_bytes_plane1,
            GLuint _src_row_bytes_plane2,
            GLuint _src_row_bytes_plane3,
            GLuint _src_row_bytes_plane4,
            GLuint _src_yuv_plane_config,
            GLuint _src_yuv_subsampling,
            GLuint _src_yuv_datatype,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _plane2_offset,
            GLuint _plane3_offset,
            GLuint _plane4_offset,
            const GLbyte* _mailbox) {
    SetHeader();
    src_width = _src_width;
    src_height = _src_height;
    src_row_bytes_plane1 = _src_row_bytes_plane1;
    src_row_bytes_plane2 = _src_row_bytes_plane2;
    src_row_bytes_plane3 = _src_row_bytes_plane3;
    src_row_bytes_plane4 = _src_row_bytes_plane4;
    src_yuv_plane_config = _src_yuv_plane_config;
    src_yuv_subsampling = _src_yuv_subsampling;
    src_yuv_datatype = _src_yuv_datatype;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
    plane2_offset = _plane2_offset;
    plane3_offset = _plane3_offset;
    plane4_offset = _plane4_offset;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _src_width,
            GLuint _src_height,
            GLuint _src_row_bytes_plane1,
            GLuint _src_row_bytes_plane2,
            GLuint _src_row_bytes_plane3,
            GLuint _src_row_bytes_plane4,
            GLuint _src_yuv_plane_config,
            GLuint _src_yuv_subsampling,
            GLuint _src_yuv_datatype,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _plane2_offset,
            GLuint _plane3_offset,
            GLuint _plane4_offset,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(
        _src_width, _src_height, _src_row_bytes_plane1, _src_row_bytes_plane2,
        _src_row_bytes_plane3, _src_row_bytes_plane4, _src_yuv_plane_config,
        _src_yuv_subsampling, _src_yuv_datatype, _shm_id, _shm_offset,
        _plane2_offset, _plane3_offset, _plane4_offset, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t src_width;
  uint32_t src_height;
  uint32_t src_row_bytes_plane1;
  uint32_t src_row_bytes_plane2;
  uint32_t src_row_bytes_plane3;
  uint32_t src_row_bytes_plane4;
  uint32_t src_yuv_plane_config;
  uint32_t src_yuv_subsampling;
  uint32_t src_yuv_datatype;
  int32_t shm_id;
  uint32_t shm_offset;
  uint32_t plane2_offset;
  uint32_t plane3_offset;
  uint32_t plane4_offset;
};

static_assert(sizeof(WritePixelsYUVINTERNALImmediate) == 60,
              "size of WritePixelsYUVINTERNALImmediate should be 60");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, header) == 0,
              "offset of WritePixelsYUVINTERNALImmediate header should be 0");
static_assert(
    offsetof(WritePixelsYUVINTERNALImmediate, src_width) == 4,
    "offset of WritePixelsYUVINTERNALImmediate src_width should be 4");
static_assert(
    offsetof(WritePixelsYUVINTERNALImmediate, src_height) == 8,
    "offset of WritePixelsYUVINTERNALImmediate src_height should be 8");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, src_row_bytes_plane1) ==
                  12,
              "offset of WritePixelsYUVINTERNALImmediate src_row_bytes_plane1 "
              "should be 12");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, src_row_bytes_plane2) ==
                  16,
              "offset of WritePixelsYUVINTERNALImmediate src_row_bytes_plane2 "
              "should be 16");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, src_row_bytes_plane3) ==
                  20,
              "offset of WritePixelsYUVINTERNALImmediate src_row_bytes_plane3 "
              "should be 20");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, src_row_bytes_plane4) ==
                  24,
              "offset of WritePixelsYUVINTERNALImmediate src_row_bytes_plane4 "
              "should be 24");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, src_yuv_plane_config) ==
                  28,
              "offset of WritePixelsYUVINTERNALImmediate src_yuv_plane_config "
              "should be 28");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, src_yuv_subsampling) ==
                  32,
              "offset of WritePixelsYUVINTERNALImmediate src_yuv_subsampling "
              "should be 32");
static_assert(
    offsetof(WritePixelsYUVINTERNALImmediate, src_yuv_datatype) == 36,
    "offset of WritePixelsYUVINTERNALImmediate src_yuv_datatype should be 36");
static_assert(offsetof(WritePixelsYUVINTERNALImmediate, shm_id) == 40,
              "offset of WritePixelsYUVINTERNALImmediate shm_id should be 40");
static_assert(
    offsetof(WritePixelsYUVINTERNALImmediate, shm_offset) == 44,
    "offset of WritePixelsYUVINTERNALImmediate shm_offset should be 44");
static_assert(
    offsetof(WritePixelsYUVINTERNALImmediate, plane2_offset) == 48,
    "offset of WritePixelsYUVINTERNALImmediate plane2_offset should be 48");
static_assert(
    offsetof(WritePixelsYUVINTERNALImmediate, plane3_offset) == 52,
    "offset of WritePixelsYUVINTERNALImmediate plane3_offset should be 52");
static_assert(
    offsetof(WritePixelsYUVINTERNALImmediate, plane4_offset) == 56,
    "offset of WritePixelsYUVINTERNALImmediate plane4_offset should be 56");

struct ReadbackARGBImagePixelsINTERNALImmediate {
  typedef ReadbackARGBImagePixelsINTERNALImmediate ValueType;
  static const CommandId kCmdId = kReadbackARGBImagePixelsINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef uint32_t Result;

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLint _src_x,
            GLint _src_y,
            GLint _plane_index,
            GLuint _dst_width,
            GLuint _dst_height,
            GLuint _row_bytes,
            GLuint _dst_sk_color_type,
            GLuint _dst_sk_alpha_type,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _color_space_offset,
            GLuint _pixels_offset,
            const GLbyte* _mailbox) {
    SetHeader();
    src_x = _src_x;
    src_y = _src_y;
    plane_index = _plane_index;
    dst_width = _dst_width;
    dst_height = _dst_height;
    row_bytes = _row_bytes;
    dst_sk_color_type = _dst_sk_color_type;
    dst_sk_alpha_type = _dst_sk_alpha_type;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
    color_space_offset = _color_space_offset;
    pixels_offset = _pixels_offset;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLint _src_x,
            GLint _src_y,
            GLint _plane_index,
            GLuint _dst_width,
            GLuint _dst_height,
            GLuint _row_bytes,
            GLuint _dst_sk_color_type,
            GLuint _dst_sk_alpha_type,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _color_space_offset,
            GLuint _pixels_offset,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(
        _src_x, _src_y, _plane_index, _dst_width, _dst_height, _row_bytes,
        _dst_sk_color_type, _dst_sk_alpha_type, _shm_id, _shm_offset,
        _color_space_offset, _pixels_offset, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t src_x;
  int32_t src_y;
  int32_t plane_index;
  uint32_t dst_width;
  uint32_t dst_height;
  uint32_t row_bytes;
  uint32_t dst_sk_color_type;
  uint32_t dst_sk_alpha_type;
  int32_t shm_id;
  uint32_t shm_offset;
  uint32_t color_space_offset;
  uint32_t pixels_offset;
};

static_assert(sizeof(ReadbackARGBImagePixelsINTERNALImmediate) == 52,
              "size of ReadbackARGBImagePixelsINTERNALImmediate should be 52");
static_assert(
    offsetof(ReadbackARGBImagePixelsINTERNALImmediate, header) == 0,
    "offset of ReadbackARGBImagePixelsINTERNALImmediate header should be 0");
static_assert(
    offsetof(ReadbackARGBImagePixelsINTERNALImmediate, src_x) == 4,
    "offset of ReadbackARGBImagePixelsINTERNALImmediate src_x should be 4");
static_assert(
    offsetof(ReadbackARGBImagePixelsINTERNALImmediate, src_y) == 8,
    "offset of ReadbackARGBImagePixelsINTERNALImmediate src_y should be 8");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate, plane_index) ==
                  12,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate plane_index "
              "should be 12");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate, dst_width) ==
                  16,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate dst_width "
              "should be 16");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate, dst_height) ==
                  20,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate dst_height "
              "should be 20");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate, row_bytes) ==
                  24,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate row_bytes "
              "should be 24");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate,
                       dst_sk_color_type) == 28,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate "
              "dst_sk_color_type should be 28");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate,
                       dst_sk_alpha_type) == 32,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate "
              "dst_sk_alpha_type should be 32");
static_assert(
    offsetof(ReadbackARGBImagePixelsINTERNALImmediate, shm_id) == 36,
    "offset of ReadbackARGBImagePixelsINTERNALImmediate shm_id should be 36");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate, shm_offset) ==
                  40,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate shm_offset "
              "should be 40");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate,
                       color_space_offset) == 44,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate "
              "color_space_offset should be 44");
static_assert(offsetof(ReadbackARGBImagePixelsINTERNALImmediate,
                       pixels_offset) == 48,
              "offset of ReadbackARGBImagePixelsINTERNALImmediate "
              "pixels_offset should be 48");

struct ReadbackYUVImagePixelsINTERNALImmediate {
  typedef ReadbackYUVImagePixelsINTERNALImmediate ValueType;
  static const CommandId kCmdId = kReadbackYUVImagePixelsINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  typedef uint32_t Result;

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _dst_width,
            GLuint _dst_height,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _y_offset,
            GLuint _y_stride,
            GLuint _u_offset,
            GLuint _u_stride,
            GLuint _v_offset,
            GLuint _v_stride,
            const GLbyte* _mailbox) {
    SetHeader();
    dst_width = _dst_width;
    dst_height = _dst_height;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
    y_offset = _y_offset;
    y_stride = _y_stride;
    u_offset = _u_offset;
    u_stride = _u_stride;
    v_offset = _v_offset;
    v_stride = _v_stride;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _dst_width,
            GLuint _dst_height,
            GLint _shm_id,
            GLuint _shm_offset,
            GLuint _y_offset,
            GLuint _y_stride,
            GLuint _u_offset,
            GLuint _u_stride,
            GLuint _v_offset,
            GLuint _v_stride,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(
        _dst_width, _dst_height, _shm_id, _shm_offset, _y_offset, _y_stride,
        _u_offset, _u_stride, _v_offset, _v_stride, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t dst_width;
  uint32_t dst_height;
  int32_t shm_id;
  uint32_t shm_offset;
  uint32_t y_offset;
  uint32_t y_stride;
  uint32_t u_offset;
  uint32_t u_stride;
  uint32_t v_offset;
  uint32_t v_stride;
};

static_assert(sizeof(ReadbackYUVImagePixelsINTERNALImmediate) == 44,
              "size of ReadbackYUVImagePixelsINTERNALImmediate should be 44");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, header) == 0,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate header should be 0");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, dst_width) == 4,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate dst_width should be 4");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, dst_height) == 8,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate dst_height should be 8");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, shm_id) == 12,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate shm_id should be 12");
static_assert(offsetof(ReadbackYUVImagePixelsINTERNALImmediate, shm_offset) ==
                  16,
              "offset of ReadbackYUVImagePixelsINTERNALImmediate shm_offset "
              "should be 16");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, y_offset) == 20,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate y_offset should be 20");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, y_stride) == 24,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate y_stride should be 24");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, u_offset) == 28,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate u_offset should be 28");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, u_stride) == 32,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate u_stride should be 32");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, v_offset) == 36,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate v_offset should be 36");
static_assert(
    offsetof(ReadbackYUVImagePixelsINTERNALImmediate, v_stride) == 40,
    "offset of ReadbackYUVImagePixelsINTERNALImmediate v_stride should be 40");

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

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_
