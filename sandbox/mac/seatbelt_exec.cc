// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/mac/seatbelt_exec.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <limits>
#include <vector>

#include "base/posix/eintr_wrapper.h"  //nogncheck
#include "sandbox/mac/sandbox_logging.h"
#include "sandbox/mac/seatbelt.h"

namespace sandbox {

namespace {

struct ReadTraits {
  using BufferType = uint8_t*;
  static constexpr char kNameString[] = "read";
  static ssize_t Operate(int fd, BufferType buffer, size_t size) {
    return read(fd, buffer, size);
  }
};
constexpr char ReadTraits::kNameString[];

struct WriteTraits {
  using BufferType = const uint8_t*;
  static constexpr char kNameString[] = "write";
  static ssize_t Operate(int fd, BufferType buffer, size_t size) {
    return write(fd, buffer, size);
  }
};
constexpr char WriteTraits::kNameString[];

template <typename Traits>
bool ReadOrWrite(int fd,
                 const typename Traits::BufferType buffer,
                 const size_t size,
                 const char* operation_description) {
  if (size > std::numeric_limits<ssize_t>::max()) {
    logging::Error("request size is greater than ssize_t::max");
    return false;
  }

  ssize_t bytes_to_transact = static_cast<ssize_t>(size);

  while (bytes_to_transact > 0) {
    ssize_t offset = size - bytes_to_transact;
    ssize_t transacted_bytes =
        HANDLE_EINTR(Traits::Operate(fd, buffer + offset, bytes_to_transact));
    if (transacted_bytes < 0) {
      logging::PError("SeatbeltExec: %s %s failed", operation_description,
                      Traits::kNameString);
      return false;
    } else if (transacted_bytes == 0) {
      // A short read from the sender, perhaps the sender process died.
      logging::Error("SeatbeltExec: %s %s failed", operation_description,
                     Traits::kNameString);
      return false;
    }

    bytes_to_transact -= transacted_bytes;
  }

  return true;
}

}  // namespace

namespace switches {

const char kSeatbeltClient[] = "--seatbelt-client=";

const char kSeatbeltClientName[] = "seatbelt-client";

}  // namespace switches

SeatbeltExecClient::SeatbeltExecClient() {
  if (pipe(pipe_) != 0)
    logging::PFatal("SeatbeltExecClient: pipe failed");
}

SeatbeltExecClient::~SeatbeltExecClient() {
  if (pipe_[0] != -1)
    IGNORE_EINTR(close(pipe_[0]));
  if (pipe_[1] != -1)
    IGNORE_EINTR(close(pipe_[1]));
}

int SeatbeltExecClient::GetReadFD() {
  return pipe_[0];
}

bool SeatbeltExecClient::SendPolicy(const mac::SandboxPolicy& policy) {
  IGNORE_EINTR(close(pipe_[0]));
  pipe_[0] = -1;

  std::string serialized_protobuf;
  if (!policy.SerializeToString(&serialized_protobuf)) {
    logging::Error("SeatbeltExecClient: Serializing the profile failed.");
    return false;
  }

  if (!WriteString(serialized_protobuf)) {
    return false;
  }

  IGNORE_EINTR(close(pipe_[1]));
  pipe_[1] = -1;

  return true;
}

bool SeatbeltExecClient::WriteString(const std::string& str) {
  uint64_t str_len = static_cast<uint64_t>(str.size());
  if (!ReadOrWrite<WriteTraits>(pipe_[1], reinterpret_cast<uint8_t*>(&str_len),
                                sizeof(str_len), "buffer length")) {
    return false;
  }

  if (!ReadOrWrite<WriteTraits>(pipe_[1],
                                reinterpret_cast<const uint8_t*>(&str[0]),
                                str_len, "buffer")) {
    return false;
  }

  return true;
}

SeatbeltExecServer::SeatbeltExecServer(int fd) : fd_(fd) {}

SeatbeltExecServer::~SeatbeltExecServer() {
  close(fd_);
}

sandbox::SeatbeltExecServer::CreateFromArgumentsResult::
    CreateFromArgumentsResult() = default;
sandbox::SeatbeltExecServer::CreateFromArgumentsResult::
    CreateFromArgumentsResult(CreateFromArgumentsResult&&) = default;
sandbox::SeatbeltExecServer::CreateFromArgumentsResult::
    ~CreateFromArgumentsResult() = default;

// static
sandbox::SeatbeltExecServer::CreateFromArgumentsResult
SeatbeltExecServer::CreateFromArguments(const char* executable_path,
                                        int argc,
                                        const char* const* argv) {
  CreateFromArgumentsResult result;
  int seatbelt_client_fd = -1;
  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], switches::kSeatbeltClient,
                strlen(switches::kSeatbeltClient)) == 0) {
      result.sandbox_required = true;
      std::string arg(argv[i]);
      std::string fd_string = arg.substr(strlen(switches::kSeatbeltClient));
      seatbelt_client_fd = std::stoi(fd_string);
    }
  }

  if (!result.sandbox_required)
    return result;

  if (seatbelt_client_fd < 0) {
    logging::Error("Must pass a valid file descriptor to %s",
                   switches::kSeatbeltClient);
    return result;
  }

  result.server.reset(new SeatbeltExecServer(seatbelt_client_fd));
  return result;
}

bool SeatbeltExecServer::InitializeSandbox() {
  std::string policy_string;
  if (!ReadString(&policy_string))
    return false;

  mac::SandboxPolicy policy;
  if (!policy.ParseFromString(policy_string)) {
    logging::Error("SeatbeltExecServer: ParseFromString failed");
    return false;
  }

  return ApplySandboxProfile(policy);
}

bool SeatbeltExecServer::ApplySandboxProfile(const mac::SandboxPolicy& policy) {
  std::string error;
  bool ok = false;

  if (policy.has_compiled()) {
    ok = Seatbelt::ApplyCompiledProfile(policy.compiled().data(), &error);
  } else {
    const mac::SourcePolicy& source_policy = policy.source();

    std::vector<const char*> weak_params;
    for (const auto& pair : source_policy.params()) {
      weak_params.push_back(pair.first.c_str());
      weak_params.push_back(pair.second.c_str());
    }
    weak_params.push_back(nullptr);

    ok = Seatbelt::InitWithParams(source_policy.profile().c_str(), 0,
                                  weak_params.data(), &error);
  }
  if (!ok) {
    logging::Error("SeatbeltExecServer: Failed to initialize sandbox: %s",
                   error.c_str());
  }
  return ok;
}

bool SeatbeltExecServer::ReadString(std::string* str) {
  uint64_t buf_len = 0;
  if (!ReadOrWrite<ReadTraits>(fd_, reinterpret_cast<uint8_t*>(&buf_len),
                               sizeof(buf_len), "buffer length")) {
    return false;
  }

  str->resize(buf_len);

  if (!ReadOrWrite<ReadTraits>(fd_, reinterpret_cast<uint8_t*>(&(*str)[0]),
                               buf_len, "buffer")) {
    return false;
  }

  return true;
}

}  // namespace sandbox
