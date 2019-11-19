// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/mac/sandbox_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include <CoreFoundation/CFTimeZone.h>
#include <signal.h>
#include <sys/param.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_port_rendezvous.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "services/service_manager/sandbox/mac/audio.sb.h"
#include "services/service_manager/sandbox/mac/cdm.sb.h"
#include "services/service_manager/sandbox/mac/common.sb.h"
#include "services/service_manager/sandbox/mac/gpu.sb.h"
#include "services/service_manager/sandbox/mac/gpu_v2.sb.h"
#include "services/service_manager/sandbox/mac/nacl_loader.sb.h"
#include "services/service_manager/sandbox/mac/network.sb.h"
#include "services/service_manager/sandbox/mac/pdf_compositor.sb.h"
#include "services/service_manager/sandbox/mac/ppapi.sb.h"
#include "services/service_manager/sandbox/mac/renderer.sb.h"
#include "services/service_manager/sandbox/mac/utility.sb.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"

namespace service_manager {

// Static variable declarations.
const char* SandboxMac::kSandboxBrowserPID = "BROWSER_PID";
const char* SandboxMac::kSandboxBundlePath = "BUNDLE_PATH";
const char* SandboxMac::kSandboxChromeBundleId = "BUNDLE_ID";
const char* SandboxMac::kSandboxComponentPath = "COMPONENT_PATH";
const char* SandboxMac::kSandboxDisableDenialLogging =
    "DISABLE_SANDBOX_DENIAL_LOGGING";
const char* SandboxMac::kSandboxEnableLogging = "ENABLE_LOGGING";
const char* SandboxMac::kSandboxHomedirAsLiteral = "USER_HOMEDIR_AS_LITERAL";
const char* SandboxMac::kSandboxLoggingPathAsLiteral = "LOG_FILE_PATH";
const char* SandboxMac::kSandboxOSVersion = "OS_VERSION";
const char* SandboxMac::kSandboxElCapOrLater = "ELCAP_OR_LATER";
const char* SandboxMac::kSandboxMacOS1013 = "MACOS_1013";
const char* SandboxMac::kSandboxFieldTrialSeverName = "FIELD_TRIAL_SERVER_NAME";
const char* SandboxMac::kSandboxBundleVersionPath = "BUNDLE_VERSION_PATH";

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
// This method is layed out in blocks, each one containing a separate function
// that needs to be warmed up. The OS version on which we found the need to
// enable the function is also noted.
// This function is tested on the following OS versions:
//     10.5.6, 10.6.0

// static
void SandboxMac::Warmup(SandboxType sandbox_type) {
  DCHECK_EQ(sandbox_type, SANDBOX_TYPE_GPU);

  @autoreleasepool {
    {  // CGColorSpaceCreateWithName(), CGBitmapContextCreate() - 10.5.6
      base::ScopedCFTypeRef<CGColorSpaceRef> rgb_colorspace(
          CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

      // Allocate a 1x1 image.
      char data[4];
      base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
          data, 1, 1, 8, 1 * 4, rgb_colorspace,
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));

      // Load in the color profiles we'll need (as a side effect).
      ignore_result(base::mac::GetSRGBColorSpace());
      ignore_result(base::mac::GetSystemColorSpace());

      // CGColorSpaceCreateSystemDefaultCMYK - 10.6
      base::ScopedCFTypeRef<CGColorSpaceRef> cmyk_colorspace(
          CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK));
    }

    {  // localtime() - 10.5.6
      time_t tv = {0};
      localtime(&tv);
    }

    {  // Gestalt() tries to read
       // /System/Library/CoreServices/SystemVersion.plist
      // on 10.5.6
      int32_t tmp;
      base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
    }

    {  // CGImageSourceGetStatus() - 10.6
       // Create a png with just enough data to get everything warmed up...
      char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
      NSData* data = [NSData dataWithBytes:png_header
                                    length:base::size(png_header)];
      base::ScopedCFTypeRef<CGImageSourceRef> img(
          CGImageSourceCreateWithData((CFDataRef)data, NULL));
      CGImageSourceGetStatus(img);
    }

    {
      // Allow access to /dev/urandom.
      base::GetUrandomFD();
    }

    {  // IOSurfaceLookup() - 10.7
      // Needed by zero-copy texture update framework - crbug.com/323338
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceLookup(0));
    }
  }
}

// Load the appropriate template for the given sandbox type.
// Returns the template as a string or an empty string on error.
std::string LoadSandboxTemplate(SandboxType sandbox_type) {
  DCHECK_EQ(sandbox_type, SANDBOX_TYPE_GPU);
  return kSeatbeltPolicyString_gpu;
}

// Turns on the OS X sandbox for this process.

