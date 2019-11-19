// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/memory_program_cache.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/disk_cache_proto.pb.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/zlib/zlib.h"
#include "ui/gl/gl_bindings.h"

// Macro to help with logging times under 10ms.
#define UMA_HISTOGRAM_VERY_SHORT_TIMES(name, time_delta)                       \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                                                 \
      name,                                                                    \
      static_cast<base::HistogramBase::Sample>((time_delta).InMicroseconds()), \
      1,                                                                       \
      static_cast<base::HistogramBase::Sample>(                                \
          base::TimeDelta::FromMilliseconds(10).InMicroseconds()),             \
      50);

namespace gpu {
namespace gles2 {

namespace {

void FillShaderVariableProto(
    ShaderVariableProto* proto, const sh::ShaderVariable& variable) {
  proto->set_type(variable.type);
  proto->set_precision(variable.precision);
  proto->set_name(variable.name);
  proto->set_mapped_name(variable.mappedName);
  proto->set_array_size(variable.getOutermostArraySize());
  proto->set_static_use(variable.staticUse);
  for (size_t ii = 0; ii < variable.fields.size(); ++ii) {
    ShaderVariableProto* field = proto->add_fields();
    FillShaderVariableProto(field, variable.fields[ii]);
  }
  proto->set_struct_name(variable.structName);
}

void FillShaderAttributeProto(
    ShaderAttributeProto* proto, const sh::Attribute& attrib) {
  FillShaderVariableProto(proto->mutable_basic(), attrib);
  proto->set_location(attrib.location);
}

void FillShaderUniformProto(
    ShaderUniformProto* proto, const sh::Uniform& uniform) {
  FillShaderVariableProto(proto->mutable_basic(), uniform);
}

void FillShaderVaryingProto(
    ShaderVaryingProto* proto, const sh::Varying& varying) {
  FillShaderVariableProto(proto->mutable_basic(), varying);
  proto->set_interpolation(varying.interpolation);
  proto->set_is_invariant(varying.isInvariant);
}

void FillShaderOutputVariableProto(ShaderOutputVariableProto* proto,
                                   const sh::OutputVariable& attrib) {
  FillShaderVariableProto(proto->mutable_basic(), attrib);
  proto->set_location(attrib.location);
}

void FillShaderInterfaceBlockFieldProto(
    ShaderInterfaceBlockFieldProto* proto,
    const sh::InterfaceBlockField& interfaceBlockField) {
  FillShaderVariableProto(proto->mutable_basic(), interfaceBlockField);
  proto->set_is_row_major_layout(interfaceBlockField.isRowMajorLayout);
}

void FillShaderInterfaceBlockProto(ShaderInterfaceBlockProto* proto,
    const sh::InterfaceBlock& interfaceBlock) {
  proto->set_name(interfaceBlock.name);
  proto->set_mapped_name(interfaceBlock.mappedName);
  proto->set_instance_name(interfaceBlock.instanceName);
  proto->set_array_size(interfaceBlock.arraySize);
  proto->set_layout(interfaceBlock.layout);
  proto->set_is_row_major_layout(interfaceBlock.isRowMajorLayout);
  proto->set_static_use(interfaceBlock.staticUse);
  for (size_t ii = 0; ii < interfaceBlock.fields.size(); ++ii) {
    ShaderInterfaceBlockFieldProto* field = proto->add_fields();
    FillShaderInterfaceBlockFieldProto(field, interfaceBlock.fields[ii]);
  }
}

void FillShaderProto(ShaderProto* proto, const char* sha,
                     const Shader* shader) {
  proto->set_sha(sha, gpu::gles2::ProgramCache::kHashLength);
  for (AttributeMap::const_iterator iter = shader->attrib_map().begin();
       iter != shader->attrib_map().end(); ++iter) {
    ShaderAttributeProto* info = proto->add_attribs();
    FillShaderAttributeProto(info, iter->second);
  }
  for (UniformMap::const_iterator iter = shader->uniform_map().begin();
       iter != shader->uniform_map().end(); ++iter) {
    ShaderUniformProto* info = proto->add_uniforms();
    FillShaderUniformProto(info, iter->second);
  }
  for (VaryingMap::const_iterator iter = shader->varying_map().begin();
       iter != shader->varying_map().end(); ++iter) {
    ShaderVaryingProto* info = proto->add_varyings();
    FillShaderVaryingProto(info, iter->second);
  }
  for (auto iter = shader->output_variable_list().begin();
       iter != shader->output_variable_list().end(); ++iter) {
    ShaderOutputVariableProto* info = proto->add_output_variables();
    FillShaderOutputVariableProto(info, *iter);
  }
  for (InterfaceBlockMap::const_iterator iter =
       shader->interface_block_map().begin();
       iter != shader->interface_block_map().end(); ++iter) {
    ShaderInterfaceBlockProto* info = proto->add_interface_blocks();
    FillShaderInterfaceBlockProto(info, iter->second);
  }
}

void RetrieveShaderVariableInfo(
    const ShaderVariableProto& proto, sh::ShaderVariable* variable) {
  variable->type = proto.type();
  variable->precision = proto.precision();
  variable->name = proto.name();
  variable->mappedName = proto.mapped_name();
  variable->setArraySize(proto.array_size());
  variable->staticUse = proto.static_use();
  variable->fields.resize(proto.fields_size());
  for (int ii = 0; ii < proto.fields_size(); ++ii)
    RetrieveShaderVariableInfo(proto.fields(ii), &(variable->fields[ii]));
  variable->structName = proto.struct_name();
}

void RetrieveShaderAttributeInfo(
    const ShaderAttributeProto& proto, AttributeMap* map) {
  sh::Attribute attrib;
  RetrieveShaderVariableInfo(proto.basic(), &attrib);
  attrib.location = proto.location();
  (*map)[proto.basic().mapped_name()] = attrib;
}

void RetrieveShaderUniformInfo(
    const ShaderUniformProto& proto, UniformMap* map) {
  sh::Uniform uniform;
  RetrieveShaderVariableInfo(proto.basic(), &uniform);
  (*map)[proto.basic().mapped_name()] = uniform;
}

void RetrieveShaderVaryingInfo(
    const ShaderVaryingProto& proto, VaryingMap* map) {
  sh::Varying varying;
  RetrieveShaderVariableInfo(proto.basic(), &varying);
  varying.interpolation = static_cast<sh::InterpolationType>(
      proto.interpolation());
  varying.isInvariant = proto.is_invariant();
  (*map)[proto.basic().mapped_name()] = varying;
}

void RetrieveShaderOutputVariableInfo(const ShaderOutputVariableProto& proto,
                                      OutputVariableList* list) {
  sh::OutputVariable output_variable;
  RetrieveShaderVariableInfo(proto.basic(), &output_variable);
  output_variable.location = proto.location();
  list->push_back(output_variable);
}

void RetrieveShaderInterfaceBlockFieldInfo(
    const ShaderInterfaceBlockFieldProto& proto,
    sh::InterfaceBlockField* interface_block_field) {
  RetrieveShaderVariableInfo(proto.basic(), interface_block_field);
  interface_block_field->isRowMajorLayout = proto.is_row_major_layout();
}

void RetrieveShaderInterfaceBlockInfo(const ShaderInterfaceBlockProto& proto,
                                      InterfaceBlockMap* map) {
  sh::InterfaceBlock interface_block;
  interface_block.name = proto.name();
  interface_block.mappedName = proto.mapped_name();
  interface_block.instanceName = proto.instance_name();
  interface_block.arraySize = proto.array_size();
  interface_block.layout = static_cast<sh::BlockLayoutType>(proto.layout());
  interface_block.isRowMajorLayout = proto.is_row_major_layout();
  interface_block.staticUse = proto.static_use();
  interface_block.fields.resize(proto.fields_size());
  for (int ii = 0; ii < proto.fields_size(); ++ii) {
    RetrieveShaderInterfaceBlockFieldInfo(proto.fields(ii),
        &(interface_block.fields[ii]));
  }
  (*map)[proto.mapped_name()] = interface_block;
}

void RunShaderCallback(DecoderClient* client,
                       GpuProgramProto* proto,
                       std::string sha_string) {
  std::string shader;
  proto->SerializeToString(&shader);

  std::string key;
  base::Base64Encode(sha_string, &key);
  client->CacheShader(key, shader);
}

bool ProgramBinaryExtensionsAvailable() {
  return gl::g_current_gl_driver &&
         (gl::g_current_gl_driver->ext.b_GL_ARB_get_program_binary ||
          gl::g_current_gl_driver->ext.b_GL_OES_get_program_binary);
}

// Returns an empty vector if compression fails.
std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data) {
  auto start_time = base::TimeTicks::Now();
  uLongf compressed_size = compressBound(data.size());
  std::vector<uint8_t> compressed_data(compressed_size);
  // Level indicates a trade-off between compression and speed. Level 1
  // indicates fastest speed (with worst compression).
  auto result = compress2(compressed_data.data(), &compressed_size, data.data(),
                          data.size(), 1 /* level */);
  // It should be impossible for compression to fail with the provided
  // parameters.
  bool success = Z_OK == result;
  UMA_HISTOGRAM_BOOLEAN("GPU.ProgramCache.CompressDataSuccess", success);
  if (!success)
    return std::vector<uint8_t>();

  compressed_data.resize(compressed_size);
  compressed_data.shrink_to_fit();

  UMA_HISTOGRAM_VERY_SHORT_TIMES("GPU.ProgramCache.CompressDataTime",
                                 base::TimeTicks::Now() - start_time);
  UMA_HISTOGRAM_PERCENTAGE("GPU.ProgramCache.CompressionPercentage",
                           (100 * compressed_size) / data.size());

  return compressed_data;
}

// Returns an empty vector if decompression fails.
std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& data,
                                    size_t decompressed_size,
                                    size_t max_size_bytes) {
  auto start_time = base::TimeTicks::Now();
  std::vector<uint8_t> decompressed_data(decompressed_size);
  uLongf decompressed_size_out =
      static_cast<uLongf>(decompressed_size);
  auto result = uncompress(decompressed_data.data(), &decompressed_size_out,
                           data.data(), data.size());

  bool success =
      result == Z_OK && decompressed_data.size() == decompressed_size_out;
  UMA_HISTOGRAM_BOOLEAN("GPU.ProgramCache.DecompressDataSuccess", success);
  if (!success)
    return std::vector<uint8_t>();

  UMA_HISTOGRAM_VERY_SHORT_TIMES("GPU.ProgramCache.DecompressDataTime",
                                 base::TimeTicks::Now() - start_time);

  return decompressed_data;
}

bool CompressProgramBinaries() {
#if !defined(OS_ANDROID)
  return false;
#else   // !defined(OS_ANDROID)
  return base::SysInfo::IsLowEndDevice();
#endif  // !defined(OS_ANDROID)
}

}  // namespace

