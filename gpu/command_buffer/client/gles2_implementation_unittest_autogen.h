// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file is included by gles2_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_UNITTEST_AUTOGEN_H_

TEST_F(GLES2ImplementationTest, AttachShader) {
  struct Cmds {
    cmds::AttachShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->AttachShader(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BindBuffer) {
  struct Cmds {
    cmds::BindBuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ARRAY_BUFFER, 2);

  gl_->BindBuffer(GL_ARRAY_BUFFER, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ClearCommands();
  gl_->BindBuffer(GL_ARRAY_BUFFER, 2);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BindBufferBase) {
  struct Cmds {
    cmds::BindBufferBase cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TRANSFORM_FEEDBACK_BUFFER, 2, 3);

  gl_->BindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BindBufferRange) {
  struct Cmds {
    cmds::BindBufferRange cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TRANSFORM_FEEDBACK_BUFFER, 2, 3, 4, 4);

  gl_->BindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 2, 3, 4, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BindFramebuffer) {
  struct Cmds {
    cmds::BindFramebuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, 2);

  gl_->BindFramebuffer(GL_FRAMEBUFFER, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ClearCommands();
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 2);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BindRenderbuffer) {
  struct Cmds {
    cmds::BindRenderbuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, 2);

  gl_->BindRenderbuffer(GL_RENDERBUFFER, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ClearCommands();
  gl_->BindRenderbuffer(GL_RENDERBUFFER, 2);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BindSampler) {
  struct Cmds {
    cmds::BindSampler cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->BindSampler(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BindTransformFeedback) {
  struct Cmds {
    cmds::BindTransformFeedback cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TRANSFORM_FEEDBACK, 2);

  gl_->BindTransformFeedback(GL_TRANSFORM_FEEDBACK, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendColor) {
  struct Cmds {
    cmds::BlendColor cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->BlendColor(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendEquation) {
  struct Cmds {
    cmds::BlendEquation cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FUNC_SUBTRACT);

  gl_->BlendEquation(GL_FUNC_SUBTRACT);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendEquationSeparate) {
  struct Cmds {
    cmds::BlendEquationSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FUNC_SUBTRACT, GL_FUNC_ADD);

  gl_->BlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_ADD);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendFunc) {
  struct Cmds {
    cmds::BlendFunc cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ZERO, GL_ZERO);

  gl_->BlendFunc(GL_ZERO, GL_ZERO);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendFuncSeparate) {
  struct Cmds {
    cmds::BlendFuncSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);

  gl_->BlendFuncSeparate(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CheckFramebufferStatus) {
  struct Cmds {
    cmds::CheckFramebufferStatus cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::CheckFramebufferStatus::Result));
  expected.cmd.Init(GL_FRAMEBUFFER, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->CheckFramebufferStatus(GL_FRAMEBUFFER);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, Clear) {
  struct Cmds {
    cmds::Clear cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_COLOR_BUFFER_BIT);

  gl_->Clear(GL_COLOR_BUFFER_BIT);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearBufferfi) {
  struct Cmds {
    cmds::ClearBufferfi cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_DEPTH_STENCIL, 2, 3, 4);

  gl_->ClearBufferfi(GL_DEPTH_STENCIL, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearBufferfv) {
  GLfloat data[4] = {0};
  struct Cmds {
    cmds::ClearBufferfvImmediate cmd;
    GLfloat data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(GL_COLOR, 2, &data[0]);
  gl_->ClearBufferfv(GL_COLOR, 2, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearBufferiv) {
  GLint data[4] = {0};
  struct Cmds {
    cmds::ClearBufferivImmediate cmd;
    GLint data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(GL_COLOR, 2, &data[0]);
  gl_->ClearBufferiv(GL_COLOR, 2, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearBufferuiv) {
  GLuint data[4] = {0};
  struct Cmds {
    cmds::ClearBufferuivImmediate cmd;
    GLuint data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLuint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(GL_COLOR, 2, &data[0]);
  gl_->ClearBufferuiv(GL_COLOR, 2, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearColor) {
  struct Cmds {
    cmds::ClearColor cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->ClearColor(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearDepthf) {
  struct Cmds {
    cmds::ClearDepthf cmd;
  };
  Cmds expected;
  expected.cmd.Init(0.5f);

  gl_->ClearDepthf(0.5f);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearStencil) {
  struct Cmds {
    cmds::ClearStencil cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->ClearStencil(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ColorMask) {
  struct Cmds {
    cmds::ColorMask cmd;
  };
  Cmds expected;
  expected.cmd.Init(true, true, true, true);

  gl_->ColorMask(true, true, true, true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CompileShader) {
  struct Cmds {
    cmds::CompileShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->CompileShader(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopyBufferSubData) {
  struct Cmds {
    cmds::CopyBufferSubData cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 3, 4, 5);

  gl_->CopyBufferSubData(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopyTexImage2D) {
  struct Cmds {
    cmds::CopyTexImage2D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2, GL_ALPHA, 4, 5, 6, 7);

  gl_->CopyTexImage2D(GL_TEXTURE_2D, 2, GL_ALPHA, 4, 5, 6, 7, 0);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopyTexImage2DInvalidConstantArg7) {
  gl_->CopyTexImage2D(GL_TEXTURE_2D, 2, GL_ALPHA, 4, 5, 6, 7, 1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());
}

TEST_F(GLES2ImplementationTest, CopyTexSubImage2D) {
  struct Cmds {
    cmds::CopyTexSubImage2D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8);

  gl_->CopyTexSubImage2D(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopyTexSubImage3D) {
  struct Cmds {
    cmds::CopyTexSubImage3D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_3D, 2, 3, 4, 5, 6, 7, 8, 9);

  gl_->CopyTexSubImage3D(GL_TEXTURE_3D, 2, 3, 4, 5, 6, 7, 8, 9);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CullFace) {
  struct Cmds {
    cmds::CullFace cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT);

  gl_->CullFace(GL_FRONT);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteBuffers) {
  GLuint ids[2] = {kBuffersStartId, kBuffersStartId + 1};
  struct Cmds {
    cmds::DeleteBuffersImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kBuffersStartId;
  expected.data[1] = kBuffersStartId + 1;
  gl_->DeleteBuffers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteFramebuffers) {
  GLuint ids[2] = {kFramebuffersStartId, kFramebuffersStartId + 1};
  struct Cmds {
    cmds::DeleteFramebuffersImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kFramebuffersStartId;
  expected.data[1] = kFramebuffersStartId + 1;
  gl_->DeleteFramebuffers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteProgram) {
  struct Cmds {
    cmds::DeleteProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DeleteProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteRenderbuffers) {
  GLuint ids[2] = {kRenderbuffersStartId, kRenderbuffersStartId + 1};
  struct Cmds {
    cmds::DeleteRenderbuffersImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kRenderbuffersStartId;
  expected.data[1] = kRenderbuffersStartId + 1;
  gl_->DeleteRenderbuffers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteSamplers) {
  GLuint ids[2] = {kSamplersStartId, kSamplersStartId + 1};
  struct Cmds {
    cmds::DeleteSamplersImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kSamplersStartId;
  expected.data[1] = kSamplersStartId + 1;
  gl_->DeleteSamplers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteSync) {
  struct Cmds {
    cmds::DeleteSync cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DeleteSync(reinterpret_cast<GLsync>(1));
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteShader) {
  struct Cmds {
    cmds::DeleteShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DeleteShader(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteTextures) {
  GLuint ids[2] = {kTexturesStartId, kTexturesStartId + 1};
  struct Cmds {
    cmds::DeleteTexturesImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kTexturesStartId;
  expected.data[1] = kTexturesStartId + 1;
  gl_->DeleteTextures(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteTransformFeedbacks) {
  GLuint ids[2] = {kTransformFeedbacksStartId, kTransformFeedbacksStartId + 1};
  struct Cmds {
    cmds::DeleteTransformFeedbacksImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kTransformFeedbacksStartId;
  expected.data[1] = kTransformFeedbacksStartId + 1;
  gl_->DeleteTransformFeedbacks(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DepthFunc) {
  struct Cmds {
    cmds::DepthFunc cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_NEVER);

  gl_->DepthFunc(GL_NEVER);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DepthMask) {
  struct Cmds {
    cmds::DepthMask cmd;
  };
  Cmds expected;
  expected.cmd.Init(true);

  gl_->DepthMask(true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DepthRangef) {
  struct Cmds {
    cmds::DepthRangef cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->DepthRangef(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DetachShader) {
  struct Cmds {
    cmds::DetachShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->DetachShader(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DisableVertexAttribArray) {
  struct Cmds {
    cmds::DisableVertexAttribArray cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DisableVertexAttribArray(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawArrays) {
  struct Cmds {
    cmds::DrawArrays cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_POINTS, 2, 3);

  gl_->DrawArrays(GL_POINTS, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, EnableVertexAttribArray) {
  struct Cmds {
    cmds::EnableVertexAttribArray cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->EnableVertexAttribArray(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Flush) {
  struct Cmds {
    cmds::Flush cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->Flush();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferRenderbuffer) {
  struct Cmds {
    cmds::FramebufferRenderbuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 4);

  gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_RENDERBUFFER, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferTexture2D) {
  struct Cmds {
    cmds::FramebufferTexture2D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 4, 5);

  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferTextureLayer) {
  struct Cmds {
    cmds::FramebufferTextureLayer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 3, 4, 5);

  gl_->FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FrontFace) {
  struct Cmds {
    cmds::FrontFace cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_CW);

  gl_->FrontFace(GL_CW);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenBuffers) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenBuffersImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kBuffersStartId;
  expected.data[1] = kBuffersStartId + 1;
  gl_->GenBuffers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kBuffersStartId, ids[0]);
  EXPECT_EQ(kBuffersStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenerateMipmap) {
  struct Cmds {
    cmds::GenerateMipmap cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D);

  gl_->GenerateMipmap(GL_TEXTURE_2D);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenFramebuffers) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenFramebuffersImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kFramebuffersStartId;
  expected.data[1] = kFramebuffersStartId + 1;
  gl_->GenFramebuffers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kFramebuffersStartId, ids[0]);
  EXPECT_EQ(kFramebuffersStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenRenderbuffers) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenRenderbuffersImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kRenderbuffersStartId;
  expected.data[1] = kRenderbuffersStartId + 1;
  gl_->GenRenderbuffers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kRenderbuffersStartId, ids[0]);
  EXPECT_EQ(kRenderbuffersStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenSamplers) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenSamplersImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kSamplersStartId;
  expected.data[1] = kSamplersStartId + 1;
  gl_->GenSamplers(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kSamplersStartId, ids[0]);
  EXPECT_EQ(kSamplersStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenTextures) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenTexturesImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kTexturesStartId;
  expected.data[1] = kTexturesStartId + 1;
  gl_->GenTextures(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kTexturesStartId, ids[0]);
  EXPECT_EQ(kTexturesStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenTransformFeedbacks) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenTransformFeedbacksImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kTransformFeedbacksStartId;
  expected.data[1] = kTransformFeedbacksStartId + 1;
  gl_->GenTransformFeedbacks(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kTransformFeedbacksStartId, ids[0]);
  EXPECT_EQ(kTransformFeedbacksStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GetBooleanv) {
  struct Cmds {
    cmds::GetBooleanv cmd;
  };
  typedef cmds::GetBooleanv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetBooleanv(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetBooleani_v) {
  struct Cmds {
    cmds::GetBooleani_v cmd;
  };
  typedef cmds::GetBooleani_v::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, 2, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetBooleani_v(123, 2, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetBufferParameteri64v) {
  struct Cmds {
    cmds::GetBufferParameteri64v cmd;
  };
  typedef cmds::GetBufferParameteri64v::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_BUFFER_SIZE, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetBufferParameteri64v(123, GL_BUFFER_SIZE, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetBufferParameteriv) {
  struct Cmds {
    cmds::GetBufferParameteriv cmd;
  };
  typedef cmds::GetBufferParameteriv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_BUFFER_SIZE, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetBufferParameteriv(123, GL_BUFFER_SIZE, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetFloatv) {
  struct Cmds {
    cmds::GetFloatv cmd;
  };
  typedef cmds::GetFloatv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetFloatv(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetFramebufferAttachmentParameteriv) {
  struct Cmds {
    cmds::GetFramebufferAttachmentParameteriv cmd;
  };
  typedef cmds::GetFramebufferAttachmentParameteriv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_COLOR_ATTACHMENT0,
                    GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, result1.id,
                    result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetFramebufferAttachmentParameteriv(
      123, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
      &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetInteger64v) {
  struct Cmds {
    cmds::GetInteger64v cmd;
  };
  typedef cmds::GetInteger64v::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetInteger64v(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetIntegeri_v) {
  struct Cmds {
    cmds::GetIntegeri_v cmd;
  };
  typedef cmds::GetIntegeri_v::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, 2, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetIntegeri_v(123, 2, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetInteger64i_v) {
  struct Cmds {
    cmds::GetInteger64i_v cmd;
  };
  typedef cmds::GetInteger64i_v::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, 2, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetInteger64i_v(123, 2, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetIntegerv) {
  struct Cmds {
    cmds::GetIntegerv cmd;
  };
  typedef cmds::GetIntegerv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetIntegerv(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetProgramiv) {
  struct Cmds {
    cmds::GetProgramiv cmd;
  };
  typedef cmds::GetProgramiv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_DELETE_STATUS, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetProgramiv(123, GL_DELETE_STATUS, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetRenderbufferParameteriv) {
  struct Cmds {
    cmds::GetRenderbufferParameteriv cmd;
  };
  typedef cmds::GetRenderbufferParameteriv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_RENDERBUFFER_RED_SIZE, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetRenderbufferParameteriv(123, GL_RENDERBUFFER_RED_SIZE, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetSamplerParameterfv) {
  struct Cmds {
    cmds::GetSamplerParameterfv cmd;
  };
  typedef cmds::GetSamplerParameterfv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_TEXTURE_MAG_FILTER, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetSamplerParameterfv(123, GL_TEXTURE_MAG_FILTER, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetSamplerParameteriv) {
  struct Cmds {
    cmds::GetSamplerParameteriv cmd;
  };
  typedef cmds::GetSamplerParameteriv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_TEXTURE_MAG_FILTER, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetSamplerParameteriv(123, GL_TEXTURE_MAG_FILTER, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetShaderiv) {
  struct Cmds {
    cmds::GetShaderiv cmd;
  };
  typedef cmds::GetShaderiv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_SHADER_TYPE, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetShaderiv(123, GL_SHADER_TYPE, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetSynciv) {
  struct Cmds {
    cmds::GetSynciv cmd;
  };
  typedef cmds::GetSynciv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_SYNC_STATUS, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetSynciv(reinterpret_cast<GLsync>(123), GL_SYNC_STATUS, 3, nullptr,
                 &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetTexParameterfv) {
  struct Cmds {
    cmds::GetTexParameterfv cmd;
  };
  typedef cmds::GetTexParameterfv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_TEXTURE_MAG_FILTER, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetTexParameterfv(123, GL_TEXTURE_MAG_FILTER, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetTexParameteriv) {
  struct Cmds {
    cmds::GetTexParameteriv cmd;
  };
  typedef cmds::GetTexParameteriv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_TEXTURE_MAG_FILTER, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetTexParameteriv(123, GL_TEXTURE_MAG_FILTER, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetVertexAttribfv) {
  struct Cmds {
    cmds::GetVertexAttribfv cmd;
  };
  typedef cmds::GetVertexAttribfv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, result1.id,
                    result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetVertexAttribfv(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetVertexAttribiv) {
  struct Cmds {
    cmds::GetVertexAttribiv cmd;
  };
  typedef cmds::GetVertexAttribiv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, result1.id,
                    result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetVertexAttribiv(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetVertexAttribIiv) {
  struct Cmds {
    cmds::GetVertexAttribIiv cmd;
  };
  typedef cmds::GetVertexAttribIiv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, result1.id,
                    result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetVertexAttribIiv(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, GetVertexAttribIuiv) {
  struct Cmds {
    cmds::GetVertexAttribIuiv cmd;
  };
  typedef cmds::GetVertexAttribIuiv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, result1.id,
                    result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetVertexAttribIuiv(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, Hint) {
  struct Cmds {
    cmds::Hint cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);

  gl_->Hint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, InvalidateFramebuffer) {
  GLenum data[2][1] = {{0}};
  struct Cmds {
    cmds::InvalidateFramebufferImmediate cmd;
    GLenum data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLenum>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(GL_FRAMEBUFFER, 2, &data[0][0]);
  gl_->InvalidateFramebuffer(GL_FRAMEBUFFER, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, InvalidateSubFramebuffer) {
  GLenum data[2][1] = {{0}};
  struct Cmds {
    cmds::InvalidateSubFramebufferImmediate cmd;
    GLenum data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLenum>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(GL_FRAMEBUFFER, 2, &data[0][0], 4, 5, 6, 7);
  gl_->InvalidateSubFramebuffer(GL_FRAMEBUFFER, 2, &data[0][0], 4, 5, 6, 7);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, IsBuffer) {
  struct Cmds {
    cmds::IsBuffer cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsBuffer::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsBuffer(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsFramebuffer) {
  struct Cmds {
    cmds::IsFramebuffer cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsFramebuffer::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsFramebuffer(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsProgram) {
  struct Cmds {
    cmds::IsProgram cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsProgram::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsRenderbuffer) {
  struct Cmds {
    cmds::IsRenderbuffer cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsRenderbuffer::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsRenderbuffer(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsSampler) {
  struct Cmds {
    cmds::IsSampler cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsSampler::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsSampler(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsShader) {
  struct Cmds {
    cmds::IsShader cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsShader::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsShader(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsSync) {
  struct Cmds {
    cmds::IsSync cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsSync::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsSync(reinterpret_cast<GLsync>(1));
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsTexture) {
  struct Cmds {
    cmds::IsTexture cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsTexture::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsTexture(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsTransformFeedback) {
  struct Cmds {
    cmds::IsTransformFeedback cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsTransformFeedback::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsTransformFeedback(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, LineWidth) {
  struct Cmds {
    cmds::LineWidth cmd;
  };
  Cmds expected;
  expected.cmd.Init(2.0f);

  gl_->LineWidth(2.0f);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, LinkProgram) {
  struct Cmds {
    cmds::LinkProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->LinkProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PauseTransformFeedback) {
  struct Cmds {
    cmds::PauseTransformFeedback cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->PauseTransformFeedback();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PixelStorei) {
  struct Cmds {
    cmds::PixelStorei cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_PACK_ALIGNMENT, 1);

  gl_->PixelStorei(GL_PACK_ALIGNMENT, 1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PolygonOffset) {
  struct Cmds {
    cmds::PolygonOffset cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->PolygonOffset(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReadBuffer) {
  struct Cmds {
    cmds::ReadBuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_NONE);

  gl_->ReadBuffer(GL_NONE);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReleaseShaderCompiler) {
  struct Cmds {
    cmds::ReleaseShaderCompiler cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->ReleaseShaderCompiler();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, RenderbufferStorage) {
  struct Cmds {
    cmds::RenderbufferStorage cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);

  gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ResumeTransformFeedback) {
  struct Cmds {
    cmds::ResumeTransformFeedback cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->ResumeTransformFeedback();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, SampleCoverage) {
  struct Cmds {
    cmds::SampleCoverage cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, true);

  gl_->SampleCoverage(1, true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, SamplerParameterf) {
  struct Cmds {
    cmds::SamplerParameterf cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  gl_->SamplerParameterf(1, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, SamplerParameterfv) {
  GLfloat data[1] = {0};
  struct Cmds {
    cmds::SamplerParameterfvImmediate cmd;
    GLfloat data[1];
  };

  for (int jj = 0; jj < 1; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, GL_TEXTURE_MAG_FILTER, &data[0]);
  gl_->SamplerParameterfv(1, GL_TEXTURE_MAG_FILTER, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, SamplerParameteri) {
  struct Cmds {
    cmds::SamplerParameteri cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  gl_->SamplerParameteri(1, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, SamplerParameteriv) {
  GLint data[1] = {0};
  struct Cmds {
    cmds::SamplerParameterivImmediate cmd;
    GLint data[1];
  };

  for (int jj = 0; jj < 1; ++jj) {
    data[jj] = static_cast<GLint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, GL_TEXTURE_MAG_FILTER, &data[0]);
  gl_->SamplerParameteriv(1, GL_TEXTURE_MAG_FILTER, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Scissor) {
  struct Cmds {
    cmds::Scissor cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Scissor(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ShaderSource) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const char* kString1 = "happy";
  const char* kString2 = "ending";
  const size_t kString1Size = ::strlen(kString1) + 1;
  const size_t kString2Size = ::strlen(kString2) + 1;
  const size_t kHeaderSize = sizeof(GLint) * 3;
  const size_t kSourceSize = kHeaderSize + kString1Size + kString2Size;
  const size_t kPaddedHeaderSize =
      transfer_buffer_->RoundToAlignment(kHeaderSize);
  const size_t kPaddedString1Size =
      transfer_buffer_->RoundToAlignment(kString1Size);
  const size_t kPaddedString2Size =
      transfer_buffer_->RoundToAlignment(kString2Size);
  struct Cmds {
    cmd::SetBucketSize set_bucket_size;
    cmd::SetBucketData set_bucket_header;
    cmd::SetToken set_token1;
    cmd::SetBucketData set_bucket_data1;
    cmd::SetToken set_token2;
    cmd::SetBucketData set_bucket_data2;
    cmd::SetToken set_token3;
    cmds::ShaderSourceBucket cmd_bucket;
    cmd::SetBucketSize clear_bucket_size;
  };

  ExpectedMemoryInfo mem0 = GetExpectedMemory(kPaddedHeaderSize);
  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPaddedString1Size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kPaddedString2Size);

  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_header.Init(kBucketId, 0, kHeaderSize, mem0.id,
                                  mem0.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_data1.Init(kBucketId, kHeaderSize, kString1Size, mem1.id,
                                 mem1.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_bucket_data2.Init(kBucketId, kHeaderSize + kString1Size,
                                 kString2Size, mem2.id, mem2.offset);
  expected.set_token3.Init(GetNextToken());
  expected.cmd_bucket.Init(1, kBucketId);
  expected.clear_bucket_size.Init(kBucketId, 0);
  const char* kStrings[] = {kString1, kString2};
  gl_->ShaderSource(1, 2, kStrings, nullptr);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ShaderSourceWithLength) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const char* kString = "foobar******";
  const size_t kStringSize = 6;  // We only need "foobar".
  const size_t kHeaderSize = sizeof(GLint) * 2;
  const size_t kSourceSize = kHeaderSize + kStringSize + 1;
  const size_t kPaddedHeaderSize =
      transfer_buffer_->RoundToAlignment(kHeaderSize);
  const size_t kPaddedStringSize =
      transfer_buffer_->RoundToAlignment(kStringSize + 1);
  struct Cmds {
    cmd::SetBucketSize set_bucket_size;
    cmd::SetBucketData set_bucket_header;
    cmd::SetToken set_token1;
    cmd::SetBucketData set_bucket_data;
    cmd::SetToken set_token2;
    cmds::ShaderSourceBucket shader_source_bucket;
    cmd::SetBucketSize clear_bucket_size;
  };

  ExpectedMemoryInfo mem0 = GetExpectedMemory(kPaddedHeaderSize);
  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPaddedStringSize);

  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_header.Init(kBucketId, 0, kHeaderSize, mem0.id,
                                  mem0.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_data.Init(kBucketId, kHeaderSize, kStringSize + 1,
                                mem1.id, mem1.offset);
  expected.set_token2.Init(GetNextToken());
  expected.shader_source_bucket.Init(1, kBucketId);
  expected.clear_bucket_size.Init(kBucketId, 0);
  const char* kStrings[] = {kString};
  const GLint kLength[] = {kStringSize};
  gl_->ShaderSource(1, 1, kStrings, kLength);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilFunc) {
  struct Cmds {
    cmds::StencilFunc cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_NEVER, 2, 3);

  gl_->StencilFunc(GL_NEVER, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilFuncSeparate) {
  struct Cmds {
    cmds::StencilFuncSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT, GL_NEVER, 3, 4);

  gl_->StencilFuncSeparate(GL_FRONT, GL_NEVER, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilMask) {
  struct Cmds {
    cmds::StencilMask cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->StencilMask(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilMaskSeparate) {
  struct Cmds {
    cmds::StencilMaskSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT, 2);

  gl_->StencilMaskSeparate(GL_FRONT, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilOp) {
  struct Cmds {
    cmds::StencilOp cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_KEEP, GL_INCR, GL_KEEP);

  gl_->StencilOp(GL_KEEP, GL_INCR, GL_KEEP);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilOpSeparate) {
  struct Cmds {
    cmds::StencilOpSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP);

  gl_->StencilOpSeparate(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameterf) {
  struct Cmds {
    cmds::TexParameterf cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  gl_->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameterfv) {
  GLfloat data[1] = {0};
  struct Cmds {
    cmds::TexParameterfvImmediate cmd;
    GLfloat data[1];
  };

  for (int jj = 0; jj < 1; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &data[0]);
  gl_->TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameteri) {
  struct Cmds {
    cmds::TexParameteri cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameteriv) {
  GLint data[1] = {0};
  struct Cmds {
    cmds::TexParameterivImmediate cmd;
    GLint data[1];
  };

  for (int jj = 0; jj < 1; ++jj) {
    data[jj] = static_cast<GLint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &data[0]);
  gl_->TexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexStorage3D) {
  struct Cmds {
    cmds::TexStorage3D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_3D, 2, GL_RGB565, 4, 5, 6);

  gl_->TexStorage3D(GL_TEXTURE_3D, 2, GL_RGB565, 4, 5, 6);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TransformFeedbackVaryings) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const char* kString1 = "happy";
  const char* kString2 = "ending";
  const size_t kString1Size = ::strlen(kString1) + 1;
  const size_t kString2Size = ::strlen(kString2) + 1;
  const size_t kHeaderSize = sizeof(GLint) * 3;
  const size_t kSourceSize = kHeaderSize + kString1Size + kString2Size;
  const size_t kPaddedHeaderSize =
      transfer_buffer_->RoundToAlignment(kHeaderSize);
  const size_t kPaddedString1Size =
      transfer_buffer_->RoundToAlignment(kString1Size);
  const size_t kPaddedString2Size =
      transfer_buffer_->RoundToAlignment(kString2Size);
  struct Cmds {
    cmd::SetBucketSize set_bucket_size;
    cmd::SetBucketData set_bucket_header;
    cmd::SetToken set_token1;
    cmd::SetBucketData set_bucket_data1;
    cmd::SetToken set_token2;
    cmd::SetBucketData set_bucket_data2;
    cmd::SetToken set_token3;
    cmds::TransformFeedbackVaryingsBucket cmd_bucket;
    cmd::SetBucketSize clear_bucket_size;
  };

  ExpectedMemoryInfo mem0 = GetExpectedMemory(kPaddedHeaderSize);
  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPaddedString1Size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kPaddedString2Size);

  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_header.Init(kBucketId, 0, kHeaderSize, mem0.id,
                                  mem0.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_data1.Init(kBucketId, kHeaderSize, kString1Size, mem1.id,
                                 mem1.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_bucket_data2.Init(kBucketId, kHeaderSize + kString1Size,
                                 kString2Size, mem2.id, mem2.offset);
  expected.set_token3.Init(GetNextToken());
  expected.cmd_bucket.Init(1, kBucketId, GL_INTERLEAVED_ATTRIBS);
  expected.clear_bucket_size.Init(kBucketId, 0);
  const char* kStrings[] = {kString1, kString2};
  gl_->TransformFeedbackVaryings(1, 2, kStrings, GL_INTERLEAVED_ATTRIBS);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1f) {
  struct Cmds {
    cmds::Uniform1f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->Uniform1f(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1fv) {
  GLfloat data[2][1] = {{0}};
  struct Cmds {
    cmds::Uniform1fvImmediate cmd;
    GLfloat data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform1fv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1i) {
  struct Cmds {
    cmds::Uniform1i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->Uniform1i(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1iv) {
  GLint data[2][1] = {{0}};
  struct Cmds {
    cmds::Uniform1ivImmediate cmd;
    GLint data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLint>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform1iv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1ui) {
  struct Cmds {
    cmds::Uniform1ui cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->Uniform1ui(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1uiv) {
  GLuint data[2][1] = {{0}};
  struct Cmds {
    cmds::Uniform1uivImmediate cmd;
    GLuint data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLuint>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform1uiv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2f) {
  struct Cmds {
    cmds::Uniform2f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->Uniform2f(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2fv) {
  GLfloat data[2][2] = {{0}};
  struct Cmds {
    cmds::Uniform2fvImmediate cmd;
    GLfloat data[2][2];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 2; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 2 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform2fv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2i) {
  struct Cmds {
    cmds::Uniform2i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->Uniform2i(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2iv) {
  GLint data[2][2] = {{0}};
  struct Cmds {
    cmds::Uniform2ivImmediate cmd;
    GLint data[2][2];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 2; ++jj) {
      data[ii][jj] = static_cast<GLint>(ii * 2 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform2iv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2ui) {
  struct Cmds {
    cmds::Uniform2ui cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->Uniform2ui(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2uiv) {
  GLuint data[2][2] = {{0}};
  struct Cmds {
    cmds::Uniform2uivImmediate cmd;
    GLuint data[2][2];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 2; ++jj) {
      data[ii][jj] = static_cast<GLuint>(ii * 2 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform2uiv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3f) {
  struct Cmds {
    cmds::Uniform3f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Uniform3f(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3fv) {
  GLfloat data[2][3] = {{0}};
  struct Cmds {
    cmds::Uniform3fvImmediate cmd;
    GLfloat data[2][3];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 3; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 3 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform3fv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3i) {
  struct Cmds {
    cmds::Uniform3i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Uniform3i(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3iv) {
  GLint data[2][3] = {{0}};
  struct Cmds {
    cmds::Uniform3ivImmediate cmd;
    GLint data[2][3];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 3; ++jj) {
      data[ii][jj] = static_cast<GLint>(ii * 3 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform3iv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3ui) {
  struct Cmds {
    cmds::Uniform3ui cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Uniform3ui(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3uiv) {
  GLuint data[2][3] = {{0}};
  struct Cmds {
    cmds::Uniform3uivImmediate cmd;
    GLuint data[2][3];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 3; ++jj) {
      data[ii][jj] = static_cast<GLuint>(ii * 3 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform3uiv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4f) {
  struct Cmds {
    cmds::Uniform4f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->Uniform4f(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4fv) {
  GLfloat data[2][4] = {{0}};
  struct Cmds {
    cmds::Uniform4fvImmediate cmd;
    GLfloat data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform4fv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4i) {
  struct Cmds {
    cmds::Uniform4i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->Uniform4i(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4iv) {
  GLint data[2][4] = {{0}};
  struct Cmds {
    cmds::Uniform4ivImmediate cmd;
    GLint data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      data[ii][jj] = static_cast<GLint>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform4iv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4ui) {
  struct Cmds {
    cmds::Uniform4ui cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->Uniform4ui(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4uiv) {
  GLuint data[2][4] = {{0}};
  struct Cmds {
    cmds::Uniform4uivImmediate cmd;
    GLuint data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      data[ii][jj] = static_cast<GLuint>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(1, 2, &data[0][0]);
  gl_->Uniform4uiv(1, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformBlockBinding) {
  struct Cmds {
    cmds::UniformBlockBinding cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->UniformBlockBinding(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix2fv) {
  GLfloat data[2][4] = {{0}};
  struct Cmds {
    cmds::UniformMatrix2fvImmediate cmd;
    GLfloat data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix2fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix2x3fv) {
  GLfloat data[2][6] = {{0}};
  struct Cmds {
    cmds::UniformMatrix2x3fvImmediate cmd;
    GLfloat data[2][6];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 6; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 6 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix2x3fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix2x4fv) {
  GLfloat data[2][8] = {{0}};
  struct Cmds {
    cmds::UniformMatrix2x4fvImmediate cmd;
    GLfloat data[2][8];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 8; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 8 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix2x4fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix3fv) {
  GLfloat data[2][9] = {{0}};
  struct Cmds {
    cmds::UniformMatrix3fvImmediate cmd;
    GLfloat data[2][9];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 9; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 9 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix3fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix3x2fv) {
  GLfloat data[2][6] = {{0}};
  struct Cmds {
    cmds::UniformMatrix3x2fvImmediate cmd;
    GLfloat data[2][6];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 6; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 6 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix3x2fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix3x4fv) {
  GLfloat data[2][12] = {{0}};
  struct Cmds {
    cmds::UniformMatrix3x4fvImmediate cmd;
    GLfloat data[2][12];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 12; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 12 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix3x4fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix4fv) {
  GLfloat data[2][16] = {{0}};
  struct Cmds {
    cmds::UniformMatrix4fvImmediate cmd;
    GLfloat data[2][16];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 16; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 16 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix4fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix4x2fv) {
  GLfloat data[2][8] = {{0}};
  struct Cmds {
    cmds::UniformMatrix4x2fvImmediate cmd;
    GLfloat data[2][8];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 8; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 8 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix4x2fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix4x3fv) {
  GLfloat data[2][12] = {{0}};
  struct Cmds {
    cmds::UniformMatrix4x3fvImmediate cmd;
    GLfloat data[2][12];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 12; ++jj) {
      data[ii][jj] = static_cast<GLfloat>(ii * 12 + jj);
    }
  }
  expected.cmd.Init(1, 2, true, &data[0][0]);
  gl_->UniformMatrix4x3fv(1, 2, true, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UseProgram) {
  struct Cmds {
    cmds::UseProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->UseProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ClearCommands();
  gl_->UseProgram(1);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, ValidateProgram) {
  struct Cmds {
    cmds::ValidateProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->ValidateProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib1f) {
  struct Cmds {
    cmds::VertexAttrib1f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->VertexAttrib1f(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib1fv) {
  GLfloat data[1] = {0};
  struct Cmds {
    cmds::VertexAttrib1fvImmediate cmd;
    GLfloat data[1];
  };

  for (int jj = 0; jj < 1; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->VertexAttrib1fv(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib2f) {
  struct Cmds {
    cmds::VertexAttrib2f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->VertexAttrib2f(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib2fv) {
  GLfloat data[2] = {0};
  struct Cmds {
    cmds::VertexAttrib2fvImmediate cmd;
    GLfloat data[2];
  };

  for (int jj = 0; jj < 2; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->VertexAttrib2fv(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib3f) {
  struct Cmds {
    cmds::VertexAttrib3f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->VertexAttrib3f(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib3fv) {
  GLfloat data[3] = {0};
  struct Cmds {
    cmds::VertexAttrib3fvImmediate cmd;
    GLfloat data[3];
  };

  for (int jj = 0; jj < 3; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->VertexAttrib3fv(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib4f) {
  struct Cmds {
    cmds::VertexAttrib4f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->VertexAttrib4f(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib4fv) {
  GLfloat data[4] = {0};
  struct Cmds {
    cmds::VertexAttrib4fvImmediate cmd;
    GLfloat data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->VertexAttrib4fv(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttribI4i) {
  struct Cmds {
    cmds::VertexAttribI4i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->VertexAttribI4i(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttribI4iv) {
  GLint data[4] = {0};
  struct Cmds {
    cmds::VertexAttribI4ivImmediate cmd;
    GLint data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->VertexAttribI4iv(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttribI4ui) {
  struct Cmds {
    cmds::VertexAttribI4ui cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->VertexAttribI4ui(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttribI4uiv) {
  GLuint data[4] = {0};
  struct Cmds {
    cmds::VertexAttribI4uivImmediate cmd;
    GLuint data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLuint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->VertexAttribI4uiv(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Viewport) {
  struct Cmds {
    cmds::Viewport cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Viewport(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlitFramebufferCHROMIUM) {
  struct Cmds {
    cmds::BlitFramebufferCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5, 6, 7, 8, 9, GL_NEAREST);

  gl_->BlitFramebufferCHROMIUM(1, 2, 3, 4, 5, 6, 7, 8, 9, GL_NEAREST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, RenderbufferStorageMultisampleCHROMIUM) {
  struct Cmds {
    cmds::RenderbufferStorageMultisampleCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, 2, GL_RGBA4, 4, 5);

  gl_->RenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, 2, GL_RGBA4, 4,
                                              5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, RenderbufferStorageMultisampleAdvancedAMD) {
  struct Cmds {
    cmds::RenderbufferStorageMultisampleAdvancedAMD cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, 2, 3, GL_RGBA4, 5, 6);

  gl_->RenderbufferStorageMultisampleAdvancedAMD(GL_RENDERBUFFER, 2, 3,
                                                 GL_RGBA4, 5, 6);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, RenderbufferStorageMultisampleEXT) {
  struct Cmds {
    cmds::RenderbufferStorageMultisampleEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, 2, GL_RGBA4, 4, 5);

  gl_->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, 2, GL_RGBA4, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferTexture2DMultisampleEXT) {
  struct Cmds {
    cmds::FramebufferTexture2DMultisampleEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 4, 5,
                    6);

  gl_->FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_TEXTURE_2D, 4, 5, 6);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexStorage2DEXT) {
  struct Cmds {
    cmds::TexStorage2DEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2, GL_RGB565, 4, 5);

  gl_->TexStorage2DEXT(GL_TEXTURE_2D, 2, GL_RGB565, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenQueriesEXT) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenQueriesEXTImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->GenQueriesEXT(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kQueriesStartId, ids[0]);
  EXPECT_EQ(kQueriesStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, DeleteQueriesEXT) {
  GLuint ids[2] = {kQueriesStartId, kQueriesStartId + 1};
  struct Cmds {
    cmds::DeleteQueriesEXTImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->DeleteQueriesEXT(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BeginTransformFeedback) {
  struct Cmds {
    cmds::BeginTransformFeedback cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_POINTS);

  gl_->BeginTransformFeedback(GL_POINTS);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, EndTransformFeedback) {
  struct Cmds {
    cmds::EndTransformFeedback cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->EndTransformFeedback();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PopGroupMarkerEXT) {
  struct Cmds {
    cmds::PopGroupMarkerEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->PopGroupMarkerEXT();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenVertexArraysOES) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenVertexArraysOESImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kVertexArraysStartId;
  expected.data[1] = kVertexArraysStartId + 1;
  gl_->GenVertexArraysOES(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kVertexArraysStartId, ids[0]);
  EXPECT_EQ(kVertexArraysStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, DeleteVertexArraysOES) {
  GLuint ids[2] = {kVertexArraysStartId, kVertexArraysStartId + 1};
  struct Cmds {
    cmds::DeleteVertexArraysOESImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kVertexArraysStartId;
  expected.data[1] = kVertexArraysStartId + 1;
  gl_->DeleteVertexArraysOES(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, IsVertexArrayOES) {
  struct Cmds {
    cmds::IsVertexArrayOES cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsVertexArrayOES::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsVertexArrayOES(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, FramebufferParameteri) {
  struct Cmds {
    cmds::FramebufferParameteri cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, 2, 3);

  gl_->FramebufferParameteri(GL_FRAMEBUFFER, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BindImageTexture) {
  struct Cmds {
    cmds::BindImageTexture cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, true, 5, 6, 7);

  gl_->BindImageTexture(1, 2, 3, true, 5, 6, 7);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DispatchCompute) {
  struct Cmds {
    cmds::DispatchCompute cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->DispatchCompute(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DispatchComputeIndirect) {
  struct Cmds {
    cmds::DispatchComputeIndirect cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DispatchComputeIndirect(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GetProgramInterfaceiv) {
  struct Cmds {
    cmds::GetProgramInterfaceiv cmd;
  };
  typedef cmds::GetProgramInterfaceiv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, 2, 3, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetProgramInterfaceiv(123, 2, 3, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, MemoryBarrierEXT) {
  struct Cmds {
    cmds::MemoryBarrierEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->MemoryBarrierEXT(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MemoryBarrierByRegion) {
  struct Cmds {
    cmds::MemoryBarrierByRegion cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->MemoryBarrierByRegion(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FlushMappedBufferRange) {
  struct Cmds {
    cmds::FlushMappedBufferRange cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ARRAY_BUFFER, 2, 3);

  gl_->FlushMappedBufferRange(GL_ARRAY_BUFFER, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DescheduleUntilFinishedCHROMIUM) {
  struct Cmds {
    cmds::DescheduleUntilFinishedCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->DescheduleUntilFinishedCHROMIUM();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopyTextureCHROMIUM) {
  struct Cmds {
    cmds::CopyTextureCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, GL_TEXTURE_2D, 4, 5, GL_ALPHA, GL_UNSIGNED_BYTE, true,
                    true, true);

  gl_->CopyTextureCHROMIUM(1, 2, GL_TEXTURE_2D, 4, 5, GL_ALPHA,
                           GL_UNSIGNED_BYTE, true, true, true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopySubTextureCHROMIUM) {
  struct Cmds {
    cmds::CopySubTextureCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, GL_TEXTURE_2D, 4, 5, 6, 7, 8, 9, 10, 11, true, true,
                    true);

  gl_->CopySubTextureCHROMIUM(1, 2, GL_TEXTURE_2D, 4, 5, 6, 7, 8, 9, 10, 11,
                              true, true, true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawArraysInstancedANGLE) {
  struct Cmds {
    cmds::DrawArraysInstancedANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_POINTS, 2, 3, 4);

  gl_->DrawArraysInstancedANGLE(GL_POINTS, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawArraysInstancedBaseInstanceANGLE) {
  struct Cmds {
    cmds::DrawArraysInstancedBaseInstanceANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_POINTS, 2, 3, 4, 5);

  gl_->DrawArraysInstancedBaseInstanceANGLE(GL_POINTS, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttribDivisorANGLE) {
  struct Cmds {
    cmds::VertexAttribDivisorANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->VertexAttribDivisorANGLE(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DiscardFramebufferEXT) {
  GLenum data[2][1] = {{0}};
  struct Cmds {
    cmds::DiscardFramebufferEXTImmediate cmd;
    GLenum data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLenum>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(GL_FRAMEBUFFER, 2, &data[0][0]);
  gl_->DiscardFramebufferEXT(GL_FRAMEBUFFER, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, LoseContextCHROMIUM) {
  struct Cmds {
    cmds::LoseContextCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_GUILTY_CONTEXT_RESET_ARB, GL_GUILTY_CONTEXT_RESET_ARB);

  gl_->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                           GL_GUILTY_CONTEXT_RESET_ARB);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawBuffersEXT) {
  GLenum data[1][1] = {{0}};
  struct Cmds {
    cmds::DrawBuffersEXTImmediate cmd;
    GLenum data[1][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 1; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLenum>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, &data[0][0]);
  gl_->DrawBuffersEXT(1, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FlushDriverCachesCHROMIUM) {
  struct Cmds {
    cmds::FlushDriverCachesCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->FlushDriverCachesCHROMIUM();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, WindowRectanglesEXT) {
  GLint data[2][4] = {{0}};
  struct Cmds {
    cmds::WindowRectanglesEXTImmediate cmd;
    GLint data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      data[ii][jj] = static_cast<GLint>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(GL_INCLUSIVE_EXT, 2, &data[0][0]);
  gl_->WindowRectanglesEXT(GL_INCLUSIVE_EXT, 2, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, WaitGpuFenceCHROMIUM) {
  struct Cmds {
    cmds::WaitGpuFenceCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->WaitGpuFenceCHROMIUM(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DestroyGpuFenceCHROMIUM) {
  struct Cmds {
    cmds::DestroyGpuFenceCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DestroyGpuFenceCHROMIUM(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferTextureMultiviewOVR) {
  struct Cmds {
    cmds::FramebufferTextureMultiviewOVR cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5, 6);

  gl_->FramebufferTextureMultiviewOVR(1, 2, 3, 4, 5, 6);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, EndSharedImageAccessDirectCHROMIUM) {
  struct Cmds {
    cmds::EndSharedImageAccessDirectCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->EndSharedImageAccessDirectCHROMIUM(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopySharedImageINTERNAL) {
  GLbyte data[32] = {0};
  struct Cmds {
    cmds::CopySharedImageINTERNALImmediate cmd;
    GLbyte data[32];
  };

  for (int jj = 0; jj < 32; ++jj) {
    data[jj] = static_cast<GLbyte>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5, 6, true, &data[0]);
  gl_->CopySharedImageINTERNAL(1, 2, 3, 4, 5, 6, true, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopySharedImageToTextureINTERNAL) {
  GLbyte data[16] = {0};
  struct Cmds {
    cmds::CopySharedImageToTextureINTERNALImmediate cmd;
    GLbyte data[16];
  };

  for (int jj = 0; jj < 16; ++jj) {
    data[jj] = static_cast<GLbyte>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5, 6, 7, 8, true, &data[0]);
  gl_->CopySharedImageToTextureINTERNAL(1, 2, 3, 4, 5, 6, 7, 8, true, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, EnableiOES) {
  struct Cmds {
    cmds::EnableiOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->EnableiOES(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DisableiOES) {
  struct Cmds {
    cmds::DisableiOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->DisableiOES(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendEquationiOES) {
  struct Cmds {
    cmds::BlendEquationiOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, GL_FUNC_SUBTRACT);

  gl_->BlendEquationiOES(1, GL_FUNC_SUBTRACT);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendEquationSeparateiOES) {
  struct Cmds {
    cmds::BlendEquationSeparateiOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, GL_FUNC_SUBTRACT, GL_FUNC_SUBTRACT);

  gl_->BlendEquationSeparateiOES(1, GL_FUNC_SUBTRACT, GL_FUNC_SUBTRACT);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendFunciOES) {
  struct Cmds {
    cmds::BlendFunciOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->BlendFunciOES(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendFuncSeparateiOES) {
  struct Cmds {
    cmds::BlendFuncSeparateiOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->BlendFuncSeparateiOES(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ColorMaskiOES) {
  struct Cmds {
    cmds::ColorMaskiOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, true, true, true, true);

  gl_->ColorMaskiOES(1, true, true, true, true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ProvokingVertexANGLE) {
  struct Cmds {
    cmds::ProvokingVertexANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->ProvokingVertexANGLE(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferMemorylessPixelLocalStorageANGLE) {
  struct Cmds {
    cmds::FramebufferMemorylessPixelLocalStorageANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->FramebufferMemorylessPixelLocalStorageANGLE(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferTexturePixelLocalStorageANGLE) {
  struct Cmds {
    cmds::FramebufferTexturePixelLocalStorageANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->FramebufferTexturePixelLocalStorageANGLE(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferPixelLocalClearValuefvANGLE) {
  GLfloat data[4] = {0};
  struct Cmds {
    cmds::FramebufferPixelLocalClearValuefvANGLEImmediate cmd;
    GLfloat data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLfloat>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->FramebufferPixelLocalClearValuefvANGLE(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferPixelLocalClearValueivANGLE) {
  GLint data[4] = {0};
  struct Cmds {
    cmds::FramebufferPixelLocalClearValueivANGLEImmediate cmd;
    GLint data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->FramebufferPixelLocalClearValueivANGLE(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferPixelLocalClearValueuivANGLE) {
  GLuint data[4] = {0};
  struct Cmds {
    cmds::FramebufferPixelLocalClearValueuivANGLEImmediate cmd;
    GLuint data[4];
  };

  for (int jj = 0; jj < 4; ++jj) {
    data[jj] = static_cast<GLuint>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, &data[0]);
  gl_->FramebufferPixelLocalClearValueuivANGLE(1, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BeginPixelLocalStorageANGLE) {
  GLenum data[1][1] = {{0}};
  struct Cmds {
    cmds::BeginPixelLocalStorageANGLEImmediate cmd;
    GLenum data[1][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 1; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLenum>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, &data[0][0]);
  gl_->BeginPixelLocalStorageANGLE(1, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, EndPixelLocalStorageANGLE) {
  GLenum data[1][1] = {{0}};
  struct Cmds {
    cmds::EndPixelLocalStorageANGLEImmediate cmd;
    GLenum data[1][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 1; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      data[ii][jj] = static_cast<GLenum>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, &data[0][0]);
  gl_->EndPixelLocalStorageANGLE(1, &data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PixelLocalStorageBarrierANGLE) {
  struct Cmds {
    cmds::PixelLocalStorageBarrierANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->PixelLocalStorageBarrierANGLE();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferPixelLocalStorageInterruptANGLE) {
  struct Cmds {
    cmds::FramebufferPixelLocalStorageInterruptANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->FramebufferPixelLocalStorageInterruptANGLE();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferPixelLocalStorageRestoreANGLE) {
  struct Cmds {
    cmds::FramebufferPixelLocalStorageRestoreANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->FramebufferPixelLocalStorageRestoreANGLE();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest,
       GetFramebufferPixelLocalStorageParameterfvANGLE) {
  struct Cmds {
    cmds::GetFramebufferPixelLocalStorageParameterfvANGLE cmd;
  };
  typedef cmds::GetFramebufferPixelLocalStorageParameterfvANGLE::Result::Type
      ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, 2, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetFramebufferPixelLocalStorageParameterfvANGLE(123, 2, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest,
       GetFramebufferPixelLocalStorageParameterivANGLE) {
  struct Cmds {
    cmds::GetFramebufferPixelLocalStorageParameterivANGLE cmd;
  };
  typedef cmds::GetFramebufferPixelLocalStorageParameterivANGLE::Result::Type
      ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, 2, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetFramebufferPixelLocalStorageParameterivANGLE(123, 2, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
}

TEST_F(GLES2ImplementationTest, ClipControlEXT) {
  struct Cmds {
    cmds::ClipControlEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->ClipControlEXT(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PolygonModeANGLE) {
  struct Cmds {
    cmds::PolygonModeANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->PolygonModeANGLE(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PolygonOffsetClampEXT) {
  struct Cmds {
    cmds::PolygonOffsetClampEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->PolygonOffsetClampEXT(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
