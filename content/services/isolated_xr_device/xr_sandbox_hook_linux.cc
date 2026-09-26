// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/isolated_xr_device/xr_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace vr {

namespace {

// Returns the env var |name|, or an empty string if unset/empty.
std::string GetEnv(const char* name) {
  const char* value = getenv(name);
  return (value && value[0]) ? std::string(value) : std::string();
}

// The shared libraries the OpenXR and Vulkan loaders dlopen() BY NAME rather
// than via DT_NEEDED. Pre-warmed in the hook and named in the broker
// allow-list below as a fall-back; RTLD_NOW maps in their closure.
constexpr const char* kRuntimeLoadedSonames[] = {
    // The Vulkan loader itself (the OpenXR runtime creates a VkInstance).
    "libvulkan.so.1",
    // Mesa ICDs: the installed GPU drivers (radeon = RADV, freedreno =
    // Qualcomm Adreno, e.g. Steam Frame) and the software rasteriser the
    // loader falls back to. Absent drivers are simply skipped.
    "libvulkan_radeon.so",
    "libvulkan_intel.so",
    "libvulkan_freedreno.so",
    "libvulkan_lvp.so",
    // Mesa implicit layers the Vulkan loader activates by default.
    "libVkLayer_MESA_device_select.so",
    "libVkLayer_MESA_anti_lag.so",
};

// Directories the dynamic loader searches for the sonames above;
// access()-probed at hook time so only paths that exist enter the policy.
constexpr const char* kLibrarySearchDirs[] = {
    "/usr/lib/x86_64-linux-gnu",
    "/lib/x86_64-linux-gnu",
    "/usr/lib64",
    "/usr/lib",
    "/lib",
    "/usr/local/lib",
};

// The XDG config directories the OpenXR loader searches for the
// active-runtime manifest when $XR_RUNTIME_JSON is unset, in loader order.
std::vector<std::string> GetManifestSearchDirs() {
  std::vector<std::string> dirs;
  std::string config_home = GetEnv("XDG_CONFIG_HOME");
  if (config_home.empty()) {
    std::string home = GetEnv("HOME");
    if (!home.empty()) {
      config_home = home + "/.config";
    }
  }
  if (!config_home.empty()) {
    dirs.push_back(config_home);
  }
  std::string config_dirs = GetEnv("XDG_CONFIG_DIRS");
  if (config_dirs.empty()) {
    config_dirs = "/etc/xdg";
  }
  for (const auto& dir :
       base::SplitString(config_dirs, ":", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    dirs.push_back(dir);
  }
  dirs.push_back("/etc");
  return dirs;
}

// Returns the active-runtime manifest path: $XR_RUNTIME_JSON if set, else the
// first <config dir>/openxr/1/active_runtime[.<arch>].json that exists --
// commonly a symlink under /etc installed by the runtime's package.
std::string FindActiveRuntimeManifest() {
  std::string runtime_json = GetEnv("XR_RUNTIME_JSON");
  if (!runtime_json.empty()) {
    return runtime_json;
  }
  // Newer loaders prefer an architecture-specific manifest name.
#if defined(ARCH_CPU_X86_64)
  constexpr const char* kManifestNames[] = {"active_runtime.x86_64.json",
                                            "active_runtime.json"};
#elif defined(ARCH_CPU_ARM64)
  constexpr const char* kManifestNames[] = {"active_runtime.aarch64.json",
                                            "active_runtime.json"};
#else
  constexpr const char* kManifestNames[] = {"active_runtime.json"};
#endif
  for (const std::string& dir : GetManifestSearchDirs()) {
    for (const char* name : kManifestNames) {
      std::string path = dir + "/openxr/1/" + name;
      if (access(path.c_str(), R_OK) == 0) {
        return path;
      }
    }
  }
  return std::string();
}

// Resolves the manifest through a possible symlink (e.g.
// /etc/openxr/1/active_runtime.json -> the packaged manifest) so relative
// paths anchor at the real file, matching the OpenXR loader.
base::FilePath CanonicalizeManifestPath(const std::string& manifest_path) {
  const base::FilePath original_path(manifest_path);
  return base::ReadSymbolicLinkAbsolute(original_path).value_or(original_path);
}

// Reads and parses the active-runtime manifest at |manifest_path|.
std::optional<XrRuntimeManifest> ReadRuntimeManifest(
    const std::string& manifest_path) {
  if (manifest_path.empty()) {
    return std::nullopt;
  }
  std::string contents;
  if (!base::ReadFileToString(base::FilePath(manifest_path), &contents)) {
    return std::nullopt;
  }
  return ParseXrRuntimeManifest(contents);
}

// Returns the absolute path of the runtime library named by the active-runtime
// manifest, or an empty string. A relative |library_path| is resolved against
// the canonical manifest's directory, as the loader does.
std::string GetRuntimeLibraryPath(const std::string& manifest_path,
                                  const std::string& library_path) {
  base::FilePath path(library_path);
  if (!path.IsAbsolute()) {
    path = CanonicalizeManifestPath(manifest_path).DirName().Append(path);
  }
  // Manifests routinely name the library relatively ("./libfoo.so"), which
  // leaves "." or ".." components in the joined path. The broker rejects those
  // outright, so resolve them here rather than handing it a path it will
  // refuse.
  base::FilePath absolute = base::MakeAbsoluteFilePath(path);
  return absolute.empty() ? std::string() : absolute.value();
}

// Files and directories the OpenXR/Vulkan loaders and the runtime open()/stat()
// from this process; read-only except runtime log and shader-cache dirs.
// Frames travel over the IPC socket and DMA-BUF fds, not brokered files.
// Returns |path| with "." and ".." components resolved, or an empty path if it
// is not absolute. The broker CHECK-fails on such components, and these paths
// come from the environment, so normalize instead of trusting them. Lexical so
// it also works for directories that do not exist yet (e.g. shader caches).
base::FilePath NormalizePath(const std::string& path) {
  base::FilePath input(path);
  if (path.empty() || !input.IsAbsolute()) {
    return base::FilePath();
  }
  std::vector<base::FilePath::StringType> out;
  for (const auto& component : input.GetComponents()) {
    if (component == base::FilePath::kCurrentDirectory) {
      continue;
    }
    if (component == base::FilePath::kParentDirectory) {
      if (out.size() > 1) {
        out.pop_back();
      }
      continue;
    }
    out.push_back(component);
  }
  base::FilePath result(FILE_PATH_LITERAL("/"));
  for (size_t i = 1; i < out.size(); i++) {
    result = result.Append(out[i]);
  }
  return result;
}

// The AF_UNIX sockets the OpenXR runtimes connect() to, and the names they
// bind() their own endpoints under. Both syscalls are brokered
// (COMMAND_CONNECT/COMMAND_BIND), so the XR process can only reach these and
// no other socket on the system, and can only create endpoints under the
// listed names. The endpoints are runtime-specific and not covered by the
// OpenXR spec, so they are listed per runtime; an abstract-namespace name is
// spelled with a leading '@'.
void AddRuntimeSocketPermissions(
    std::vector<BrokerFilePermission>& permissions) {
  std::string runtime_dir = GetEnv("XDG_RUNTIME_DIR");
  if (runtime_dir.empty()) {
    runtime_dir = GetEnv("XDG_CACHE_HOME");
  }
  if (!runtime_dir.empty()) {
    // Monado: its client library connects to this compositor IPC socket
    // (XRT_IPC_MSG_SOCK_FILENAME).
    base::FilePath socket = NormalizePath(
        base::FilePath(runtime_dir).AppendASCII("monado_comp_ipc").value());
    if (!socket.empty()) {
      permissions.push_back(BrokerFilePermission::ConnectOnly(socket.value()));
    }
  }

  // SteamVR mints per-instance names under this prefix (SteamVR_Namespace,
  // VR_ServerPipe_<pid>, VR_CompositorPipe_<pid>), so only the prefix can be
  // listed. Every name under it is bound by SteamVR's own processes, and the
  // abstract namespace has no access control, so any local process can already
  // reach them; the grant adds nothing beyond what SteamVR itself exposes.
  permissions.push_back(
      BrokerFilePermission::ConnectOnlyRecursive("@/steamvr/"));

  // SteamVR's client library also bind()s its own per-instance endpoints
  // ("fd-cl-<n>" in the abstract namespace), over which the compositor sends
  // the shared Vulkan texture fds back with SCM_RIGHTS.
  permissions.push_back(BrokerFilePermission::BindOnlyRecursive("@fd-cl-"));
}

std::vector<BrokerFilePermission> GetOpenXrFilePermissions(
    const std::string& manifest_path,
    const std::string& runtime_library_path,
    XrRuntimeId runtime_id) {
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/etc/ld.so.cache"),
  };

  auto add_dir = [&permissions](const std::string& dir) {
    base::FilePath path = NormalizePath(dir);
    if (path.empty()) {
      return;
    }
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        path.AsEndingWithSeparator().value()));
  };
  auto add_dir_rw = [&permissions](const std::string& dir) {
    base::FilePath path = NormalizePath(dir);
    if (path.empty()) {
      return;
    }
    permissions.push_back(BrokerFilePermission::ReadWriteCreateRecursive(
        path.AsEndingWithSeparator().value()));
  };
  auto add_file = [&permissions](const std::string& file) {
    base::FilePath path = NormalizePath(file);
    if (path.empty()) {
      return;
    }
    permissions.push_back(BrokerFilePermission::ReadOnly(path.value()));
  };
  // The loader canonicalizes paths with realpath(), which readlink()s every
  // component and gives up on any error other than EINVAL. readlink() is
  // brokered and needs read access to the exact path, so grant each ancestor
  // directory of |file|: readlink() on a directory only reports EINVAL, and
  // the grant otherwise just lets the directory be opened for listing.
  auto add_ancestor_dirs = [&permissions](const std::string& file) {
    base::FilePath path = NormalizePath(file);
    if (path.empty()) {
      return;
    }
    for (base::FilePath dir = path.DirName(); dir.value() != "/";
         dir = dir.DirName()) {
      permissions.push_back(BrokerFilePermission::ReadOnly(dir.value()));
    }
  };

  // The loader resolves the active-runtime manifest (possibly through a
  // symlink) before dlopen()ing the runtime it names; grant the manifest, its
  // canonical location, and the runtime .so's directory (helper libraries).
  // realpath() also walks the ancestors of each of these.
  if (!manifest_path.empty()) {
    add_file(manifest_path);
    add_ancestor_dirs(manifest_path);
    base::FilePath canonical = CanonicalizeManifestPath(manifest_path);
    add_file(canonical.value());
    add_dir(canonical.DirName().value());
    add_ancestor_dirs(base::MakeAbsoluteFilePath(canonical).value());
  }
  if (!runtime_library_path.empty()) {
    add_dir(base::FilePath(runtime_library_path).DirName().value());
    add_ancestor_dirs(runtime_library_path);
  }

  std::string home = GetEnv("HOME");
  std::string config_home = GetEnv("XDG_CONFIG_HOME");
  if (config_home.empty() && !home.empty()) {
    config_home = home + "/.config";
  }
  for (const std::string& dir : GetManifestSearchDirs()) {
    add_dir(dir + "/openxr");
  }
  add_dir("/usr/share/openxr");
  add_dir("/usr/local/share/openxr");

  // SteamVR only: openvrpaths.vrpath locates the SteamVR install, and the
  // runtime writes logs under ~/.config/openvr/logs (PC) or the Steam
  // directory (Steam Frame).
  if (runtime_id == XrRuntimeId::kSteamVr) {
    if (!config_home.empty()) {
      add_dir(config_home + "/openvr");
      add_dir_rw(config_home + "/openvr/logs");
    }
    if (!home.empty()) {
      add_dir_rw(home + "/.local/share/Steam/logs");
    }
  }

  // Mesa on-disk shader caches; the driver creates and rewrites entries.
  std::string cache_home = GetEnv("XDG_CACHE_HOME");
  if (cache_home.empty() && !home.empty()) {
    cache_home = home + "/.cache";
  }
  if (!cache_home.empty()) {
    add_dir_rw(cache_home + "/mesa_shader_cache");
    add_dir_rw(cache_home + "/mesa_shader_cache_db");
    add_dir_rw(cache_home + "/radv_builtin_shaders");
  }

  // POSIX shared memory: like the GPU process, only objects this process
  // creates itself (O_CREAT | O_EXCL), so pre-existing ones belonging to other
  // processes cannot be opened. SteamVR's IPC cannot work that way: vrserver
  // creates shared objects with per-instance names (u<uid>-Shm_<id>) outside
  // the sandbox that vrclient opens O_RDWR by name, and its named IPC object
  // (u<uid>-ValveIPCSharedObj-SteamVR) is opened O_RDWR | O_CREAT by whichever
  // side comes first, so that runtime alone gets read/write/create access.
  if (runtime_id == XrRuntimeId::kSteamVr) {
    add_dir_rw("/dev/shm");
  } else {
    permissions.push_back(
        BrokerFilePermission::ReadWriteCreateTemporaryRecursive("/dev/shm/"));
  }

  // PCI ID database consulted by Mesa while enumerating devices; the second
  // location is used by AMD's packaged driver stacks.
  add_dir("/usr/share/libdrm");
  add_dir("/opt/amdgpu/share/libdrm");

  // PrewarmGraphicsLibraries() has already made the whole closure resident, so
  // this is only a fall-back for a skipped pre-warm: name the individual
  // libraries rather than granting recursive read over the library tree.
  for (const char* soname : kRuntimeLoadedSonames) {
    for (const char* dir : kLibrarySearchDirs) {
      std::string path = base::StringPrintf("%s/%s", dir, soname);
      if (access(path.c_str(), R_OK) == 0) {
        permissions.push_back(BrokerFilePermission::ReadOnly(path));
      }
    }
  }

  // Vulkan ICD and implicit-layer manifests describing the installed driver.
  add_dir("/usr/share/vulkan");
  add_dir("/etc/vulkan");

  // The runtime creates its own Vulkan instance here, so it needs the same GPU
  // access the GPU process has: the DRM nodes (read-write) plus the sysfs
  // entries the driver reads while enumerating physical devices.
  permissions.push_back(BrokerFilePermission::ReadOnly("/dev/dri"));
  for (int i = 0; i < 8; ++i) {
    permissions.push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("/dev/dri/renderD%d", 128 + i)));
    permissions.push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("/dev/dri/card%d", i)));
  }
  add_dir("/sys/dev/char");
  add_dir("/sys/class/drm");
  add_dir("/sys/devices");
  add_dir("/sys/bus/pci/devices");

  AddRuntimeSocketPermissions(permissions);

  return permissions;
}

