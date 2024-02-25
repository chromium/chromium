// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class ProgressReporter;
}

namespace gpu {

class DecoderClient;
struct GpuPreferences;

namespace gles2 {

class FeatureInfo;
class ProgramCache;
class ProgramManager;
class Shader;
class ShaderManager;

enum class UniformApiType : uint32_t {
  kUniformNone = 0,
  kUniform1i = 1 << 0,
  kUniform2i = 1 << 1,
  kUniform3i = 1 << 2,
  kUniform4i = 1 << 3,
  kUniform1f = 1 << 4,
  kUniform2f = 1 << 5,
  kUniform3f = 1 << 6,
  kUniform4f = 1 << 7,
  kUniformMatrix2f = 1 << 8,
  kUniformMatrix3f = 1 << 9,
  kUniformMatrix4f = 1 << 10,
  kUniform1ui = 1 << 11,
  kUniform2ui = 1 << 12,
  kUniform3ui = 1 << 13,
  kUniform4ui = 1 << 14,
  kUniformMatrix2x3f = 1 << 15,
  kUniformMatrix2x4f = 1 << 16,
  kUniformMatrix3x2f = 1 << 17,
  kUniformMatrix3x4f = 1 << 18,
  kUniformMatrix4x2f = 1 << 19,
  kUniformMatrix4x3f = 1 << 20,
};

inline constexpr UniformApiType operator|(UniformApiType a, UniformApiType b) {
  return static_cast<UniformApiType>(uint32_t(a) | uint32_t(b));
}

inline constexpr UniformApiType operator&(UniformApiType a, UniformApiType b) {
  return static_cast<UniformApiType>(uint32_t(a) & uint32_t(b));
}

// This is used to track which attributes a particular program needs
// so we can verify at glDrawXXX time that every attribute is either disabled
// or if enabled that it points to a valid source.
class GPU_GLES2_EXPORT Program : public base::RefCounted<Program> {
 public:
  static const int kMaxAttachedShaders = 2;

  struct ProgramOutputInfo {
    ProgramOutputInfo(GLuint _color_name,
                      GLuint _index,
                      const std::string& _name)
        : color_name(_color_name), index(_index), name(_name) {}
    ProgramOutputInfo() : color_name(0), index(0) {}
    GLuint color_name;
    GLuint index;
    std::string name;
  };

  struct UniformInfo {
    UniformInfo();
    UniformInfo(const UniformInfo& other);
    UniformInfo(const std::string& client_name,
                GLint client_location_base,
                GLenum _type,
                bool _is_array,
                const std::vector<GLint>& service_locations);
    ~UniformInfo();
    bool IsSampler() const {
      switch (type) {
        case GL_SAMPLER_2D:
        case GL_SAMPLER_2D_RECT_ARB:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_EXTERNAL_OES:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_SAMPLER_CUBE_SHADOW:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_3D:
        case GL_INT_SAMPLER_CUBE:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          return true;
        default:
          return false;
      }
    }

    GLsizei size;
    GLenum type;
    UniformApiType accepts_api_type;
    GLint fake_location_base;
    bool is_array;
    std::string name;
    std::vector<GLint> element_locations;
    std::vector<GLuint> texture_units;
  };
  struct VertexAttrib {
    VertexAttrib(GLsizei size,
                 GLenum type,
                 const std::string& name,
                 GLint location,
                 size_t location_count)
        : size(size),
          type(type),
          location(location),
          location_count(location_count),
          name(name) {}
    GLsizei size;
    GLenum type;
    GLint location;
    size_t location_count;
    std::string name;
  };
  struct UniformBlockSizeInfo {
    uint32_t binding;
    uint32_t data_size;
  };

