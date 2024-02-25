// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_FORK_DELEGATE_LINUX_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_FORK_DELEGATE_LINUX_H_

#include <unistd.h>

#include <string>
#include <vector>

// TODO(jln) base::TerminationStatus should be forward declared when switching
// to C++11.
#include "base/process/kill.h"

namespace content {

// The ZygoteForkDelegate allows the Chrome Linux zygote to delegate
// fork operations to another class that knows how to do some
// specialized version of fork.
class ZygoteForkDelegate {
 public:
  // A ZygoteForkDelegate is created during Chrome linux zygote
  // initialization, and provides "fork()" functionality as an
  // alternative to forking the zygote.  A new delegate is passed in
  // as an argument to ZygoteMain().
  virtual ~ZygoteForkDelegate() {}

  // Initialization happens in the zygote after it has been
  // started by ZygoteMain.
  // If |enable_layer1_sandbox| is true, the delegate must enable a
  // layer-1 sandbox such as the setuid sandbox.
  virtual void Init(int sandboxdesc, bool enable_layer1_sandbox) = 0;

  // After Init, supply a UMA_HISTOGRAM_ENUMERATION the delegate would like
  // reported to the browser process.  (Note: Because these reports are
  // piggy-backed onto fork responses that don't otherwise contain UMA reports,
  // this method may not be called until much later.)
  virtual void InitialUMA(std::string* uma_name,
                          int* uma_sample,
                          int* uma_boundary_value) = 0;

  // Returns 'true' if the delegate would like to handle a given fork
  // request.  Otherwise returns false.  Optionally, fills in uma_name et al
  // with a report the helper wants to make via UMA_HISTOGRAM_ENUMERATION.
  virtual bool CanHelp(const std::string& process_type,
                       std::string* uma_name,
                       int* uma_sample,
                       int* uma_boundary_value) = 0;

  // Indexes of FDs in the vector passed to Fork().
  enum {
    // Used to pass in the descriptor for talking to the Browser.
    // Because the children use ChannelMojo, this is actually the Mojo fd.
    kBrowserFDIndex,
    // The PID oracle is used in the protocol for discovering the
    // child process's real PID from within the SUID sandbox.
    // The child process is required to write to the socket after
    // successfully forking.
    kPIDOracleFDIndex,
    // A descriptor for a read-only shared memory region that can be mapped and
    // used to initialize a base::FieldTrialList.
    kFieldTrialFDIndex,
    // A descriptor for the read-write shared memory region that is passed from
    // the parent process for use by the child for allocating histograms. This
    // is then accessed by the parent process for metrics reporting.
    kHistogramFDIndex,

    // Number of FDs in the vector passed to Fork().
    kNumPassedFDs
  };

  // Delegate forks, returning a -1 on failure. Outside the
  // suid sandbox, Fork() returns the Linux process ID.
  // This method is not aware of any potential pid namespaces, so it'll
  // return a raw pid just like fork() would.
  // Delegate is responsible for communicating the channel ID to the
  // newly created child process.
  virtual pid_t Fork(const std::string& process_type,
                     const std::vector<std::string>& args,
                     const std::vector<int>& fds,
                     const std::string& channel_id) = 0;

  // The fork delegate must also assume the role of waiting for its children
  // since the caller will not be their parents and cannot do it. |pid| here
  // should be a pid that has been returned by the Fork() method. i.e. This
  // method is completely unaware of eventual PID namespaces due to sandboxing.
  // |known_dead| indicates that the process is already dead and that a
  // blocking wait() should be performed. In this case, GetTerminationStatus()
  // will send a SIGKILL to the target process first.
  virtual bool GetTerminationStatus(pid_t pid,
                                    bool known_dead,
                                    base::TerminationStatus* status,
                                    int* exit_code) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_ZYGOTE_FORK_DELEGATE_LINUX_H_
