// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/sandbox_mac.h"

#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/apple/mach_port_rendezvous.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "remoting/host/base/switches.h"
#include "sandbox/mac/seatbelt.h"

namespace remoting::mac {

namespace {

constexpr uid_t kNobodyUid = static_cast<uid_t>(-2);
constexpr gid_t kNobodyGid = static_cast<gid_t>(-2);

// Resolves symlinks and firmlinks so Seatbelt path rules match XNU canonical
// paths.
base::FilePath GetCanonicalPath(const base::FilePath& path) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(ERROR) << "GetCanonicalPath() failed for: " << path.value();
    return path;
  }

  base::FilePath::CharType canonical_path[MAXPATHLEN];
  if (HANDLE_EINTR(fcntl(fd.get(), F_GETPATH, canonical_path)) != 0) {
    DPLOG(ERROR) << "GetCanonicalPath() failed for: " << path.value();
    return path;
  }

  return base::FilePath(canonical_path);
}

// Baseline Seatbelt rules shared by `network` and `peer_connection` workers.
constexpr char kCommonSeatbeltProfile[] = R"sb(
(version 1)
(deny default)

; Explicit process-isolation lockdown (`deny default` leaves process-info* and
; iokit-get-properties allowed by default).
(deny iokit-get-properties process-info* nvram*)
(allow signal (target self))
(allow process-info-pidinfo (target self))
(allow process-info-setcontrol (target self))

; Component builds load shared libraries from DIR_EXE.
(if (string? (param "COMPONENT_PATH"))
  (allow file-read* (subpath (param "COMPONENT_PATH"))))

(allow file-read* (path (param "EXECUTABLE_PATH")))
(allow file-read* (subpath (param "BUNDLE_PATH")))

; macOS 11/12 firmlink workaround for bundles on the Data volume.
(define (string-prefix? str prefix)
  (let ((l (string-length prefix)))
    (if (< (string-length str) l)
      #f
      (equal? (substring str 0 l) prefix))))
(define data-volume-root "/System/Volumes/Data/")
(when (string-prefix? (param "BUNDLE_PATH") data-volume-root)
  (allow file-read*
    (subpath (substring (param "BUNDLE_PATH")
                        (- (string-length data-volume-root) 1)
                        (string-length (param "BUNDLE_PATH"))))))

; Allow realpath() to resolve parent directory metadata.
(allow file-read-metadata (subpath "/"))

; Defense in depth against reading root host secrets or system keychains even
; if located near the bundle path.
(deny file-read*
  (path "/Library/PrivilegedHelperTools/org.chromium.chromoting.json")
  (subpath "/Library/Keychains"))

; System libraries, frameworks, and Cryptex dyld caches.
(allow file-read*
  (subpath "/System/Library/CoreServices/CoreTypes.bundle")
  (subpath "/System/Library/CoreServices/SystemVersion.bundle")
  (subpath "/System/Library/Frameworks")
  (subpath "/System/Library/Preferences/Logging")
  (subpath "/System/Library/PrivateFrameworks")
  (subpath "/Library/Apple/System/Library/Frameworks")
  (subpath "/Library/Apple/System/Library/PrivateFrameworks")
  (subpath "/usr/lib"))

(allow file-read* file-map-executable
  (subpath "/System/Cryptexes/App")
  (subpath "/System/Cryptexes/OS")
  (subpath "/System/Volumes/Preboot/Cryptexes/App/System")
  (subpath "/System/Volumes/Preboot/Cryptexes/OS"))

(allow file-read* (subpath "/private/var/db/timezone"))
(allow file-read-data
  (path "/private/etc/passwd")
  (path "/dev/null")
  (path "/dev/random")
  (path "/dev/urandom")
  (path "/System/Library/CoreServices/SystemVersion.plist")
  (path "/System/Library/CoreServices/checkfixlist")
  (subpath "/usr/share/zoneinfo.default")
  (subpath "/usr/share/icu")
  (subpath "/usr/share/locale"))

; Logging and NSS/libinfo Mach lookups.
(allow mach-lookup
  (global-name "com.apple.logd")
  (global-name "com.apple.system.logger")
  (global-name "com.apple.system.opendirectoryd.libinfo"))

(allow network-outbound
  (literal "/private/var/run/asl_input")
  (literal "/private/var/run/syslog"))