// dlopen()s the runtime and driver libraries with RTLD_NOW so their DT_NEEDED
// closure maps in while the filesystem is reachable. MUST run AFTER
// StartBrokerProcess(), which forks and needs a single-threaded process.
void PrewarmGraphicsLibraries(const std::string& runtime_library_path) {
  constexpr int kFlags = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;
  for (const char* soname : kRuntimeLoadedSonames) {
    // Best-effort: a driver for another vendor simply isn't present.
    dlopen(soname, kFlags);
  }
  // Loading the runtime library maps in its whole closure (libvulkan, libGLX,
  // libX11, ...) so the OpenXR loader reuses the resident copy post-seal.
  if (!runtime_library_path.empty()) {
    dlopen(runtime_library_path.c_str(), kFlags);
  }
}

}  // namespace

std::optional<XrRuntimeManifest> ParseXrRuntimeManifest(std::string_view json) {
  std::optional<base::DictValue> root =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!root) {
    return std::nullopt;
  }
  const base::DictValue* runtime = root->FindDict("runtime");
  if (!runtime) {
    return std::nullopt;
  }
  const std::string* library_path = runtime->FindString("library_path");
  if (!library_path || library_path->empty()) {
    return std::nullopt;
  }
  XrRuntimeManifest manifest;
  manifest.library_path = *library_path;
  manifest.runtime_id =
      runtime->FindBool("VALVE_runtime_is_steamvr").value_or(false)
          ? XrRuntimeId::kSteamVr
          : XrRuntimeId::kOther;
  return manifest;
}

