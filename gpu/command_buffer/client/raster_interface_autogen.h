// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by raster_interface.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_AUTOGEN_H_

virtual void Finish() = 0;
virtual void Flush() = 0;
virtual GLenum GetError() = 0;
virtual void ShallowFlushCHROMIUM() = 0;
virtual void OrderingBarrierCHROMIUM() = 0;
virtual void GenQueriesEXT(GLsizei n, GLuint* queries) = 0;
virtual void DeleteQueriesEXT(GLsizei n, const GLuint* queries) = 0;
virtual void QueryCounterEXT(GLuint id, GLenum target) = 0;
virtual void BeginQueryEXT(GLenum target, GLuint id) = 0;
virtual void EndQueryEXT(GLenum target) = 0;
virtual void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) = 0;
virtual void GetQueryObjectui64vEXT(GLuint id,
                                    GLenum pname,
                                    GLuint64* params) = 0;
virtual void LoseContextCHROMIUM(GLenum current, GLenum other) = 0;
virtual GLenum GetGraphicsResetStatusKHR() = 0;
virtual void EndRasterCHROMIUM() = 0;
virtual void TraceBeginCHROMIUM(const char* category_name,
                                const char* trace_name) = 0;
virtual void TraceEndCHROMIUM() = 0;
virtual void SetActiveURLCHROMIUM(const char* url) = 0;
#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_AUTOGEN_H_