MemoryProgramCache::MemoryProgramCache(
    size_t max_cache_size_bytes,
    bool disable_gpu_shader_disk_cache,
    bool disable_program_caching_for_transform_feedback,
    GpuProcessActivityFlags* activity_flags)
    : ProgramCache(max_cache_size_bytes),
      disable_gpu_shader_disk_cache_(disable_gpu_shader_disk_cache),
      disable_program_caching_for_transform_feedback_(
          disable_program_caching_for_transform_feedback),
      compress_program_binaries_(CompressProgramBinaries()),
      curr_size_bytes_(0),
      store_(ProgramMRUCache::NO_AUTO_EVICT),
      activity_flags_(activity_flags) {}

MemoryProgramCache::~MemoryProgramCache() = default;

void MemoryProgramCache::ClearBackend() {
  store_.Clear();
  DCHECK_EQ(0U, curr_size_bytes_);
}

ProgramCache::ProgramLoadResult MemoryProgramCache::LoadLinkedProgram(
    GLuint program,
    Shader* shader_a,
    Shader* shader_b,
    const LocationMap* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode,
    DecoderClient* client) {
  if (!ProgramBinaryExtensionsAvailable()) {
    // Early exit if this context can't support program binaries
    return PROGRAM_LOAD_FAILURE;
  }

  char a_sha[kHashLength];
  char b_sha[kHashLength];
  DCHECK(shader_a && !shader_a->last_compiled_source().empty() &&
         shader_b && !shader_b->last_compiled_source().empty());
  ComputeShaderHash(
      shader_a->last_compiled_signature(), a_sha);
  ComputeShaderHash(
      shader_b->last_compiled_signature(), b_sha);

  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     transform_feedback_varyings,
                     transform_feedback_buffer_mode,
                     sha);
  const std::string sha_string(sha, kHashLength);

  ProgramMRUCache::iterator found = store_.Get(sha_string);
  if (found == store_.end()) {
    return PROGRAM_LOAD_FAILURE;
  }
  const scoped_refptr<ProgramCacheValue> value = found->second;
  const std::vector<uint8_t>& decoded =
      value->is_compressed()
          ? DecompressData(value->data(), value->decompressed_length(),
                           max_size_bytes())
          : value->data();
  if (decoded.empty()) {
    // Decompression failure.
    DCHECK(value->is_compressed());
    return PROGRAM_LOAD_FAILURE;
  }

  {
    GpuProcessActivityFlags::ScopedSetFlag scoped_set_flag(
        activity_flags_, ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY);
    glProgramBinary(program, value->format(),
                    static_cast<const GLvoid*>(decoded.data()), decoded.size());
  }

  GLint success = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (success == GL_FALSE) {
    return PROGRAM_LOAD_FAILURE;
  }
  shader_a->set_attrib_map(value->attrib_map_0());
  shader_a->set_uniform_map(value->uniform_map_0());
  shader_a->set_varying_map(value->varying_map_0());
  shader_a->set_output_variable_list(value->output_variable_list_0());
  shader_a->set_interface_block_map(value->interface_block_map_0());
  shader_b->set_attrib_map(value->attrib_map_1());
  shader_b->set_uniform_map(value->uniform_map_1());
  shader_b->set_varying_map(value->varying_map_1());
  shader_b->set_output_variable_list(value->output_variable_list_1());
  shader_b->set_interface_block_map(value->interface_block_map_1());

  if (!disable_gpu_shader_disk_cache_) {
    std::unique_ptr<GpuProgramProto> proto(
        GpuProgramProto::default_instance().New());
    proto->set_sha(sha, kHashLength);
    proto->set_format(value->format());
    proto->set_program(value->data().data(), value->data().size());
    proto->set_program_is_compressed(value->is_compressed());
    proto->set_program_decompressed_length(value->decompressed_length());

    FillShaderProto(proto->mutable_vertex_shader(), a_sha, shader_a);
    FillShaderProto(proto->mutable_fragment_shader(), b_sha, shader_b);
    RunShaderCallback(client, proto.get(), sha_string);
  }

  return PROGRAM_LOAD_SUCCESS;
}

