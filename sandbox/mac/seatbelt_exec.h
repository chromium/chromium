// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_EXEC_H_
#define SANDBOX_MAC_SEATBELT_EXEC_H_

#include <memory>
#include <string>

#include "sandbox/mac/seatbelt.pb.h"
#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

namespace switches {

// This switch is set by the process running the SeatbeltExecClient. It
// specifies the FD number from which the SeatbeltExecServer should read the
// sandbox profile and parameters. This is prefixed with "--" and ends with "="
// for easier processing in C.
SEATBELT_EXPORT extern const char kSeatbeltClient[];

// This is the same as kSeatbeltClient without the prefix and suffix.
SEATBELT_EXPORT extern const char kSeatbeltClientName[];

}  // namespace switches

// SeatbeltExecClient is used by the process that is launching another sandboxed
// process. The API allows the launcher process to supply a sandbox profile and
// parameters, which will be communicated to the sandboxed process over IPC.
class SEATBELT_EXPORT SeatbeltExecClient {
 public:
  SeatbeltExecClient();
  ~SeatbeltExecClient();

  // The Set*Parameter functions return true if the parameter was successfully
  // inserted. Check the return value, which indicates if the parameter was
  // added successfully.

  // This returns the FD used for reading the sandbox profile in the child
  // process. The FD should be mapped into the sandboxed child process.
  // This must be called before SendProfile() or the returned FD will be -1.
  // Callers should check that the returned FD is valid.
  int GetReadFD();

  // Sends the policy to the SeatbeltExecServer and returns success or failure.
  bool SendPolicy(const mac::SandboxPolicy& policy);

 private:
  // This writes a string (the serialized protobuf) to the |pipe_|.
  bool WriteString(const std::string& str);

  // A file descriptor pair used for interprocess communication.
  int pipe_[2];
};

// SeatbeltExecServer is used by the process that will be sandboxed to receive
// the profile and parameters from the launcher process. It can then initialize
// the profile, sandboxing the process.
class SEATBELT_EXPORT SeatbeltExecServer {
 public:
  // Creates a server instance with |server_fd| being the pipe returned from
  // SeatbeltExecClient::GetReadFD(). To sandbox the process,
  // InitializeSandbox() must be called.
  explicit SeatbeltExecServer(int sandbox_fd);
  ~SeatbeltExecServer();

  // CreateFromArguments parses the command line arguments for the
  // kSeatbeltClient flag. If no flag is present, then |sandbox_required| is
  // false and |server| is nullptr. If the flag is present, then
  // |sandbox_required| is true. If the SeatbeltExecServer was successfully
  // created then |server| will be the result instance, upon which
  // InitializeSandbox() must be called. If initialization fails, then |server|
  // will be nullptr.
  struct CreateFromArgumentsResult {
    CreateFromArgumentsResult();
    CreateFromArgumentsResult(CreateFromArgumentsResult&&);
    ~CreateFromArgumentsResult();

    bool sandbox_required = false;
    std::unique_ptr<SeatbeltExecServer> server;
  };
  static CreateFromArgumentsResult CreateFromArguments(
      const char* executable_path,
      int argc,
      const char* const* argv);

  // Reads the policy from the client, applies the profile, and returns whether
  // or not the operation succeeds.
  bool InitializeSandbox();

  // Applies the given sandbox policy, and returns whether or not the operation
  // succeeds.
  bool ApplySandboxProfile(const mac::SandboxPolicy& sandbox_policy);

 private:
  // Reads from the |fd_| and stores the data into a string. This does
  // not append a NUL terminator as protobuf does not expect one.
  bool ReadString(std::string* string);

  // The file descriptor used to communicate with the launcher process.
  int fd_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_SEATBELT_EXEC_H_
