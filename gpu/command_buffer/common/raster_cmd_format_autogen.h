// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_

#define GL_SCANOUT_CHROMIUM 0x6000

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

struct InsertFenceSyncCHROMIUM {
  typedef InsertFenceSyncCHROMIUM ValueType;
  static const CommandId kCmdId = kInsertFenceSyncCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint64 _release_count) {
    SetHeader();
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_release_count), &release_count_0,
        &release_count_1);
  }

  void* Set(void* cmd, GLuint64 _release_count) {
    static_cast<ValueType*>(cmd)->Init(_release_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 release_count() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        release_count_0, release_count_1));
  }

  gpu::CommandHeader header;
  uint32_t release_count_0;
  uint32_t release_count_1;
};

static_assert(sizeof(InsertFenceSyncCHROMIUM) == 12,
              "size of InsertFenceSyncCHROMIUM should be 12");
static_assert(offsetof(InsertFenceSyncCHROMIUM, header) == 0,
              "offset of InsertFenceSyncCHROMIUM header should be 0");
static_assert(offsetof(InsertFenceSyncCHROMIUM, release_count_0) == 4,
              "offset of InsertFenceSyncCHROMIUM release_count_0 should be 4");
static_assert(offsetof(InsertFenceSyncCHROMIUM, release_count_1) == 8,
              "offset of InsertFenceSyncCHROMIUM release_count_1 should be 8");

struct WaitSyncTokenCHROMIUM {
  typedef WaitSyncTokenCHROMIUM ValueType;
  static const CommandId kCmdId = kWaitSyncTokenCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _namespace_id,
            GLuint64 _command_buffer_id,
            GLuint64 _release_count) {
    SetHeader();
    namespace_id = _namespace_id;
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_command_buffer_id), &command_buffer_id_0,
        &command_buffer_id_1);
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_release_count), &release_count_0,
        &release_count_1);
  }

  void* Set(void* cmd,
            GLint _namespace_id,
            GLuint64 _command_buffer_id,
            GLuint64 _release_count) {
    static_cast<ValueType*>(cmd)->Init(_namespace_id, _command_buffer_id,
                                       _release_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 command_buffer_id() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        command_buffer_id_0, command_buffer_id_1));
  }

  GLuint64 release_count() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        release_count_0, release_count_1));
  }

  gpu::CommandHeader header;
  int32_t namespace_id;
  uint32_t command_buffer_id_0;
  uint32_t command_buffer_id_1;
  uint32_t release_count_0;
  uint32_t release_count_1;
};

static_assert(sizeof(WaitSyncTokenCHROMIUM) == 24,
              "size of WaitSyncTokenCHROMIUM should be 24");
static_assert(offsetof(WaitSyncTokenCHROMIUM, header) == 0,
              "offset of WaitSyncTokenCHROMIUM header should be 0");
static_assert(offsetof(WaitSyncTokenCHROMIUM, namespace_id) == 4,
              "offset of WaitSyncTokenCHROMIUM namespace_id should be 4");
static_assert(
    offsetof(WaitSyncTokenCHROMIUM, command_buffer_id_0) == 8,
    "offset of WaitSyncTokenCHROMIUM command_buffer_id_0 should be 8");
static_assert(
    offsetof(WaitSyncTokenCHROMIUM, command_buffer_id_1) == 12,
    "offset of WaitSyncTokenCHROMIUM command_buffer_id_1 should be 12");
static_assert(offsetof(WaitSyncTokenCHROMIUM, release_count_0) == 16,
              "offset of WaitSyncTokenCHROMIUM release_count_0 should be 16");
