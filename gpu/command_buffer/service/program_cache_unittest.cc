// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_cache.h"

#include <memory>

#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"

using ::testing::Return;

namespace gpu {
namespace gles2 {

class NoBackendProgramCache : public ProgramCache {
 public:
  NoBackendProgramCache() : ProgramCache(0) {}
  ProgramLoadResult LoadLinkedProgram(
      GLuint /* program */,
      Shader* /* shader_a */,
      Shader* /* shader_b */,
      const LocationMap* /* bind_attrib_location_map */,
      const std::vector<std::string>& /* transform_feedback_varyings */,
      GLenum /* transform_feedback_buffer_mode */,
      DecoderClient* /* client */) override {
    return PROGRAM_LOAD_SUCCESS;
  }
  void SaveLinkedProgram(
      GLuint /* program */,
      const Shader* /* shader_a */,
      const Shader* /* shader_b */,
      const LocationMap* /* bind_attrib_location_map */,
      const std::vector<std::string>& /* transform_feedback_varyings */,
      GLenum /* transform_feedback_buffer_mode */,
      DecoderClient* /* client */) override {}

  void LoadProgram(const std::string& /*key*/,
                   const std::string& /* program */) override {}

  void ClearBackend() override {}

  void SaySuccessfullyCached(const std::string& shader1,
                             const std::string& shader2,
                             std::map<std::string, GLint>* attrib_map,
                             const std::vector<std::string>& varyings,
                             GLenum buffer_mode) {
    Hash a_sha;
    Hash b_sha;
    ComputeShaderHash(shader1, a_sha);
    ComputeShaderHash(shader2, b_sha);

    Hash program_sha;
    ComputeProgramHash(a_sha, b_sha, attrib_map, varyings, buffer_mode,
                       program_sha);
    CompiledShaderCacheSuccess(a_sha);
    CompiledShaderCacheSuccess(b_sha);
    LinkedProgramCacheSuccess(program_sha);
  }

  void ComputeShaderHash(const std::string& shader, Hash& result) const {
    ProgramCache::ComputeShaderHash(shader, result);
  }

  void ComputeProgramHash(
      HashView hashed_shader_0,
      HashView hashed_shader_1,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode,
      Hash& result) const {
    ProgramCache::ComputeProgramHash(hashed_shader_0,
                                     hashed_shader_1,
                                     bind_attrib_location_map,
                                     transform_feedback_varyings,
                                     transform_feedback_buffer_mode,
                                     result);
  }

  void Evict(const Hash& program_hash,
             const Hash& shader_0_hash,
             const Hash& shader_1_hash) {
    ProgramCache::Evict(program_hash, shader_0_hash, shader_1_hash);
  }

  size_t Trim(size_t limit) override { return 0; }
};

class ProgramCacheTest : public testing::Test {
 public:
  ProgramCacheTest() :
    cache_(new NoBackendProgramCache()) { }

 protected:
  std::unique_ptr<NoBackendProgramCache> cache_;
  std::vector<std::string> varyings_;
};

TEST_F(ProgramCacheTest, LinkStatusSave) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  {
    std::string shader_a = shader1;
    std::string shader_b = shader2;
    EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
              cache_->GetLinkedProgramStatus(shader_a, shader_b, nullptr,
                                             varyings_, GL_NONE));
    cache_->SaySuccessfullyCached(shader_a, shader_b, nullptr, varyings_,
                                  GL_NONE);

    shader_a.clear();
    shader_b.clear();
  }
  // make sure it was copied
  EXPECT_EQ(ProgramCache::LINK_SUCCEEDED,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));
}

TEST_F(ProgramCacheTest, LinkUnknownOnFragmentSourceChange) {
  const std::string shader1 = "abcd1234";
  std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  cache_->SaySuccessfullyCached(shader1, shader2, nullptr, varyings_, GL_NONE);

  shader2 = "different!";
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));
}