; Baseline CPU and OS sysctls.
(allow sysctl-read
  (sysctl-name "hw.activecpu")
  (sysctl-name "hw.busfrequency_compat")
  (sysctl-name "hw.byteorder")
  (sysctl-name "hw.cacheconfig")
  (sysctl-name "hw.cachelinesize_compat")
  (sysctl-name "hw.cpufamily")
  (sysctl-name "hw.cpufrequency_compat")
  (sysctl-name "hw.cputype")
  (sysctl-name "hw.l1dcachesize_compat")
  (sysctl-name "hw.l1icachesize_compat")
  (sysctl-name "hw.l2cachesize_compat")
  (sysctl-name "hw.l3cachesize_compat")
  (sysctl-name "hw.logicalcpu_max")
  (sysctl-name "hw.machine")
  (sysctl-name "hw.memsize")
  (sysctl-name "hw.ncpu")
  (sysctl-name "hw.nperflevels")
  (sysctl-name "hw.packages")
  (sysctl-name "hw.pagesize_compat")
  (sysctl-name "hw.physicalcpu_max")
  (sysctl-name "hw.tbfrequency_compat")
  (sysctl-name "hw.vectorunit")
  (sysctl-name "kern.hostname")
  (sysctl-name "kern.hv_vmm_present")
  (sysctl-name "kern.maxfilesperproc")
  (sysctl-name "kern.osproductversion")
  (sysctl-name "kern.osrelease")
  (sysctl-name "kern.ostype")
  (sysctl-name "kern.osvariant_status")
  (sysctl-name "kern.osversion")
  (sysctl-name "kern.secure_kernel")
  (sysctl-name "kern.usrstack64")
  (sysctl-name "kern.version")
  (sysctl-name "sysctl.proc_cputype")
  (sysctl-name-prefix "hw.optional.")
  (sysctl-name-prefix "hw.perflevel"))

; Network/ICE sockets, routing table inspection, and DNS resolution.
(allow sysctl-read
  (sysctl-name-regex #"^net.routetable"))

(allow system-socket
  (require-all (socket-domain AF_SYSTEM)
               (socket-protocol 2)) ; SYSPROTO_CONTROL
  (socket-domain AF_ROUTE))

(allow file-read*
  (path "/")
  (path "/etc")
  (path "/etc/hosts")
  (path "/etc/resolv.conf")
  (path "/private")
  (path "/private/etc")
  (path "/private/etc/hosts")
  (path "/private/etc/resolv.conf")
  (path "/private/var")
  (path "/private/var/run")
  (path "/private/var/run/resolv.conf")
  (path "/var")
  (path "/var/run"))

(allow network-outbound
  (control-name "com.apple.netsrc")
  (literal "/private/var/run/mDNSResponder")
  (remote tcp)
  (remote udp))

(allow network-bind network-inbound
  (local tcp)
  (local udp))
)sb";

// Additional Seatbelt rules required by the trusted `network` control-plane
// worker for HTTPS/TLS certificate trust evaluation and network-change
// monitoring (still keeps /Library/Keychains denied).
constexpr char kNetworkSeatbeltAdditions[] = R"sb(
(allow file-read*
  (path "/Library/Preferences/com.apple.security.plist")
  (subpath "/System/Library/Keychains")
  (subpath "/System/Library/Security")
  (subpath "/private/var/db/mds"))

(allow ipc-posix-shm-read-data
  (ipc-posix-name "apple.shm.notification_center"))

(allow ipc-posix-shm
  (ipc-posix-name "com.apple.AppleDatabaseChanged"))

(allow mach-lookup
  (global-name "com.apple.trustd")
  (global-name "com.apple.trustd.agent")
  (global-name "com.apple.ocspd")
  (global-name "com.apple.SystemConfiguration.DNSConfiguration")
  (global-name "com.apple.SystemConfiguration.configd")
  (global-name "com.apple.system.notification_center")
  (global-name "com.apple.nehelper")
  (global-name "com.apple.networkd"))
)sb";

}  // namespace

void DropRootPrivilegesAndAcquireBootstrapPort() {
  if (getuid() == 0 || geteuid() == 0) {
    PCHECK(setgroups(1, &kNobodyGid) == 0);
    PCHECK(setgid(kNobodyGid) == 0);
    PCHECK(setuid(kNobodyUid) == 0);
    // Verify privileges cannot be reacquired.
    CHECK_EQ(setuid(0), -1);
    CHECK_EQ(seteuid(0), -1);
  }
  CHECK_NE(getuid(), 0u);
  CHECK_NE(geteuid(), 0u);
  CHECK_NE(getgid(), 0u);
  CHECK_NE(getegid(), 0u);

  // Claim the inherited Mach bootstrap rendezvous ports as `nobody` before
  // Seatbelt locks down arbitrary Mach service lookups.
  base::MachPortRendezvousClient::GetInstance();
}

void EnterSeatbeltSandbox(std::string_view process_type) {
  std::string profile = kCommonSeatbeltProfile;
  if (process_type == kProcessTypeNetwork) {
    profile += kNetworkSeatbeltAdditions;
  } else if (process_type != kProcessTypePeerConnection) {
    NOTREACHED() << "Unexpected sandboxed process type: " << process_type;
  }

  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::FILE_EXE, &exe_path));

  std::vector<std::string> params = {
      "EXECUTABLE_PATH",
      GetCanonicalPath(exe_path).value(),
      "BUNDLE_PATH",
      GetCanonicalPath(base::apple::MainBundlePath()).value(),
  };

#if defined(COMPONENT_BUILD)
  base::FilePath exe_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_dir));
  params.push_back("COMPONENT_PATH");
  params.push_back(GetCanonicalPath(exe_dir).value());
#endif

  std::string error;
  CHECK(sandbox::Seatbelt::InitWithParams(profile, /*flags=*/0, params, &error))
      << "Failed to initialize Seatbelt sandbox for " << process_type << ": "
      << error;
}

}  // namespace remoting::mac