static_assert(offsetof(WaitSyncTokenCHROMIUM, release_count_1) == 20,
              "offset of WaitSyncTokenCHROMIUM release_count_1 should be 20");

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

  void Init(GLuint _sk_color,
            GLuint _msaa_sample_count,
            GLboolean _can_use_lcd_text,
            GLint _color_type,
            GLuint _color_space_transfer_cache_id,
            const GLbyte* _mailbox) {
    SetHeader();
    sk_color = _sk_color;
    msaa_sample_count = _msaa_sample_count;
    can_use_lcd_text = _can_use_lcd_text;
    color_type = _color_type;
    color_space_transfer_cache_id = _color_space_transfer_cache_id;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _sk_color,
            GLuint _msaa_sample_count,
            GLboolean _can_use_lcd_text,
            GLint _color_type,
            GLuint _color_space_transfer_cache_id,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(
        _sk_color, _msaa_sample_count, _can_use_lcd_text, _color_type,
        _color_space_transfer_cache_id, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t sk_color;
  uint32_t msaa_sample_count;
  uint32_t can_use_lcd_text;
  int32_t color_type;
  uint32_t color_space_transfer_cache_id;
};

static_assert(sizeof(BeginRasterCHROMIUMImmediate) == 24,
              "size of BeginRasterCHROMIUMImmediate should be 24");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, header) == 0,
              "offset of BeginRasterCHROMIUMImmediate header should be 0");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, sk_color) == 4,
              "offset of BeginRasterCHROMIUMImmediate sk_color should be 4");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, msaa_sample_count) == 8,
    "offset of BeginRasterCHROMIUMImmediate msaa_sample_count should be 8");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, can_use_lcd_text) == 12,
    "offset of BeginRasterCHROMIUMImmediate can_use_lcd_text should be 12");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, color_type) == 16,
              "offset of BeginRasterCHROMIUMImmediate color_type should be 16");
static_assert(offsetof(BeginRasterCHROMIUMImmediate,
                       color_space_transfer_cache_id) == 20,
              "offset of BeginRasterCHROMIUMImmediate "
              "color_space_transfer_cache_id should be 20");

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

struct CreateTexture {
  typedef CreateTexture ValueType;
  static const CommandId kCmdId = kCreateTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(bool _use_buffer,
            gfx::BufferUsage _buffer_usage,
            viz::ResourceFormat _format,
            uint32_t _client_id) {
    SetHeader();
    use_buffer = _use_buffer;
    buffer_usage = static_cast<uint32_t>(_buffer_usage);
    format = static_cast<uint32_t>(_format);
    client_id = _client_id;
  }