  template <typename T>
  class ShaderVariableLocationEntry {
   public:
    ShaderVariableLocationEntry()
        : shader_variable_(nullptr), inactive_(false) {}
    bool IsUnused() const { return !shader_variable_ && !inactive_; }
    bool IsInactive() const { return inactive_; }
    bool IsActive() const { return shader_variable_ != nullptr; }
    void SetInactive() {
      shader_variable_ = nullptr;
      inactive_ = true;
    }
    void SetActive(T* shader_variable) {
      DCHECK(shader_variable);
      shader_variable_ = shader_variable;
      inactive_ = false;
    }
    const T* shader_variable() const {
      DCHECK(IsActive());
      return shader_variable_;
    }
    T* shader_variable() {
      DCHECK(IsActive());
      return shader_variable_;
    }

   private:
    raw_ptr<T> shader_variable_;  // Pointer to *_info_ vector entry.
    bool inactive_;
  };

  typedef std::vector<UniformInfo> UniformInfoVector;
  typedef std::vector<ShaderVariableLocationEntry<UniformInfo>>
      UniformLocationVector;
  typedef std::vector<VertexAttrib> AttribInfoVector;
  typedef std::vector<ProgramOutputInfo> ProgramOutputInfoVector;
  typedef std::vector<int> SamplerIndices;
  typedef std::map<std::string, GLint> LocationMap;
  typedef std::map<std::string, std::pair<GLuint, GLuint>> LocationIndexMap;
  typedef std::vector<std::string> StringVector;

  Program(ProgramManager* manager, GLuint service_id);

  GLuint service_id() const {
    return service_id_;
  }

  const SamplerIndices& sampler_indices() {
    return sampler_indices_;
  }

  const AttribInfoVector& GetAttribInfos() const {
    return attrib_infos_;
  }

  const VertexAttrib* GetAttribInfo(GLint index) const {
    return (static_cast<size_t>(index) < attrib_infos_.size())
               ? &attrib_infos_[index]
               : nullptr;
  }

  GLint GetAttribLocation(const std::string& original_name) const;

  const VertexAttrib* GetAttribInfoByLocation(GLuint location) const {
    if (location < attrib_location_to_index_map_.size()) {
      GLint index = attrib_location_to_index_map_[location];
      if (index >= 0) {
        return &attrib_infos_[index];
      }
    }
    return nullptr;
  }

  const UniformInfo* GetUniformInfo(GLint index) const;

  // If the original name is not found, return nullptr.
  const std::string* GetAttribMappedName(
      const std::string& original_name) const;

  // If the original name is not found, return nullptr.
  const std::string* GetUniformMappedName(
      const std::string& original_name) const;

  // If the hashed name name is not found, return nullptr.
  // Use this only when one of the more specific Get*Info methods can't be used.
  const std::string* GetOriginalNameFromHashedName(
      const std::string& hashed_name) const;

  // If the hashed name is not found, return nullptr.
  const sh::Varying* GetVaryingInfo(const std::string& hashed_name) const;

  // If the hashed name is not found, return nullptr.
  const sh::InterfaceBlock* GetInterfaceBlockInfo(
      const std::string& hashed_name) const;

  // Gets the fake location of a uniform by name.
  GLint GetUniformFakeLocation(const std::string& name) const;

  // Gets the UniformInfo of a uniform by location.
  const UniformInfo* GetUniformInfoByFakeLocation(
      GLint fake_location, GLint* real_location, GLint* array_index) const;

  // Returns true if |fake_location| is a location for an inactive uniform,
  // -1 for bound, non-existing uniform.
  bool IsInactiveUniformLocationByFakeLocation(GLint fake_location) const;

  // Gets the ProgramOutputInfo of a fragment output by name.
  const ProgramOutputInfo* GetProgramOutputInfo(
      const std::string& original_name) const;

  // Gets all the program info.
  void GetProgramInfo(
      ProgramManager* manager, CommonDecoder::Bucket* bucket) const;

  // Gets all the UniformBlock info.
  // Return false on overflow.
  bool GetUniformBlocks(CommonDecoder::Bucket* bucket) const;

  // Gets all the TransformFeedbackVarying info.
  // Return false on overflow.
  bool GetTransformFeedbackVaryings(CommonDecoder::Bucket* bucket) const;