// static
bool SandboxMac::Enable(SandboxType sandbox_type) {
  DCHECK_EQ(sandbox_type, SANDBOX_TYPE_GPU);

  std::string sandbox_data = LoadSandboxTemplate(sandbox_type);
  if (sandbox_data.empty())
    return false;

  sandbox::SandboxCompiler compiler(sandbox_data);

  // Enable verbose logging if enabled on the command line. (See common.sb
  // for details).
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool enable_logging =
      command_line->HasSwitch(switches::kEnableSandboxLogging);
  if (!compiler.InsertBooleanParam(kSandboxEnableLogging, enable_logging))
    return false;

  // Without this, the sandbox will print a message to the system log every
  // time it denies a request.  This floods the console with useless spew.
  if (!compiler.InsertBooleanParam(kSandboxDisableDenialLogging,
                                   !enable_logging))
    return false;

  // Splice the path of the user's home directory into the sandbox profile
  // (see renderer.sb for details).
  std::string home_dir = [NSHomeDirectory() fileSystemRepresentation];
  base::FilePath home_dir_canonical =
      GetCanonicalPath(base::FilePath(home_dir));

  if (!compiler.InsertStringParam(kSandboxHomedirAsLiteral,
                                  home_dir_canonical.value())) {
    return false;
  }

  if (!compiler.InsertStringParam(
          kSandboxFieldTrialSeverName,
          base::MachPortRendezvousClient::GetBootstrapName())) {
    return false;
  }

  bool elcap_or_later = base::mac::IsAtLeastOS10_11();
  if (!compiler.InsertBooleanParam(kSandboxElCapOrLater, elcap_or_later))
    return false;

  bool macos_1013 = base::mac::IsOS10_13();
  if (!compiler.InsertBooleanParam(kSandboxMacOS1013, macos_1013))
    return false;

  if (sandbox_type == service_manager::SANDBOX_TYPE_GPU) {
    base::FilePath bundle_path =
        SandboxMac::GetCanonicalPath(base::mac::FrameworkBundlePath());
    if (!compiler.InsertStringParam(kSandboxBundleVersionPath,
                                    bundle_path.value()))
      return false;
  }

  // Initialize sandbox.
  std::string error_str;
  bool success = compiler.CompileAndApplyProfile(&error_str);
  DLOG_IF(FATAL, !success) << "Failed to initialize sandbox: " << error_str;
  return success;
}

// static
base::FilePath SandboxMac::GetCanonicalPath(const base::FilePath& path) {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(FATAL) << "GetCanonicalSandboxPath() failed for: " << path.value();
    return path;
  }

  base::FilePath::CharType canonical_path[MAXPATHLEN];
  if (HANDLE_EINTR(fcntl(fd.get(), F_GETPATH, canonical_path)) != 0) {
    DPLOG(FATAL) << "GetCanonicalSandboxPath() failed for: " << path.value();
    return path;
  }

  return base::FilePath(canonical_path);
}

// static
std::string SandboxMac::GetSandboxProfile(SandboxType sandbox_type) {
  std::string profile =
      std::string(service_manager::kSeatbeltPolicyString_common);

  switch (sandbox_type) {
    case service_manager::SANDBOX_TYPE_AUDIO:
      profile += service_manager::kSeatbeltPolicyString_audio;
      break;
    case service_manager::SANDBOX_TYPE_CDM:
      profile += service_manager::kSeatbeltPolicyString_cdm;
      break;
    case service_manager::SANDBOX_TYPE_GPU:
      profile += service_manager::kSeatbeltPolicyString_gpu_v2;
      break;
    case service_manager::SANDBOX_TYPE_NACL_LOADER:
      profile += service_manager::kSeatbeltPolicyString_nacl_loader;
      break;
    case service_manager::SANDBOX_TYPE_NETWORK:
      profile += service_manager::kSeatbeltPolicyString_network;
      break;
    case service_manager::SANDBOX_TYPE_PDF_COMPOSITOR:
      profile += service_manager::kSeatbeltPolicyString_pdf_compositor;
      break;
    case service_manager::SANDBOX_TYPE_PPAPI:
      profile += service_manager::kSeatbeltPolicyString_ppapi;
      break;
    case service_manager::SANDBOX_TYPE_PROFILING:
    case service_manager::SANDBOX_TYPE_UTILITY:
      profile += service_manager::kSeatbeltPolicyString_utility;
      break;
    case service_manager::SANDBOX_TYPE_RENDERER:
      profile += service_manager::kSeatbeltPolicyString_renderer;
      break;
    case service_manager::SANDBOX_TYPE_INVALID:
    case service_manager::SANDBOX_TYPE_FIRST_TYPE:
    case service_manager::SANDBOX_TYPE_AFTER_LAST_TYPE:
      CHECK(false);
      break;
  }
  return profile;
}

}  // namespace service_manager