  void* Set(void* cmd,
            bool _use_buffer,
            gfx::BufferUsage _buffer_usage,
            viz::ResourceFormat _format,
            uint32_t _client_id) {
    static_cast<ValueType*>(cmd)->Init(_use_buffer, _buffer_usage, _format,
                                       _client_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t use_buffer;
  uint32_t buffer_usage;
  uint32_t format;
  uint32_t client_id;
};

static_assert(sizeof(CreateTexture) == 20,
              "size of CreateTexture should be 20");
static_assert(offsetof(CreateTexture, header) == 0,
              "offset of CreateTexture header should be 0");
static_assert(offsetof(CreateTexture, use_buffer) == 4,
              "offset of CreateTexture use_buffer should be 4");
static_assert(offsetof(CreateTexture, buffer_usage) == 8,
              "offset of CreateTexture buffer_usage should be 8");
static_assert(offsetof(CreateTexture, format) == 12,
              "offset of CreateTexture format should be 12");
static_assert(offsetof(CreateTexture, client_id) == 16,
              "offset of CreateTexture client_id should be 16");

struct SetColorSpaceMetadata {
  typedef SetColorSpaceMetadata ValueType;
  static const CommandId kCmdId = kSetColorSpaceMetadata;
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

static_assert(sizeof(SetColorSpaceMetadata) == 20,
              "size of SetColorSpaceMetadata should be 20");
static_assert(offsetof(SetColorSpaceMetadata, header) == 0,
              "offset of SetColorSpaceMetadata header should be 0");
static_assert(offsetof(SetColorSpaceMetadata, texture_id) == 4,
              "offset of SetColorSpaceMetadata texture_id should be 4");
static_assert(offsetof(SetColorSpaceMetadata, shm_id) == 8,
              "offset of SetColorSpaceMetadata shm_id should be 8");
static_assert(offsetof(SetColorSpaceMetadata, shm_offset) == 12,
              "offset of SetColorSpaceMetadata shm_offset should be 12");
static_assert(offsetof(SetColorSpaceMetadata, color_space_size) == 16,
              "offset of SetColorSpaceMetadata color_space_size should be 16");

struct ProduceTextureDirectImmediate {
  typedef ProduceTextureDirectImmediate ValueType;
  static const CommandId kCmdId = kProduceTextureDirectImmediate;
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

static_assert(sizeof(ProduceTextureDirectImmediate) == 8,
              "size of ProduceTextureDirectImmediate should be 8");
static_assert(offsetof(ProduceTextureDirectImmediate, header) == 0,
              "offset of ProduceTextureDirectImmediate header should be 0");
static_assert(offsetof(ProduceTextureDirectImmediate, texture) == 4,
              "offset of ProduceTextureDirectImmediate texture should be 4");

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

  void Init(GLuint _texture_id,
            bool _use_buffer,
            gfx::BufferUsage _buffer_usage,
            viz::ResourceFormat _format,
            const GLbyte* _mailbox) {
    SetHeader();
    texture_id = _texture_id;
    use_buffer = _use_buffer;
    buffer_usage = static_cast<uint32_t>(_buffer_usage);
    format = static_cast<uint32_t>(_format);
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _texture_id,
            bool _use_buffer,
            gfx::BufferUsage _buffer_usage,
            viz::ResourceFormat _format,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _use_buffer, _buffer_usage,
                                       _format, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t use_buffer;
  uint32_t buffer_usage;
  uint32_t format;
};

static_assert(sizeof(CreateAndConsumeTextureINTERNALImmediate) == 20,
              "size of CreateAndConsumeTextureINTERNALImmediate should be 20");
static_assert(
    offsetof(CreateAndConsumeTextureINTERNALImmediate, header) == 0,
    "offset of CreateAndConsumeTextureINTERNALImmediate header should be 0");
static_assert(offsetof(CreateAndConsumeTextureINTERNALImmediate, texture_id) ==
                  4,
              "offset of CreateAndConsumeTextureINTERNALImmediate texture_id "
              "should be 4");
static_assert(offsetof(CreateAndConsumeTextureINTERNALImmediate, use_buffer) ==
                  8,
              "offset of CreateAndConsumeTextureINTERNALImmediate use_buffer "
              "should be 8");
static_assert(offsetof(CreateAndConsumeTextureINTERNALImmediate,
                       buffer_usage) == 12,
              "offset of CreateAndConsumeTextureINTERNALImmediate buffer_usage "
              "should be 12");
static_assert(
    offsetof(CreateAndConsumeTextureINTERNALImmediate, format) == 16,
    "offset of CreateAndConsumeTextureINTERNALImmediate format should be 16");

struct TexParameteri {
  typedef TexParameteri ValueType;
  static const CommandId kCmdId = kTexParameteri;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id, GLenum _pname, GLint _param) {
    SetHeader();
    texture_id = _texture_id;
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLuint _texture_id, GLenum _pname, GLint _param) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t pname;
  int32_t param;
};

static_assert(sizeof(TexParameteri) == 16,
              "size of TexParameteri should be 16");
static_assert(offsetof(TexParameteri, header) == 0,
              "offset of TexParameteri header should be 0");
static_assert(offsetof(TexParameteri, texture_id) == 4,
              "offset of TexParameteri texture_id should be 4");
static_assert(offsetof(TexParameteri, pname) == 8,
              "offset of TexParameteri pname should be 8");
static_assert(offsetof(TexParameteri, param) == 12,
              "offset of TexParameteri param should be 12");

struct BindTexImage2DCHROMIUM {
  typedef BindTexImage2DCHROMIUM ValueType;
  static const CommandId kCmdId = kBindTexImage2DCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id, GLint _image_id) {
    SetHeader();
    texture_id = _texture_id;
    image_id = _image_id;
  }

  void* Set(void* cmd, GLuint _texture_id, GLint _image_id) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _image_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  int32_t image_id;
};

static_assert(sizeof(BindTexImage2DCHROMIUM) == 12,
              "size of BindTexImage2DCHROMIUM should be 12");
static_assert(offsetof(BindTexImage2DCHROMIUM, header) == 0,
              "offset of BindTexImage2DCHROMIUM header should be 0");
static_assert(offsetof(BindTexImage2DCHROMIUM, texture_id) == 4,
              "offset of BindTexImage2DCHROMIUM texture_id should be 4");
static_assert(offsetof(BindTexImage2DCHROMIUM, image_id) == 8,
              "offset of BindTexImage2DCHROMIUM image_id should be 8");

struct ReleaseTexImage2DCHROMIUM {
  typedef ReleaseTexImage2DCHROMIUM ValueType;
  static const CommandId kCmdId = kReleaseTexImage2DCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id, GLint _image_id) {
    SetHeader();
    texture_id = _texture_id;
    image_id = _image_id;
  }

  void* Set(void* cmd, GLuint _texture_id, GLint _image_id) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _image_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  int32_t image_id;
};

static_assert(sizeof(ReleaseTexImage2DCHROMIUM) == 12,
              "size of ReleaseTexImage2DCHROMIUM should be 12");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, header) == 0,
              "offset of ReleaseTexImage2DCHROMIUM header should be 0");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, texture_id) == 4,
              "offset of ReleaseTexImage2DCHROMIUM texture_id should be 4");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, image_id) == 8,
              "offset of ReleaseTexImage2DCHROMIUM image_id should be 8");