  // Gather all info through glGetActiveUniformsiv, except for size, type,
  // name_length, which we gather through glGetActiveUniform in
  // glGetProgramInfoCHROMIUM.
  bool GetUniformsES3(CommonDecoder::Bucket* bucket) const;

  // Returns the fragment shader output variable color name binding.
  // Returns -1 if |original_name| is not an out variable or error.
  GLint GetFragDataLocation(const std::string& original_name) const;

  // Returns the fragment shader output variable color index binding.
  // Returns -1 if |original_name| is not an out variable or error.
  GLint GetFragDataIndex(const std::string& original_name) const;

  // Sets the sampler values for a uniform.
  // This should be called only for valid fake location pointing to
  // an active uniform.
  // If the location is not a sampler uniform nothing will happen.
  // Returns false if fake_location is a sampler and any value is >=
  // num_texture_units. Returns true otherwise.
  bool SetSamplers(
      GLint num_texture_units, GLint fake_location,
      GLsizei count, const GLint* value);

  bool IsDeleted() const {
    return deleted_;
  }

  void GetProgramiv(GLenum pname, GLint* params);

  bool IsValid() const {
    return valid_;
  }

  bool IsShaderAttached(Shader* shader);
  bool AttachShader(ShaderManager* manager, Shader* shader);
  void DetachShader(ShaderManager* manager, Shader* shader);

  void CompileAttachedShaders();
  bool AttachedShadersExist() const;
  bool CanLink() const;

  // Performs glLinkProgram and related activities.
  bool Link(ShaderManager* manager,
            DecoderClient* client);

  // Performs glValidateProgram and related activities.
  void Validate();

  const std::string* log_info() const {
    return log_info_.get();
  }

  bool InUse() const {
    DCHECK_GE(use_count_, 0);
    return use_count_ != 0;
  }

  // Sets attribute-location binding from a glBindAttribLocation() call.
  void SetAttribLocationBinding(const std::string& attrib, GLint location) {
    bind_attrib_location_map_[attrib] = location;
  }

  // Sets uniform-location binding from a glBindUniformLocationCHROMIUM call.
  // returns false if error.
  bool SetUniformLocationBinding(const std::string& name, GLint location);

  // Detects if the shader version combination is not valid.
  bool DetectShaderVersionMismatch() const;

  // Sets program output variable location. Also sets color index to zero.
  void SetProgramOutputLocationBinding(const std::string& name,
                                       GLuint colorName);

  // Sets program output variable location and color index.
  void SetProgramOutputLocationIndexedBinding(const std::string& name,
                                              GLuint colorName,
                                              GLuint index);

  // Detects if there are attribute location conflicts from
  // glBindAttribLocation() calls.
  // We only consider the declared attributes in the program.
  bool DetectAttribLocationBindingConflicts() const;

  // Detects if there are uniform location conflicts from
  // glBindUniformLocationCHROMIUM() calls.
  // We only consider the statically used uniforms in the program.
  bool DetectUniformLocationBindingConflicts() const;

  // Detects if there are uniforms of the same name but different type
  // or precision in vertex/fragment shaders.
  // Return true and set the first found conflicting hashed name to
  // conflicting_name if such cases are detected.
  bool DetectUniformsMismatch(std::string* conflicting_name) const;

  // Detects if there are interface blocks of the same name but different
  // layouts.
  bool DetectInterfaceBlocksMismatch(std::string* conflicting_name) const;

  // Return true if a varying is statically used in fragment shader, but it
  // is not declared in vertex shader.
  bool DetectVaryingsMismatch(std::string* conflicting_name) const;

  // Detects if there are program output location conflicts from
  // glBindFragDataLocation and ..LocationIndexedEXT calls.
  // We only consider the statically used program outputs in the program.
  bool DetectProgramOutputLocationBindingConflicts() const;

  // Return true if any built-in invariant matching rules are broken as in
  // GLSL ES spec 1.00.17, section 4.6.4, Invariance and Linkage.
  bool DetectBuiltInInvariantConflicts() const;

