// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/landlock_gpu_policy_android.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/files/scoped_file.h"
#include "base/threading/thread_id_name_manager.h"
#include "build/build_config.h"
#include "sandbox/linux/services/thread_helpers.h"

#if BUILDFLAG(IS_ANDROID)
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace sandbox::landlock {

bool AddRulesToPolicy(int ruleset_fd,
                      const std::vector<std::string>& paths,
                      uint64_t allowed_access) {
  bool success = true;
  for (const auto& path : paths) {
    base::ScopedFD parent_fd(open(path.c_str(), O_PATH | O_CLOEXEC));
    if (!parent_fd.is_valid()) {
      PLOG(ERROR) << "open failed for " << path;
      continue;
    }
    struct landlock_path_beneath_attr path_beneath = {
        .allowed_access = allowed_access,
        .parent_fd = static_cast<uint32_t>(parent_fd.get()),
    };

    if (landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath,
                          0)) {
      PLOG(ERROR) << "landlock_add_rule() failed for " << path;
      success = false;
    }
  }
  return success;
}
#endif

bool ApplyLandlock(sandbox::mojom::Sandbox sandbox_type) {
#if BUILDFLAG(IS_ANDROID)
  if (sandbox_type != sandbox::mojom::Sandbox::kGpu) {
    LOG(ERROR) << "Sandbox type not GPU, skipping Landlock";
    return false;
  }

  if (!sandbox::ThreadHelpers::IsSingleThreaded()) {
    LOG(ERROR) << "Not single threaded, skipping Landlock";
    for (const auto& id : base::ThreadIdNameManager::GetInstance()->GetIds()) {
      LOG(ERROR) << "ThreadId=" << id << " name:"
                 << base::ThreadIdNameManager::GetInstance()->GetName(id);
    }
    return false;
  }

  struct landlock_ruleset_attr ruleset_attr = {
      .handled_access_fs = LANDLOCK_HANDLED_ACCESS_TYPES};
  base::ScopedFD ruleset_fd(
      landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0));
  if (!ruleset_fd.is_valid()) {
    PLOG(ERROR) << "Ruleset creation failed";
    return false;
  }

  std::vector<std::string> allowed_ro_paths = {
      // RO access to /data/app because there may be sub-directories that don't
      // exist yet at policy creation.
      "/data/app",
      // Allow read-only access to /proc/self. This is needed for the process
      // to introspect its own state.
      "/proc/self", "/sys", "/var"};
  uint64_t ro_access =
      LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR;
  if (!AddRulesToPolicy(ruleset_fd.get(), allowed_ro_paths, ro_access)) {
    LOG(ERROR) << "Adding Landlock RO rules failed";
  }

  std::vector<std::string> allowed_rw_paths = {
      "/data/cache/com.android.chrome",
      // We need to allowlist /dev, because ashmem creates dynamically named
      // directories, such as /dev/ashmemc4072f6e-da0f-447e-98d4-b6497e5f57af.
      // TODO(crbug.com/40215931): Move away from ashmem and remove this.
      "/dev",
  };
  uint64_t rw_access =
      LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
      LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_FILE;
  if (!AddRulesToPolicy(ruleset_fd.get(), allowed_rw_paths, rw_access)) {
    LOG(ERROR) << "Adding Landlock RW rules failed";
  }
  // Landlock requires no_new_privs.
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
    PLOG(ERROR) << "SET_NO_NEW_PRIVS failed";
    return false;
  }
  int result = landlock_restrict_self(ruleset_fd.get(), 0);
  if (result != 0) {
    PLOG(ERROR) << "landlock_restrict_self() failed";
    return false;
  }

  return true;
#else  // !BUILDFLAG(IS_ANDROID)
  // Landlock is not applicable on non-Android platforms.
  return false;
#endif
}

}  // namespace sandbox::landlock
