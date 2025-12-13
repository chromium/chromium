// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt.h"

#include <errno.h>
#include <unistd.h>

#include <memory>
#include <utility>

extern "C" {
#include <sandbox.h>

int sandbox_init_with_parameters(const char* profile,
                                 uint64_t flags,
                                 const char* const parameters[],
                                 char** errorbuf);

// Not deprecated. The canonical usage to test if sandboxed is
// sandbox_check(getpid(), NULL, SANDBOX_FILTER_NONE), which returns
// 1 if sandboxed. Note `type` is actually a sandbox_filter_type enum value, but
// it is unused currently.
int sandbox_check(pid_t pid, const char* operation, int type, ...);

// A deleter for std::unique_ptr that frees memory allocated by the sandbox
// library's error functions.
struct sandbox_error_deleter {
  inline void operator()(char* ptr) const {
    if (ptr) {
// This function is marked in the SDK as unavailable after macOS 10.8, but there
// is no replacement, so ignore the deprecation warning.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      ::sandbox_free_error(ptr);
#pragma clang diagnostic pop
    }
  }
};

struct sandbox_params_t {
  const char** params;
  size_t size;
  size_t available;
};
sandbox_params_t* sandbox_create_params();
int sandbox_set_param(sandbox_params_t*, const char* key, const char* value);
void sandbox_free_params(sandbox_params_t*);

struct sandbox_profile_t {
  char* builtin;
  const uint8_t* data;
  size_t size;
};
sandbox_profile_t* sandbox_compile_string(const char* data,
                                          sandbox_params_t*,
                                          char** error);
int sandbox_apply(sandbox_profile_t*);
void sandbox_free_profile(sandbox_profile_t*);

}  // extern "C"

namespace sandbox {

namespace {

// `managed_errorbuf` uses a unique_ptr with a deleter to ensure that memory is
// freed using the sandbox library's deallocation function to prevent unexpected
// behavior.
//
// `managed_errorbuf`'s destructor will automatically call
// `sandbox_error_deleter` in all return paths.
bool HandleSandboxResult(
    int rv,
    std::unique_ptr<char, sandbox_error_deleter> managed_errorbuf,
    std::string* error) {
  if (rv == 0) {
    if (error) {
      error->clear();
    }
    return true;
  }

  char* errorbuf = managed_errorbuf.get();

  // Ensure that we never attempt to assign a nullptr to the `error` string
  // (crbug.com/40066531).
  if (error && errorbuf) {
    *error = errorbuf;
  }
  return false;
}

bool HandleSandboxErrno(int rv, const char* message, std::string* error) {
  if (rv == 0) {
    if (error) {
      error->clear();
    }
    return true;
  }

  if (error) {
    char* perror = strerror(errno);
    error->assign(message);
    error->append(perror);
  }
  return false;
}

}  // namespace

// static
Seatbelt::Parameters Seatbelt::Parameters::Create() {
  Parameters params;
  params.params_ = ::sandbox_create_params();
  return params;
}

Seatbelt::Parameters::Parameters() = default;

Seatbelt::Parameters::Parameters(Seatbelt::Parameters&& other) {
  params_ = std::exchange(other.params_, nullptr);
}

Seatbelt::Parameters& Seatbelt::Parameters::operator=(
    Seatbelt::Parameters&& other) {
  params_ = std::exchange(other.params_, nullptr);
  return *this;
}

bool Seatbelt::Parameters::Set(const char* key, const char* value) {
  return ::sandbox_set_param(params_, key, value) == 0;
}

Seatbelt::Parameters::~Parameters() {
  if (params_) {
    ::sandbox_free_params(params_);
  }
}

// Initialize the static member variables.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
const char* Seatbelt::kProfilePureComputation = kSBXProfilePureComputation;
#pragma clang diagnostic pop

// static
bool Seatbelt::Init(const char* profile, uint64_t flags, std::string* error) {
// OS X deprecated these functions, but did not provide a suitable replacement,
// so ignore the deprecation warning.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  char* errorbuf = nullptr;
  int rv = ::sandbox_init(profile, flags, &errorbuf);
  return HandleSandboxResult(
      rv, std::unique_ptr<char, sandbox_error_deleter>(errorbuf), error);
#pragma clang diagnostic pop
}

// static
bool Seatbelt::InitWithParams(const std::string& profile,
                              uint64_t flags,
                              const std::vector<std::string>& parameters,
                              std::string* error) {
  std::vector<const char*> weak_params;
  for (const std::string& param : parameters) {
    weak_params.push_back(param.c_str());
  }
  // The parameters array must be null terminated.
  weak_params.push_back(nullptr);
  char* errorbuf = nullptr;
  int rv = ::sandbox_init_with_parameters(profile.c_str(), flags,
                                          weak_params.data(), &errorbuf);
  return HandleSandboxResult(
      rv, std::unique_ptr<char, sandbox_error_deleter>(errorbuf), error);
}

// static
bool Seatbelt::Compile(const char* profile,
                       const Seatbelt::Parameters& params,
                       std::string& compiled_profile,
                       std::string* error) {
  char* errorbuf = nullptr;
  sandbox_profile_t* sandbox_profile =
      ::sandbox_compile_string(profile, params.params(), &errorbuf);
  if (!HandleSandboxResult(
          sandbox_profile ? 0 : -1,
          std::unique_ptr<char, sandbox_error_deleter>(errorbuf), error)) {
    return false;
  }
  compiled_profile.assign(reinterpret_cast<const char*>(sandbox_profile->data),
                          sandbox_profile->size);
  ::sandbox_free_profile(sandbox_profile);
  return true;
}

// static
bool Seatbelt::ApplyCompiledProfile(const std::string& profile,
                                    std::string* error) {
  sandbox_profile_t sbox_profile = {
      .builtin = nullptr,
      .data = reinterpret_cast<const uint8_t*>(profile.data()),
      .size = profile.size()};
  return HandleSandboxErrno(::sandbox_apply(&sbox_profile),
                            "sandbox_apply: ", error);
}

// static
bool Seatbelt::IsSandboxed() {
  return ::sandbox_check(getpid(), NULL, 0);
}

}  // namespace sandbox
