// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/built_in_shader_cache_writer.h"

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/config/gpu_switches.h"

namespace gpu {

namespace {

// Returns the path to use is no path is supplied.
base::FilePath GetDefaultPath() {
  auto path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kShaderCachePath);
  return path.empty() ? base::FilePath::FromASCII("/tmp/shader") : path;
}

}  // namespace

const uint32_t BuiltInShaderCacheWriter::kSignature = 0x53484430;

BuiltInShaderCacheWriter::BuiltInShaderCacheWriter(const base::FilePath& path)
    : file_(path.empty() ? GetDefaultPath() : path,
            base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_OPEN_TRUNCATED |
                base::File::FLAG_WRITE) {
  valid_file_ = file_.IsValid() && file_.WriteAtCurrentPosAndCheck(
                                       base::byte_span_from_ref(kSignature));
}

BuiltInShaderCacheWriter::~BuiltInShaderCacheWriter() = default;

void BuiltInShaderCacheWriter::OnValueAddedToCache(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& value) {
  CHECK(!key.empty());
  CHECK(!value.empty());
  LOG(ERROR) << "Added shader cache value, count is " << ++add_count_;
  valid_file_ &= WriteVectorToFile(key);
  valid_file_ &= WriteVectorToFile(value);
  if (valid_file_) {
    // Flush after every entry because the gpu is not cleanly shutdown.
    file_.Flush();
  }
}

bool BuiltInShaderCacheWriter::WriteVectorToFile(
    const std::vector<uint8_t>& value) {
  // The cache has a max size which is represented by a uint32_t (see
  // `GpuPreferences::gpu_program_cache_size`), additionally the current max
  // is ~6mb (anything above the max is dropped, and shouldn't result in
  // calling this). For this reason `uint32_t` is used.
  const uint32_t size = base::checked_cast<uint32_t>(value.size());
  return file_.WriteAtCurrentPosAndCheck(base::byte_span_from_ref(size)) &&
         file_.WriteAtCurrentPosAndCheck(value);
}

}  // namespace gpu
