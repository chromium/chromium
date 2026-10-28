// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PEER_CONNECTION_BPF_POLICY_LINUX_H_
#define REMOTING_HOST_LINUX_PEER_CONNECTION_BPF_POLICY_LINUX_H_

#include <sys/types.h>

#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/bpf_dsl/policy.h"

namespace remoting {

// A Seccomp-BPF policy for the Peer Connection process on Linux.
// Implements a targeted blocklist that denies process execution, raw sockets,
// process inspection, and filesystem modification while allowing normal WebRTC,
// threading, and IPC syscalls. Since all peer connection processes run as the
// same user, it also denies syscalls that would let one peer connection process
// signal, inspect, or otherwise affect another, and kernel state that is shared
// by all processes of the same user.
class PeerConnectionBpfPolicyLinux : public sandbox::bpf_dsl::Policy {
 public:
  // `policy_pid` is the PID of the process that the policy will be applied to.
  explicit PeerConnectionBpfPolicyLinux(pid_t policy_pid);
  PeerConnectionBpfPolicyLinux(const PeerConnectionBpfPolicyLinux&) = delete;
  PeerConnectionBpfPolicyLinux& operator=(const PeerConnectionBpfPolicyLinux&) =
      delete;
  ~PeerConnectionBpfPolicyLinux() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(int sysno) const override;

 private:
  const pid_t policy_pid_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PEER_CONNECTION_BPF_POLICY_LINUX_H_