  // Return true if an uniform and an attribute share the same name.
  bool DetectGlobalNameConflicts(std::string* conflicting_name) const;

  // Return false if varyings can't be packed into the max available
  // varying registers.
  bool CheckVaryingsPacking() const;

  void TransformFeedbackVaryings(GLsizei count, const char* const* varyings,
      GLenum buffer_mode);

  // Visible for testing
  const LocationMap& bind_attrib_location_map() const {
    return bind_attrib_location_map_;
  }

  const std::vector<std::string>& effective_transform_feedback_varyings()
      const {
    return effective_transform_feedback_varyings_;
  }

  GLenum effective_transform_feedback_buffer_mode() const {
    return effective_transform_feedback_buffer_mode_;
  }

  GLint draw_id_uniform_location() const { return draw_id_uniform_location_; }

  GLint base_vertex_uniform_location() const {
    return base_vertex_uniform_location_;
  }

  GLint base_instance_uniform_location() const {
    return base_instance_uniform_location_;
  }

  // See member declaration for details.
  // The data are only valid after a successful link.
  uint32_t fragment_output_type_mask() const {
    return fragment_output_type_mask_;
  }
  uint32_t fragment_output_written_mask() const {
    return fragment_output_written_mask_;
  }

  // The data are only valid after a successful link.
  const std::vector<uint32_t>& vertex_input_base_type_mask() const {
    return vertex_input_base_type_mask_;
  }
  const std::vector<uint32_t>& vertex_input_active_mask() const {
    return vertex_input_active_mask_;
  }

  // Update uniform block binding after a successful glUniformBlockBinding().
  void SetUniformBlockBinding(GLuint index, GLuint binding);

  const std::vector<UniformBlockSizeInfo>& uniform_block_size_info() const {
    return uniform_block_size_info_;
  }

  // Return the transform feedback varying sizes (per vertex).
  // Note that if the bufferMode is GL_INTERLEAVED_ATTRIBS, then there is only
  // one entry and it is the sum of all varying sizes.
  const std::vector<GLsizeiptr>& GetTransformFeedbackVaryingSizes() const {
    return transform_feedback_data_size_per_vertex_;
  }

 private:
  friend class base::RefCounted<Program>;
  friend class ProgramManager;

  ~Program();

  void set_log_info(const char* str) {
    log_info_.reset(str ? new std::string(str) : nullptr);
  }

  void ClearLinkStatus() {
    link_status_ = false;
  }

  void IncUseCount() {
    ++use_count_;
  }

  void DecUseCount() {
    --use_count_;
    DCHECK_GE(use_count_, 0);
  }

  void MarkAsDeleted() {
    DCHECK(!deleted_);
    deleted_ =  true;
  }

  // Resets the program.
  void Reset();

  // Updates the program info after a successful link.
  void Update();
  bool UpdateUniforms();
  void UpdateProgramOutputs();
  void UpdateFragmentOutputBaseTypes();
  void UpdateVertexInputBaseTypes();
  void UpdateUniformBlockSizeInfo();
  void UpdateTransformFeedbackInfo();

  // Process the program log, replacing the hashed names with original names.
  std::string ProcessLogInfo(const std::string& log);

  // Updates the program log info from GL
  void UpdateLogInfo();

  // Updates the draw id uniform location used by ANGLE_multi_draw
  void UpdateDrawIDUniformLocation();

  // Updates the base vertex and base instance uniform location used by
  // ANGLE_base_vertex_base_instance
  void UpdateBaseVertexUniformLocation();

  void UpdateBaseInstanceUniformLocation();

  // If long attribate names are mapped during shader translation, call
  // glBindAttribLocation() again with the mapped names.
  // This is called right before the glLink() call, but after shaders are
  // translated.
  void ExecuteBindAttribLocationCalls();

  // The names of transform feedback varyings need to be hashed just
  // like bound attributes' locations, just before the link call.
  // Returns false upon failure.
  bool ExecuteTransformFeedbackVaryingsCall();

  void ExecuteProgramOutputBindCalls();

