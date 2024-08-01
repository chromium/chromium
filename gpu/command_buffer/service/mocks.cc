// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/mocks.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/command_buffer_service.h"

using testing::Invoke;
using testing::_;

namespace gpu {

AsyncAPIMock::AsyncAPIMock(bool default_do_commands,
                           CommandBufferDirect* command_buffer,
                           CommandBufferServiceBase* command_buffer_service)
    : command_buffer_(command_buffer),
      command_buffer_service_(command_buffer_service) {
  testing::DefaultValue<error::Error>::Set(
      error::kNoError);

  if (default_do_commands) {
    ON_CALL(*this, DoCommands(_, _, _, _))
        .WillByDefault(Invoke(this, &AsyncAPIMock::FakeDoCommands));
  }
  if (command_buffer_) {
    command_buffer_->set_handler(this);
  }
}

AsyncAPIMock::~AsyncAPIMock() {
  if (command_buffer_) {
    command_buffer_->set_handler(nullptr);
  }
}

error::Error AsyncAPIMock::FakeDoCommands(unsigned int num_commands,
                                          const volatile void* buffer,
                                          int num_entries,
                                          int* entries_processed) {
  DCHECK(entries_processed);
  int commands_to_process = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process--) {
    CommandHeader header = CommandHeader::FromVolatile(cmd_data->value_header);
    DCHECK_GT(header.size, 0u);
    DCHECK_LE(static_cast<int>(header.size) + process_pos, num_entries);

    const unsigned int command = header.command;
    const unsigned int arg_count = header.size - 1;

    result = DoCommand(command, arg_count, cmd_data);

    if (result != error::kDeferCommandUntilLater) {
      process_pos += header.size;
      cmd_data += header.size;
    }
  }

  *entries_processed = process_pos;

  return result;
}

void AsyncAPIMock::SetToken(unsigned int command,
                            unsigned int arg_count,
                            const volatile void* _args) {
  DCHECK(command_buffer_service_);
  DCHECK_EQ(1u, command);
  DCHECK_EQ(1u, arg_count);
  const volatile cmd::SetToken* args =
      static_cast<const volatile cmd::SetToken*>(_args);
  command_buffer_service_->SetToken(args->token);
}

MockDecoderClient::MockDecoderClient() = default;
MockDecoderClient::~MockDecoderClient() = default;

MockIsolationKeyProvider::MockIsolationKeyProvider() = default;
MockIsolationKeyProvider::~MockIsolationKeyProvider() = default;

namespace gles2 {

MockShaderTranslator::MockShaderTranslator() = default;

MockShaderTranslator::~MockShaderTranslator() = default;

MockProgramCache::MockProgramCache() : ProgramCache(0) {}
MockProgramCache::~MockProgramCache() = default;

MockMemoryTracker::MockMemoryTracker() = default;
MockMemoryTracker::~MockMemoryTracker() = default;

}  // namespace gles2
}  // namespace gpu