struct TexStorage2D {
  typedef TexStorage2D ValueType;
  static const CommandId kCmdId = kTexStorage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id, GLsizei _width, GLsizei _height) {
    SetHeader();
    texture_id = _texture_id;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd, GLuint _texture_id, GLsizei _width, GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(TexStorage2D) == 16, "size of TexStorage2D should be 16");
static_assert(offsetof(TexStorage2D, header) == 0,
              "offset of TexStorage2D header should be 0");
static_assert(offsetof(TexStorage2D, texture_id) == 4,
              "offset of TexStorage2D texture_id should be 4");
static_assert(offsetof(TexStorage2D, width) == 8,
              "offset of TexStorage2D width should be 8");
static_assert(offsetof(TexStorage2D, height) == 12,
              "offset of TexStorage2D height should be 12");

struct CopySubTexture {
  typedef CopySubTexture ValueType;
  static const CommandId kCmdId = kCopySubTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _source_id,
            GLuint _dest_id,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    source_id = _source_id;
    dest_id = _dest_id;
    xoffset = _xoffset;
    yoffset = _yoffset;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLuint _source_id,
            GLuint _dest_id,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_source_id, _dest_id, _xoffset, _yoffset,
                                       _x, _y, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t source_id;
  uint32_t dest_id;
  int32_t xoffset;
  int32_t yoffset;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(CopySubTexture) == 36,
              "size of CopySubTexture should be 36");
static_assert(offsetof(CopySubTexture, header) == 0,
              "offset of CopySubTexture header should be 0");
static_assert(offsetof(CopySubTexture, source_id) == 4,
              "offset of CopySubTexture source_id should be 4");
static_assert(offsetof(CopySubTexture, dest_id) == 8,
              "offset of CopySubTexture dest_id should be 8");
static_assert(offsetof(CopySubTexture, xoffset) == 12,
              "offset of CopySubTexture xoffset should be 12");
static_assert(offsetof(CopySubTexture, yoffset) == 16,
              "offset of CopySubTexture yoffset should be 16");
static_assert(offsetof(CopySubTexture, x) == 20,
              "offset of CopySubTexture x should be 20");
static_assert(offsetof(CopySubTexture, y) == 24,
              "offset of CopySubTexture y should be 24");
static_assert(offsetof(CopySubTexture, width) == 28,
              "offset of CopySubTexture width should be 28");
static_assert(offsetof(CopySubTexture, height) == 32,
              "offset of CopySubTexture height should be 32");

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
