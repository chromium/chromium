// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/ipc/service/built_in_shader_cache_loader.h"

#include "base/apple/foundation_util.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/service/built_in_shader_cache_writer.h"

namespace gpu {

namespace {

// File the shaders are put in.
const char kShaderCacheFileName[] = "gpu_shader_cache.bin";

// A single instance is created. It is destroyed once the values are taken.
static BuiltInShaderCacheLoader* g_loader = nullptr;

// Responsible for reading the shader file.
class FileReader {
 public:
  bool Init(const base::FilePath& path) {
    file_.Initialize(AdjustPath(path),
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file_.IsValid()) {
      LOG(WARNING) << "Failed opening metal shader cache";
      return false;
    }
    return ReadHeader();
  }

  bool ReadHeader() {
    uint32_t header;
    if (!ReadBytes(sizeof(header), reinterpret_cast<char*>(&header))) {
      return false;
    }
    return header == BuiltInShaderCacheWriter::kSignature;
  }

  bool ReadKeyOrValue(std::vector<uint8_t>& data) {
    // uint32_t corresponds to how the data is written. See writer for details.
    uint32_t data_size;
    if (!ReadBytes(sizeof(data_size), reinterpret_cast<char*>(&data_size))) {
      return false;
    }
    if (data_size == 0) {
      // Invalid data size.
      return false;
    }
    data.resize(data_size);
    return ReadBytes(data_size, reinterpret_cast<char*>(&data.front()));
  }

 private:
  static base::FilePath AdjustPath(const base::FilePath& path) {
    return path.empty() ? base::apple::PathForFrameworkBundleResource(
                              kShaderCacheFileName)
                        : path;
  }

  bool ReadBytes(uint32_t size, char* data) {
    char* data_ptr = data;
    char* data_end = data + size;
    while (!AtEndOrErrored()) {
      if (!RemainingBytesInBuffer() && !FillBuffer()) {
        return false;
      }
      const uint32_t bytes_to_copy = std::min(
          static_cast<uint32_t>(data_end - data_ptr), RemainingBytesInBuffer());
      CopyFromBufferAndAdvance(bytes_to_copy, data_ptr);
      data_ptr += bytes_to_copy;
      if (data_ptr == data_end) {
        return true;
      }
    }
    return false;
  }

  bool AtEndOrErrored() const { return at_end_or_errored_; }

  void CopyFromBufferAndAdvance(uint32_t num_bytes, char* data) {
    CHECK_LE(num_bytes, RemainingBytesInBuffer());
    memcpy(data, read_buffer_ + current_pos_, num_bytes);
    current_pos_ += num_bytes;
  }

  uint32_t RemainingBytesInBuffer() const {
    CHECK_GE(bytes_available_, current_pos_);
    return bytes_available_ - current_pos_;
  }

  bool FillBuffer() {
    current_pos_ = 0;
    int read = file_.ReadAtCurrentPos(read_buffer_, kReadBufferSize);
    if (read <= 0) {
      bytes_available_ = 0;
      at_end_or_errored_ = true;
      return false;
    }
    bytes_available_ = static_cast<uint32_t>(read);
    return true;
  }

  static constexpr size_t kReadBufferSize = 4096;
  base::File file_;
  char read_buffer_[kReadBufferSize];
  uint32_t current_pos_ = 0;
  uint32_t bytes_available_ = 0;
  bool at_end_or_errored_ = false;
};

}  // namespace

BuiltInShaderCacheLoader::CacheEntry::CacheEntry() = default;
BuiltInShaderCacheLoader::CacheEntry::CacheEntry(CacheEntry&& other) = default;
BuiltInShaderCacheLoader::CacheEntry::~CacheEntry() = default;

// static
void BuiltInShaderCacheLoader::StartLoading() {
  CHECK(!g_loader);
  // Destroyed when finished loading.
  g_loader = new BuiltInShaderCacheLoader;
  auto path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kShaderCachePath);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&BuiltInShaderCacheLoader::Load,
                     base::Unretained(g_loader), path));
}

// static
std::unique_ptr<std::vector<BuiltInShaderCacheLoader::CacheEntry>>
BuiltInShaderCacheLoader::TakeEntries() {
  // This is always called, but StartLoading() is only called if loading is
  // needed.
  if (!g_loader) {
    return std::make_unique<std::vector<CacheEntry>>();
  }
  auto entries = g_loader->TakeEntriesImpl();
  delete g_loader;
  g_loader = nullptr;
  return entries;
}

std::unique_ptr<std::vector<BuiltInShaderCacheLoader::CacheEntry>>
BuiltInShaderCacheLoader::TakeEntriesImpl() {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  loaded_signaler_.Wait();
  const base::TimeTicks end_time = base::TimeTicks::Now();
  base::UmaHistogramCustomMicrosecondsTimes(
      "Gpu.MetalShaderCache.WaitTime", end_time - start_time,
      base::Microseconds(1), base::Milliseconds(100), 100);
  std::unique_ptr<std::vector<CacheEntry>> entries =
      std::make_unique<std::vector<CacheEntry>>(std::move(entries_));
  return entries;
}

BuiltInShaderCacheLoader::BuiltInShaderCacheLoader() = default;

BuiltInShaderCacheLoader::~BuiltInShaderCacheLoader() = default;

void BuiltInShaderCacheLoader::Load(const base::FilePath& path) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  LoadImpl(path);
  const base::TimeTicks end_time = base::TimeTicks::Now();
  base::UmaHistogramCustomMicrosecondsTimes(
      "Gpu.MetalShaderCache.LoadTime", end_time - start_time,
      base::Microseconds(10), base::Milliseconds(100), 100);
  base::UmaHistogramCounts100("Gpu.MetalShaderCache.NumEntriesInCache",
                              entries_.size());
  loaded_signaler_.Signal();
}

void BuiltInShaderCacheLoader::LoadImpl(const base::FilePath& path) {
  FileReader reader;
  if (!reader.Init(path)) {
    return;
  }
  for (;;) {
    CacheEntry entry;
    if (!reader.ReadKeyOrValue(entry.key) ||
        !reader.ReadKeyOrValue(entry.value)) {
      return;
    }
    entries_.push_back(std::move(entry));
  }
}

}  // namespace gpu