void MemoryProgramCache::SaveLinkedProgram(
    GLuint program,
    const Shader* shader_a,
    const Shader* shader_b,
    const LocationMap* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode,
    DecoderClient* client) {
  if (!ProgramBinaryExtensionsAvailable()) {
    // Early exit if this context can't support program binaries
    return;
  }
  if (disable_program_caching_for_transform_feedback_ &&
      !transform_feedback_varyings.empty()) {
    return;
  }
  GLenum format;
  GLsizei length = 0;
  glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &length);
  if (length == 0 || static_cast<unsigned int>(length) > max_size_bytes()) {
    return;
  }
  std::vector<uint8_t> binary(length);
  glGetProgramBinary(program, length, nullptr, &format,
                     reinterpret_cast<char*>(binary.data()));

  if (compress_program_binaries_) {
    binary = CompressData(binary);
    if (binary.empty()) {
      // Zero size indicates failure.
      return;
    }
  }

  // If the binary is so big it will never fit in the cache, throw it away.
  if (binary.size() > max_size_bytes())
    return;

  UMA_HISTOGRAM_COUNTS_1M("GPU.ProgramCache.ProgramBinarySizeBytes",
                          binary.size());

  char a_sha[kHashLength];
  char b_sha[kHashLength];
  DCHECK(shader_a && !shader_a->last_compiled_source().empty() &&
         shader_b && !shader_b->last_compiled_source().empty());
  ComputeShaderHash(
      shader_a->last_compiled_signature(), a_sha);
  ComputeShaderHash(
      shader_b->last_compiled_signature(), b_sha);

  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     transform_feedback_varyings,
                     transform_feedback_buffer_mode,
                     sha);
  const std::string sha_string(sha, sizeof(sha));

  UMA_HISTOGRAM_COUNTS_1M("GPU.ProgramCache.MemorySizeBeforeKb",
                          curr_size_bytes_ / 1024);

  // Evict any cached program with the same key in favor of the least recently
  // accessed.
  ProgramMRUCache::iterator existing = store_.Peek(sha_string);
  if(existing != store_.end())
    store_.Erase(existing);

  // If the cache is overflowing, remove some old entries.
  DCHECK(max_size_bytes() >= binary.size());
  Trim(max_size_bytes() - binary.size());

  if (!disable_gpu_shader_disk_cache_) {
    std::unique_ptr<GpuProgramProto> proto(
        GpuProgramProto::default_instance().New());
    proto->set_sha(sha, kHashLength);
    proto->set_format(format);
    proto->set_program(binary.data(), binary.size());
    proto->set_program_decompressed_length(length);
    proto->set_program_is_compressed(compress_program_binaries_);

    FillShaderProto(proto->mutable_vertex_shader(), a_sha, shader_a);
    FillShaderProto(proto->mutable_fragment_shader(), b_sha, shader_b);
    RunShaderCallback(client, proto.get(), sha_string);
  }

  store_.Put(
      sha_string,
      new ProgramCacheValue(
          format, std::move(binary), compress_program_binaries_, length,
          sha_string, a_sha, shader_a->attrib_map(), shader_a->uniform_map(),
          shader_a->varying_map(), shader_a->output_variable_list(),
          shader_a->interface_block_map(), b_sha, shader_b->attrib_map(),
          shader_b->uniform_map(), shader_b->varying_map(),
          shader_b->output_variable_list(), shader_b->interface_block_map(),
          this));

  UMA_HISTOGRAM_COUNTS_1M("GPU.ProgramCache.MemorySizeAfterKb",
                          curr_size_bytes_ / 1024);
}