TEST_F(ProgramCacheTest, LinkUnknownOnVertexSourceChange) {
  std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  cache_->SaySuccessfullyCached(shader1, shader2, nullptr, varyings_, GL_NONE);

  shader1 = "different!";
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));
}

TEST_F(ProgramCacheTest, StatusEviction) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  cache_->SaySuccessfullyCached(shader1, shader2, nullptr, varyings_, GL_NONE);
  ProgramCache::Hash a_sha;
  ProgramCache::Hash b_sha;
  cache_->ComputeShaderHash(shader1, a_sha);
  cache_->ComputeShaderHash(shader2, b_sha);

  ProgramCache::Hash program_sha;
  cache_->ComputeProgramHash(a_sha, b_sha, nullptr, varyings_, GL_NONE,
                             program_sha);
  cache_->Evict(program_sha, a_sha, b_sha);
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));
}

TEST_F(ProgramCacheTest, EvictionWithReusedShader) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  const std::string shader3 = "asbjbbjj239a";
  cache_->SaySuccessfullyCached(shader1, shader2, nullptr, varyings_, GL_NONE);
  cache_->SaySuccessfullyCached(shader1, shader3, nullptr, varyings_, GL_NONE);

  ProgramCache::Hash a_sha;
  ProgramCache::Hash b_sha;
  ProgramCache::Hash c_sha;
  cache_->ComputeShaderHash(shader1, a_sha);
  cache_->ComputeShaderHash(shader2, b_sha);
  cache_->ComputeShaderHash(shader3, c_sha);

  ProgramCache::Hash program_sha;
  cache_->ComputeProgramHash(a_sha, b_sha, nullptr, varyings_, GL_NONE,
                             program_sha);
  cache_->Evict(program_sha, a_sha, b_sha);
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));
  EXPECT_EQ(ProgramCache::LINK_SUCCEEDED,
            cache_->GetLinkedProgramStatus(shader1, shader3, nullptr, varyings_,
                                           GL_NONE));

  cache_->ComputeProgramHash(a_sha, c_sha, nullptr, varyings_, GL_NONE,
                             program_sha);
  cache_->Evict(program_sha, a_sha, c_sha);
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader3, nullptr, varyings_,
                                           GL_NONE));
}

TEST_F(ProgramCacheTest, StatusClear) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  const std::string shader3 = "asbjbbjj239a";
  cache_->SaySuccessfullyCached(shader1, shader2, nullptr, varyings_, GL_NONE);
  cache_->SaySuccessfullyCached(shader1, shader3, nullptr, varyings_, GL_NONE);
  cache_->Clear();
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader3, nullptr, varyings_,
                                           GL_NONE));
}

TEST_F(ProgramCacheTest, LinkUnknownOnTransformFeedbackChange) {
  const std::string shader1 = "abcd1234";
  std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  varyings_.push_back("a");
  cache_->SaySuccessfullyCached(shader1, shader2, nullptr, varyings_,
                                GL_INTERLEAVED_ATTRIBS);

  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_SEPARATE_ATTRIBS));

  varyings_.push_back("b");
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_INTERLEAVED_ATTRIBS));
}

TEST_F(ProgramCacheTest, ShaderCompileStatus) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  {
    std::string shader_a = shader1;
    std::string shader_b = shader2;

    EXPECT_EQ(cache_->HasSuccessfullyCompiledShader(shader1), false);
    EXPECT_EQ(cache_->HasSuccessfullyCompiledShader(shader2), false);
    cache_->SaySuccessfullyCached(shader_a, shader_b, nullptr, varyings_,
                                  GL_NONE);

    shader_a.clear();
    shader_b.clear();
  }
  // make sure it was copied
  EXPECT_EQ(ProgramCache::LINK_SUCCEEDED,
            cache_->GetLinkedProgramStatus(shader1, shader2, nullptr, varyings_,
                                           GL_NONE));

  EXPECT_EQ(cache_->HasSuccessfullyCompiledShader(shader1), true);
  EXPECT_EQ(cache_->HasSuccessfullyCompiledShader(shader2), true);
}

}  // namespace gles2
}  // namespace gpu
