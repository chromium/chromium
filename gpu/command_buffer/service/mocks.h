// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains definitions for mock objects, used for testing.

// TODO(apatrick): This file "manually" defines some mock objects. Using gMock
// would be definitely preferable, unfortunately it doesn't work on Windows yet.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MOCKS_H_
#define GPU_COMMAND_BUFFER_SERVICE_MOCKS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/service/async_api_interface.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/isolation_key_provider.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class CommandBufferDirect;
class CommandBufferServiceBase;

// Mocks an AsyncAPIInterface, using GMock.
class AsyncAPIMock : public AsyncAPIInterface {
 public:
  explicit AsyncAPIMock(bool default_do_commands,
                        CommandBufferDirect* command_buffer,
                        CommandBufferServiceBase* command_buffer_service);
  ~AsyncAPIMock() override;

  error::Error FakeDoCommands(unsigned int num_commands,
                              const volatile void* buffer,
                              int num_entries,
                              int* entries_processed);

  // Predicate that matches args passed to DoCommand, by looking at the values.
  class IsArgs {
   public:
    IsArgs(unsigned int arg_count, const volatile void* args)
        : arg_count_(arg_count),
          args_(static_cast<volatile CommandBufferEntry*>(
              const_cast<volatile void*>(args))) {}

    bool operator()(const volatile void* _args) const {
      const volatile CommandBufferEntry* args =
          static_cast<const volatile CommandBufferEntry*>(_args) + 1;
      for (unsigned int i = 0; i < arg_count_; ++i) {
        if (args[i].value_uint32 != args_[i].value_uint32) return false;
      }
      return true;
    }

   private:
    unsigned int arg_count_;
    raw_ptr<volatile CommandBufferEntry, DanglingUntriaged | AllowPtrArithmetic>
        args_;
  };

  void BeginDecoding() override {}
  void EndDecoding() override {}

  MOCK_METHOD3(DoCommand,
               error::Error(unsigned int command,
                            unsigned int arg_count,
                            const volatile void* cmd_data));

  MOCK_METHOD4(DoCommands,
               error::Error(unsigned int num_commands,
                            const volatile void* buffer,
                            int num_entries,
                            int* entries_processed));

  std::string_view GetLogPrefix() override { return "None"; }

  // Forwards the SetToken commands to the engine.
  void SetToken(unsigned int command,
                unsigned int arg_count,
                const volatile void* _args);

 private:
  raw_ptr<CommandBufferDirect> command_buffer_;
  raw_ptr<CommandBufferServiceBase, DanglingUntriaged> command_buffer_service_;
};

class MockDecoderClient : public DecoderClient {
 public:
  MockDecoderClient();
  ~MockDecoderClient() override;

  MOCK_METHOD(void, OnConsoleMessage, (int32_t id, const std::string& message));
  MOCK_METHOD(void, OnGpuSwitched, (gl::GpuPreference active_gpu_heuristic));
  MOCK_METHOD(void,
              CacheBlob,
              (gpu::GpuDiskCacheType type,
               const std::string& key,
               const std::string& shader));
  MOCK_METHOD(void, OnFenceSyncRelease, (uint64_t release));
  MOCK_METHOD(void, OnDescheduleUntilFinished, ());
  MOCK_METHOD(void, OnRescheduleAfterFinished, ());
  MOCK_METHOD(void, OnSwapBuffers, (uint64_t swap_id, uint32_t flags));
  MOCK_METHOD(void, ScheduleGrContextCleanup, ());
  MOCK_METHOD(void, SetActiveURL, (GURL url));
  MOCK_METHOD(void, HandleReturnData, (base::span<const uint8_t> data));
  MOCK_METHOD(bool, ShouldYield, ());
};

class MockIsolationKeyProvider : public IsolationKeyProvider {
 public:
  MockIsolationKeyProvider();
  ~MockIsolationKeyProvider() override;

  MOCK_METHOD(void,
              GetIsolationKey,
              (const blink::WebGPUExecutionContextToken& token,
               GetIsolationKeyCallback cb),
              (override));
};

namespace gles2 {

class MockShaderTranslator : public ShaderTranslatorInterface {
 public:
  MockShaderTranslator();

  MOCK_METHOD6(Init,
               bool(sh::GLenum shader_type,
                    ShShaderSpec shader_spec,
                    const ShBuiltInResources* resources,
                    ShShaderOutput shader_output_language,
                    const ShCompileOptions& driver_bug_workarounds,
                    bool gl_shader_interm_output));
  MOCK_CONST_METHOD9(Translate,
                     bool(const std::string& shader_source,
                          std::string* info_log,
                          std::string* translated_source,
                          int* shader_version,
                          AttributeMap* attrib_map,
                          UniformMap* uniform_map,
                          VaryingMap* varying_map,
                          InterfaceBlockMap* interface_block_map,
                          OutputVariableList* output_variable_list));
  MOCK_CONST_METHOD0(GetStringForOptionsThatWouldAffectCompilation,
                     OptionsAffectingCompilationString*());

 private:
  ~MockShaderTranslator() override;
};

class MockProgramCache : public ProgramCache {
 public:
  MockProgramCache();
  ~MockProgramCache() override;

  MOCK_METHOD7(LoadLinkedProgram,
               ProgramLoadResult(
                   GLuint program,
                   Shader* shader_a,
                   Shader* shader_b,
                   const LocationMap* bind_attrib_location_map,
                   const std::vector<std::string>& transform_feedback_varyings,
                   GLenum transform_feedback_buffer_mode,
                   DecoderClient* client));

  MOCK_METHOD7(SaveLinkedProgram,
               void(GLuint program,
                    const Shader* shader_a,
                    const Shader* shader_b,
                    const LocationMap* bind_attrib_location_map,
                    const std::vector<std::string>& transform_feedback_varyings,
                    GLenum transform_feedback_buffer_mode,
                    DecoderClient* client));
  MOCK_METHOD2(LoadProgram, void(const std::string&, const std::string&));
  MOCK_METHOD1(Trim, size_t(size_t));

 private:
  MOCK_METHOD0(ClearBackend, void());
};

class MockMemoryTracker : public MemoryTracker {
 public:
  MockMemoryTracker();
  ~MockMemoryTracker() override;

  MOCK_METHOD1(TrackMemoryAllocatedChange, void(int64_t delta));
  uint64_t GetSize() const override { return 0; }
  MOCK_CONST_METHOD0(ClientTracingId, uint64_t());
  MOCK_CONST_METHOD0(ClientId, int());
  MOCK_CONST_METHOD0(ContextGroupTracingId, uint64_t());
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MOCKS_H_
