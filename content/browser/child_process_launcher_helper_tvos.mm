// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"

// TODO(crbug.com/391914246): Implement the ChildProcessLauncherHelper when
// tvOS actually requires the functionality.
namespace content {
namespace internal {

std::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnLauncherThread() {
  NOTREACHED();
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  NOTREACHED();
}

std::unique_ptr<PosixFileDescriptorInfo>
ChildProcessLauncherHelper::GetFilesToMap() {
  NOTREACHED();
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  NOTREACHED();
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<PosixFileDescriptorInfo> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  NOTREACHED();
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  NOTREACHED();
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {
  NOTREACHED();
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  NOTREACHED();
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  NOTREACHED();
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  NOTREACHED();
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    base::Process::Priority priority) {
  NOTREACHED();
}

base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  // Not used yet (until required files are described in the service manifest on
  // tvOS).
  NOTREACHED();
}

}  // namespace internal
}  // namespace content
