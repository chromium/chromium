// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include "base/logging.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

using v8::MaybeLocal;
using std::ref;
using std::lock_guard;
using std::mutex;
using std::chrono::time_point;
using std::chrono::steady_clock;
using std::chrono::seconds;
using std::chrono::duration_cast;

static const seconds kSleepSeconds(1);

// Because of the sleep we do, the actual max will be:
// kSleepSeconds + kMaxExecutionSeconds.
// TODO(metzman): Determine if having such a short timeout causes too much
// indeterminism.
static const seconds kMaxExecutionSeconds(7);

// Inspired by/copied from d8 code, this allocator will return nullptr when
// an allocation request is made that puts currently_allocated_ over
// kAllocationLimit (1 GB). Should handle the current allocations done by V8.
class MockArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
  std::unique_ptr<Allocator> allocator_ =
      std::unique_ptr<Allocator>(NewDefaultAllocator());

  const size_t kAllocationLimit = 1000 * 1024 * 1024;
  // TODO(metzman): Determine if this approach where we keep track of state
  // between runs is a good idea. Maybe we should simply prevent allocations
  // over a certain size regardless of previous allocations.
  size_t currently_allocated_;
  mutex mtx_;

 public:
  MockArrayBufferAllocator()
      : v8::ArrayBuffer::Allocator(), currently_allocated_(0) {}

  void* Allocate(size_t length) override {
    void* data = AllocateUninitialized(length);
    return data == nullptr ? data : memset(data, 0, length);
  }

  void* AllocateUninitialized(size_t length) override {
    lock_guard<mutex> mtx_locker(mtx_);
    if (length + currently_allocated_ > kAllocationLimit) {
      return nullptr;
    }
    currently_allocated_ += length;
    return malloc(length);
  }

  void Free(void* ptr, size_t length) override {
    lock_guard<mutex> mtx_locker(mtx_);
    currently_allocated_ -= length;
    // We need to free before we unlock, otherwise currently_allocated_ will
    // be innacurate.
    free(ptr);
  }
};

void terminate_execution(v8::Isolate* isolate,
                         mutex& mtx,
                         bool& is_running,
                         time_point<steady_clock>& start_time) {
  while (true) {
    std::this_thread::sleep_for(kSleepSeconds);
    lock_guard<mutex> mtx_locker(mtx);
    if (is_running) {
      if (duration_cast<seconds>(steady_clock::now() - start_time) >
          kMaxExecutionSeconds) {
        isolate->TerminateExecution();
        is_running = false;
        std::cout << "Terminated" << std::endl;
        fflush(0);
      }
    }
  }
}

struct Environment {
  Environment() {
    platform_ = v8::platform::NewDefaultPlatform(
        0, v8::platform::IdleTaskSupport::kDisabled,
        v8::platform::InProcessStackDumping::kDisabled, nullptr);

    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();
    v8::Isolate::CreateParams create_params;

    create_params.array_buffer_allocator = &mock_arraybuffer_allocator;
    isolate = v8::Isolate::New(create_params);
    terminator_thread = std::thread(terminate_execution, isolate, ref(mtx),
                                    ref(is_running), ref(start_time));
  }
  MockArrayBufferAllocator mock_arraybuffer_allocator;
  mutex mtx;
  std::thread terminator_thread;
  v8::Isolate* isolate;
  std::unique_ptr<v8::Platform> platform_;
  time_point<steady_clock> start_time;
  bool is_running = true;
};

// Explicitly specify some attributes to avoid issues with the linker dead-
// stripping the following function on macOS, as it is not called directly
// by fuzz target. LibFuzzer runtime uses dlsym() to resolve that function.
extern "C" __attribute__((used)) __attribute__((visibility("default"))) int
LLVMFuzzerInitialize(int* argc, char*** argv) {
  v8::V8::InitializeICUDefaultLocation((*argv)[0]);
  v8::V8::InitializeExternalStartupData((*argv)[0]);
  v8::V8::SetFlagsFromCommandLine(argc, *argv, true);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment* env = new Environment();

  if (size < 1)
    return 0;

  v8::Isolate::Scope isolate_scope(env->isolate);
  v8::HandleScope handle_scope(env->isolate);
  v8::Local<v8::Context> context = v8::Context::New(env->isolate);
  v8::Context::Scope context_scope(context);

  std::string source_string =
      std::string(reinterpret_cast<const char*>(data), size);

  MaybeLocal<v8::String> source_v8_string = v8::String::NewFromUtf8(
      env->isolate, source_string.c_str(), v8::NewStringType::kNormal);

  if (source_v8_string.IsEmpty())
    return 0;

  v8::TryCatch try_catch(env->isolate);
  MaybeLocal<v8::Script> script =
      v8::Script::Compile(context, source_v8_string.ToLocalChecked());

  if (script.IsEmpty())
    return 0;

  auto local_script = script.ToLocalChecked();
  env->mtx.lock();
  env->start_time = steady_clock::now();
  env->mtx.unlock();

  ALLOW_UNUSED_LOCAL(local_script->Run(context));

  lock_guard<mutex> mtx_locker(env->mtx);
  env->is_running = false;
  return 0;
}