  // Query VertexAttrib data returned by ANGLE translator by the mapped name.
  void GetVertexAttribData(
      const std::string& name, std::string* original_name, GLenum* type) const;

  void DetachShaders(ShaderManager* manager);

  static inline size_t GetUniformLocationIndexFromFakeLocation(
      GLint fake_location) {
    return static_cast<size_t>(fake_location & 0xFFFF);
  }

  static inline size_t GetArrayElementIndexFromFakeLocation(
      GLint fake_location) {
    return static_cast<size_t>((fake_location >> 16) & 0xFFFF);
  }

  const FeatureInfo& feature_info() const;

  void ClearVertexInputMasks();

  raw_ptr<ProgramManager> manager_;

  int use_count_;

  GLsizei max_attrib_name_length_;

  // Attrib by index.
  AttribInfoVector attrib_infos_;

  // Attrib by location to index.
  std::vector<GLint> attrib_location_to_index_map_;

  GLsizei max_uniform_name_length_;

  // Uniform info by index.
  UniformInfoVector uniform_infos_;
  UniformLocationVector uniform_locations_;

  // The indices of the uniforms that are samplers.
  SamplerIndices sampler_indices_;

  ProgramOutputInfoVector program_output_infos_;

  // The program this Program is tracking.
  GLuint service_id_;

  // Shaders by type of shader.
  scoped_refptr<Shader> attached_shaders_[kMaxAttachedShaders];
  scoped_refptr<Shader> shaders_from_last_successful_link_[kMaxAttachedShaders];

  // True if this program is marked as deleted.
  bool deleted_;

  // This is true if glLinkProgram was successful at least once.
  bool valid_;

  // This is true if glLinkProgram was successful last time it was called.
  bool link_status_;

  // True if the uniforms have been cleared.
  bool uniforms_cleared_;

  // ANGLE_multi_draw
  GLint draw_id_uniform_location_;

  // ANGLE_base_vertex_base_instance
  GLint base_vertex_uniform_location_;
  GLint base_instance_uniform_location_;

  // Log info
  std::unique_ptr<std::string> log_info_;

  // attribute-location binding map from glBindAttribLocation() calls.
  LocationMap bind_attrib_location_map_;

  // uniform-location binding map from glBindUniformLocationCHROMIUM() calls.
  LocationMap bind_uniform_location_map_;

  // Set by glTransformFeedbackVaryings().
  std::vector<std::string> transform_feedback_varyings_;
  GLenum transform_feedback_buffer_mode_;

  // After a successful link.
  std::vector<std::string> effective_transform_feedback_varyings_;
  GLenum effective_transform_feedback_buffer_mode_;
  // If buffer mode is INTERLEVED, there is only one entry; otherwise there
  // might be multiple entries, one per transform feedback varying.
  // The size requirement is per vertex. Total minimum buffer size requirment
  // is calculated at DrawArrays{Instanced} time by multiplying vertex count.
  std::vector<GLsizeiptr> transform_feedback_data_size_per_vertex_;

  // output variable - (location,index) binding map from
  // glBindFragDataLocation() and ..IndexedEXT() calls.
  LocationIndexMap bind_program_output_location_index_map_;

  // It's stored in the order of uniform block indices, i.e., the first
  // entry is the info about UniformBlock with index 0, etc.
  std::vector<UniformBlockSizeInfo> uniform_block_size_info_;

  // Fragment output variable base types: FLOAT, INT, or UINT.
  // We have up to 16 outputs, each is encoded into 2 bits, total 32 bits:
  // the lowest 2 bits for location 0, the highest 2 bits for location 15.
  uint32_t fragment_output_type_mask_;
  // Same layout as above, 2 bits per location, 0x03 if a location is occupied
  // by an output variable, 0x00 if not.
  uint32_t fragment_output_written_mask_;