void MemoryProgramCache::LoadProgram(const std::string& key,
                                     const std::string& program) {
  std::unique_ptr<GpuProgramProto> proto(
      GpuProgramProto::default_instance().New());
  if (proto->ParseFromString(program)) {
    AttributeMap vertex_attribs;
    UniformMap vertex_uniforms;
    VaryingMap vertex_varyings;
    OutputVariableList vertex_output_variables;
    InterfaceBlockMap vertex_interface_blocks;
    for (int i = 0; i < proto->vertex_shader().attribs_size(); i++) {
      RetrieveShaderAttributeInfo(proto->vertex_shader().attribs(i),
                                  &vertex_attribs);
    }
    for (int i = 0; i < proto->vertex_shader().uniforms_size(); i++) {
      RetrieveShaderUniformInfo(proto->vertex_shader().uniforms(i),
                                &vertex_uniforms);
    }
    for (int i = 0; i < proto->vertex_shader().varyings_size(); i++) {
      RetrieveShaderVaryingInfo(proto->vertex_shader().varyings(i),
                                &vertex_varyings);
    }
    for (int i = 0; i < proto->vertex_shader().output_variables_size(); i++) {
      RetrieveShaderOutputVariableInfo(
          proto->vertex_shader().output_variables(i), &vertex_output_variables);
    }
    for (int i = 0; i < proto->vertex_shader().interface_blocks_size(); i++) {
      RetrieveShaderInterfaceBlockInfo(
          proto->vertex_shader().interface_blocks(i), &vertex_interface_blocks);
    }

    AttributeMap fragment_attribs;
    UniformMap fragment_uniforms;
    VaryingMap fragment_varyings;
    OutputVariableList fragment_output_variables;
    InterfaceBlockMap fragment_interface_blocks;
    for (int i = 0; i < proto->fragment_shader().attribs_size(); i++) {
      RetrieveShaderAttributeInfo(proto->fragment_shader().attribs(i),
                                  &fragment_attribs);
    }
    for (int i = 0; i < proto->fragment_shader().uniforms_size(); i++) {
      RetrieveShaderUniformInfo(proto->fragment_shader().uniforms(i),
                                &fragment_uniforms);
    }
    for (int i = 0; i < proto->fragment_shader().varyings_size(); i++) {
      RetrieveShaderVaryingInfo(proto->fragment_shader().varyings(i),
                                &fragment_varyings);
    }
    for (int i = 0; i < proto->fragment_shader().output_variables_size(); i++) {
      RetrieveShaderOutputVariableInfo(
          proto->fragment_shader().output_variables(i),
          &fragment_output_variables);
    }
    for (int i = 0; i < proto->fragment_shader().interface_blocks_size(); i++) {
      RetrieveShaderInterfaceBlockInfo(
          proto->fragment_shader().interface_blocks(i),
          &fragment_interface_blocks);
    }

    std::vector<uint8_t> binary(proto->program().length());
    memcpy(binary.data(), proto->program().c_str(), proto->program().length());

    store_.Put(
        proto->sha(),
        new ProgramCacheValue(
            proto->format(), std::move(binary),
            proto->has_program_is_compressed() &&
                proto->program_is_compressed(),
            proto->program_decompressed_length(), proto->sha(),
            proto->vertex_shader().sha().c_str(), vertex_attribs,
            vertex_uniforms, vertex_varyings, vertex_output_variables,
            vertex_interface_blocks, proto->fragment_shader().sha().c_str(),
            fragment_attribs, fragment_uniforms, fragment_varyings,
            fragment_output_variables, fragment_interface_blocks, this));

    UMA_HISTOGRAM_COUNTS_1M("GPU.ProgramCache.MemorySizeAfterKb",
                            curr_size_bytes_ / 1024);
  } else {
    DVLOG(2) << "Failed to parse proto file.";
  }
}

