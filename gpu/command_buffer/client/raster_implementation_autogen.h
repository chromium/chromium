// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by raster_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_AUTOGEN_H_

void Finish() override;

void Flush() override;

GLenum GetError() override;

void ShallowFlushCHROMIUM() override;

void OrderingBarrierCHROMIUM() override;

void GenQueriesEXT(GLsizei n, GLuint* queries) override;

void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;

void QueryCounterEXT(GLuint id, GLenum target) override;

void BeginQueryEXT(GLenum target, GLuint id) override;

void EndQueryEXT(GLenum target) override;

void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;

void GetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64* params) override;

void LoseContextCHROMIUM(GLenum current, GLenum other) override;

GLenum GetGraphicsResetStatusKHR() override;

void EndRasterCHROMIUM() override;

void TraceBeginCHROMIUM(const char* category_name,
                        const char* trace_name) override;

void TraceEndCHROMIUM() override;

void SetActiveURLCHROMIUM(const char* url) override;

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_AUTOGEN_H_
