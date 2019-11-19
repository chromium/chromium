// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt_exec.h"

#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <vector>

#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
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
                 const size_t size) {
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
      logging::PError("%s failed", Traits::kNameString);
      return false;
    } else if (transacted_bytes == 0) {
      // A short read from the sender, perhaps the sender process died.
      logging::Error("%s failed", Traits::kNameString);
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

bool SeatbeltExecClient::SetBooleanParameter(const std::string& key,
                                             bool value) {
  google::protobuf::MapPair<std::string, std::string> pair(
      key, value ? "TRUE" : "FALSE");
  return policy_.mutable_params()->insert(pair).second;
}

bool SeatbeltExecClient::SetParameter(const std::string& key,
                                      const std::string& value) {
  google::protobuf::MapPair<std::string, std::string> pair(key, value);
  return policy_.mutable_params()->insert(pair).second;
}

void SeatbeltExecClient::SetProfile(const std::string& policy) {
  policy_.set_profile(policy);
}

int SeatbeltExecClient::GetReadFD() {
  return pipe_[0];
}

bool SeatbeltExecClient::SendProfile() {
  IGNORE_EINTR(close(pipe_[0]));
  pipe_[0] = -1;

  std::string serialized_protobuf;
  if (!policy_.SerializeToString(&serialized_protobuf)) {
    logging::Error("SeatbeltExecClient: Serializing the profile failed.");
    return false;
  }

  if (!WriteString(serialized_protobuf)) {
    logging::Error(
        "SeatbeltExecClient: Writing the serialized profile failed.");
    return false;
  }

  IGNORE_EINTR(close(pipe_[1]));
  pipe_[1] = -1;

  return true;
}

bool SeatbeltExecClient::WriteString(const std::string& str) {
  uint64_t str_len = static_cast<uint64_t>(str.size());
  if (!ReadOrWrite<WriteTraits>(pipe_[1], reinterpret_cast<uint8_t*>(&str_len),
                                sizeof(str_len))) {
    logging::Error("SeatbeltExecClient: write buffer length failed.");
    return false;
  }

  if (!ReadOrWrite<WriteTraits>(
          pipe_[1], reinterpret_cast<const uint8_t*>(&str[0]), str_len)) {
    logging::Error("SeatbeltExecClient: write buffer failed.");
    return false;
  }

  return true;
}

SeatbeltExecServer::SeatbeltExecServer(int fd) : fd_(fd), extra_params_() {}

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
                                        char** argv) {
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

  char full_exec_path[MAXPATHLEN];
  if (realpath(executable_path, full_exec_path) == NULL) {
    logging::PError("realpath");
    return result;
  }

  auto server = std::make_unique<SeatbeltExecServer>(seatbelt_client_fd);
  // These parameters are provided for every profile to use.
  if (!server->SetParameter("EXECUTABLE_PATH", full_exec_path) ||
      !server->SetParameter("CURRENT_PID", std::to_string(getpid()))) {
    logging::Error("Failed to set up parameters for sandbox.");
    return result;
  }

  result.server = std::move(server);
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
  std::vector<const char*> weak_params;
  for (const auto& pair : policy.params()) {
    weak_params.push_back(pair.first.c_str());
    weak_params.push_back(pair.second.c_str());
  }
  for (const auto& pair : extra_params_) {
    weak_params.push_back(pair.first.c_str());
    weak_params.push_back(pair.second.c_str());
  }
  weak_params.push_back(nullptr);

  char* error = nullptr;
  int rv = Seatbelt::InitWithParams(policy.profile().c_str(), 0,
                                    weak_params.data(), &error);
  if (error) {
    logging::Error("SeatbeltExecServer: Failed to initialize sandbox: %d %s",
                   rv, error);
    Seatbelt::FreeError(error);
    return false;
  }

  return rv == 0;
}

bool SeatbeltExecServer::ReadString(std::string* str) {
  uint64_t buf_len = 0;
  if (!ReadOrWrite<ReadTraits>(fd_, reinterpret_cast<uint8_t*>(&buf_len),
                               sizeof(buf_len))) {
    logging::Error("SeatbeltExecServer: failed to read buffer length.");
    return false;
  }

  str->resize(buf_len);

  if (!ReadOrWrite<ReadTraits>(fd_, reinterpret_cast<uint8_t*>(&(*str)[0]),
                               buf_len)) {
    logging::Error("SeatbeltExecServer: failed to read buffer.");
    return false;
  }

  return true;
}

bool SeatbeltExecServer::SetParameter(const std::string& key,
                                      const std::string& value) {
  return extra_params_.insert(std::make_pair(key, value)).second;
}

}  // namespace sandbox