size_t MemoryProgramCache::Trim(size_t limit) {
  size_t initial_size = curr_size_bytes_;
  while (curr_size_bytes_ > limit) {
    DCHECK(!store_.empty());
    store_.Erase(store_.rbegin());
  }
  return initial_size - curr_size_bytes_;
}

MemoryProgramCache::ProgramCacheValue::ProgramCacheValue(
    GLenum format,
    std::vector<uint8_t> data,
    bool is_compressed,
    GLsizei decompressed_length,
    const std::string& program_hash,
    const char* shader_0_hash,
    const AttributeMap& attrib_map_0,
    const UniformMap& uniform_map_0,
    const VaryingMap& varying_map_0,
    const OutputVariableList& output_variable_list_0,
    const InterfaceBlockMap& interface_block_map_0,
    const char* shader_1_hash,
    const AttributeMap& attrib_map_1,
    const UniformMap& uniform_map_1,
    const VaryingMap& varying_map_1,
    const OutputVariableList& output_variable_list_1,
    const InterfaceBlockMap& interface_block_map_1,
    MemoryProgramCache* program_cache)
    : format_(format),
      data_(std::move(data)),
      is_compressed_(is_compressed),
      decompressed_length_(decompressed_length),
      program_hash_(program_hash),
      shader_0_hash_(shader_0_hash, kHashLength),
      attrib_map_0_(attrib_map_0),
      uniform_map_0_(uniform_map_0),
      varying_map_0_(varying_map_0),
      output_variable_list_0_(output_variable_list_0),
      interface_block_map_0_(interface_block_map_0),
      shader_1_hash_(shader_1_hash, kHashLength),
      attrib_map_1_(attrib_map_1),
      uniform_map_1_(uniform_map_1),
      varying_map_1_(varying_map_1),
      output_variable_list_1_(output_variable_list_1),
      interface_block_map_1_(interface_block_map_1),
      program_cache_(program_cache) {
  program_cache_->curr_size_bytes_ += data_.size();
  program_cache->CompiledShaderCacheSuccess(shader_0_hash_);
  program_cache->CompiledShaderCacheSuccess(shader_1_hash_);
  program_cache_->LinkedProgramCacheSuccess(program_hash);
}

MemoryProgramCache::ProgramCacheValue::~ProgramCacheValue() {
  program_cache_->curr_size_bytes_ -= data_.size();
  program_cache_->Evict(program_hash_, shader_0_hash_, shader_1_hash_);
}

}  // namespace gles2
}  // namespace gpu