bool XrPreSandboxHook(sandbox::policy::SandboxLinux::Options options) {
  // Resolve the manifest ($XR_RUNTIME_JSON or the XDG/etc search) and the
  // runtime library it names before the broker forks (plain file reads, no
  // threads); the library is dlopen()ed afterwards.
  const std::string manifest_path = FindActiveRuntimeManifest();
  const std::optional<XrRuntimeManifest> manifest =
      ReadRuntimeManifest(manifest_path);
  const std::string runtime_library_path =
      manifest ? GetRuntimeLibraryPath(manifest_path, manifest->library_path)
               : std::string();

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(
      MakeBrokerCommandSet({
          sandbox::syscall_broker::COMMAND_ACCESS,
          sandbox::syscall_broker::COMMAND_BIND,
          sandbox::syscall_broker::COMMAND_CONNECT,
          sandbox::syscall_broker::COMMAND_OPEN,
          sandbox::syscall_broker::COMMAND_READLINK,
          sandbox::syscall_broker::COMMAND_STAT,
      }),
      GetOpenXrFilePermissions(
          manifest_path, runtime_library_path,
          manifest ? manifest->runtime_id : XrRuntimeId::kOther),
      options);

  // Pre-warm the runtime + graphics driver closure now that the broker has
  // forked, so the recursive system-library grants are unnecessary.
  PrewarmGraphicsLibraries(runtime_library_path);

  // Intentionally no EngageNamespaceSandboxIfPossible(): its network namespace
  // breaks the runtime's connect() to the compositor's host AF_UNIX socket.
  // The process is still confined by XrProcessPolicy and the file broker.

  return true;
}

}  // namespace vr