  // Vertex input attrib base types: FLOAT, INT, or UINT.
  // Each base type is encoded into 2 bits, the lowest 2 bits for location 0,
  // the highest 2 bits for location (max_vertex_attribs - 1).
  std::vector<uint32_t> vertex_input_base_type_mask_;
  // Same layout as above, 2 bits per location, 0x03 if a location is set
  // by vertexAttrib API, 0x00 if not.
  std::vector<uint32_t> vertex_input_active_mask_;
};

// Tracks the Programs.
//
// NOTE: To support shared resources an instance of this class will
// need to be shared by multiple GLES2Decoders.
class GPU_GLES2_EXPORT ProgramManager {
 public:
  ProgramManager(ProgramCache* program_cache,
                 uint32_t max_varying_vectors,
                 uint32_t max_draw_buffers,
                 uint32_t max_dual_source_draw_buffers,
                 uint32_t max_vertex_attribs,
                 const GpuPreferences& gpu_preferences,
                 FeatureInfo* feature_info,
                 gl::ProgressReporter* progress_reporter);

  ProgramManager(const ProgramManager&) = delete;
  ProgramManager& operator=(const ProgramManager&) = delete;

  ~ProgramManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a new program.
  Program* CreateProgram(GLuint client_id, GLuint service_id);

  // Gets a program.
  Program* GetProgram(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  // Gets the shader cache
  ProgramCache* program_cache() const;

  // Marks a program as deleted. If it is not used the program will be deleted.
  void MarkAsDeleted(ShaderManager* shader_manager, Program* program);

  // Marks a program as used.
  void UseProgram(Program* program);

  // Makes a program as unused. If deleted the program will be removed.
  void UnuseProgram(ShaderManager* shader_manager, Program* program);

  // Updates the draw id location for this program for ANGLE_multi_draw
  void UpdateDrawIDUniformLocation(Program* program);

  // Updates the base vertex location for this program for
  // ANGLE_base_vertex_base_instance
  void UpdateBaseVertexUniformLocation(Program* program);

  // Updates the base instance location for this program for
  // ANGLE_base_vertex_base_instance
  void UpdateBaseInstanceUniformLocation(Program* program);

  // Returns true if |name| has a prefix that is intended for GL built-in shader
  // variables.
  static bool HasBuiltInPrefix(const std::string& name);

  // Check if a Program is owned by this ProgramManager.
  bool IsOwned(Program* program) const;

  // Return true if this shader has compiled status cached.
  bool HasCachedCompileStatus(Shader* shader) const;

  static int32_t MakeFakeLocation(int32_t index, int32_t element);

  uint32_t max_varying_vectors() const { return max_varying_vectors_; }

  uint32_t max_draw_buffers() const { return max_draw_buffers_; }

  uint32_t max_dual_source_draw_buffers() const {
    return max_dual_source_draw_buffers_;
  }

  uint32_t max_vertex_attribs() const { return max_vertex_attribs_; }

 private:
  friend class Program;

  void StartTracking(Program* program);
  void StopTracking(Program* program);

  void RemoveProgramInfoIfUnused(
      ShaderManager* shader_manager, Program* program);

  // Info for each "successfully linked" program by service side program Id.
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, scoped_refptr<Program> > ProgramMap;
  ProgramMap programs_;

  // Counts the number of Program allocated with 'this' as its manager.
  // Allows to check no Program will outlive this.
  unsigned int program_count_;

  bool have_context_;

  // Used to clear uniforms.
  std::vector<uint8_t> zero_;

  raw_ptr<ProgramCache> program_cache_;

  uint32_t max_varying_vectors_;
  uint32_t max_draw_buffers_;
  uint32_t max_dual_source_draw_buffers_;
  uint32_t max_vertex_attribs_;

  const raw_ref<const GpuPreferences> gpu_preferences_;
  scoped_refptr<FeatureInfo> feature_info_;

  // Used to notify the watchdog thread of progress during destruction,
  // preventing time-outs when destruction takes a long time. May be null when
  // using in-process command buffer.
  raw_ptr<gl::ProgressReporter> progress_reporter_;
};

inline const FeatureInfo& Program::feature_info() const {
  return *manager_->feature_info_.get();
}

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_
