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
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/metrics/field_trial_memory_mac.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "services/service_manager/sandbox/mac/audio.sb.h"
#include "services/service_manager/sandbox/mac/cdm.sb.h"
#include "services/service_manager/sandbox/mac/common.sb.h"
#include "services/service_manager/sandbox/mac/gpu.sb.h"
#include "services/service_manager/sandbox/mac/nacl_loader.sb.h"
#include "services/service_manager/sandbox/mac/ppapi.sb.h"
#include "services/service_manager/sandbox/mac/renderer.sb.h"
#include "services/service_manager/sandbox/mac/utility.sb.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"

namespace service_manager {
namespace {

// Is the sandbox currently active.
bool gSandboxIsActive = false;

struct SandboxTypeToResourceIDMapping {
  SandboxType sandbox_type;
  const char* seatbelt_policy_string;
};

// Mapping from sandbox process types to resource IDs containing the sandbox
// profile for all process types known to service_manager.
// TODO(tsepez): Implement profile for SANDBOX_TYPE_NETWORK.
SandboxTypeToResourceIDMapping kDefaultSandboxTypeToResourceIDMapping[] = {
    {SANDBOX_TYPE_NO_SANDBOX, nullptr},
    {SANDBOX_TYPE_RENDERER, kSeatbeltPolicyString_renderer},
    {SANDBOX_TYPE_UTILITY, kSeatbeltPolicyString_utility},
    {SANDBOX_TYPE_GPU, kSeatbeltPolicyString_gpu},
    {SANDBOX_TYPE_PPAPI, kSeatbeltPolicyString_ppapi},
    {SANDBOX_TYPE_NETWORK, nullptr},
    {SANDBOX_TYPE_CDM, kSeatbeltPolicyString_cdm},
    {SANDBOX_TYPE_NACL_LOADER, kSeatbeltPolicyString_nacl_loader},
    {SANDBOX_TYPE_PDF_COMPOSITOR, kSeatbeltPolicyString_ppapi},
    {SANDBOX_TYPE_PROFILING, kSeatbeltPolicyString_utility},
    {SANDBOX_TYPE_AUDIO, kSeatbeltPolicyString_audio},
};

static_assert(arraysize(kDefaultSandboxTypeToResourceIDMapping) ==
                  size_t(SANDBOX_TYPE_AFTER_LAST_TYPE),
              "sandbox type to resource id mapping incorrect");

}  // namespace

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
  base::mac::ScopedNSAutoreleasePool scoped_pool;

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

  {  // Gestalt() tries to read /System/Library/CoreServices/SystemVersion.plist
    // on 10.5.6
    int32_t tmp;
    base::SysInfo::OperatingSystemVersionNumbers(&tmp, &tmp, &tmp);
  }

  {  // CGImageSourceGetStatus() - 10.6
     // Create a png with just enough data to get everything warmed up...
    char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    NSData* data =
        [NSData dataWithBytes:png_header length:arraysize(png_header)];
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

  // Process-type dependent warm-up.
  if (sandbox_type == SANDBOX_TYPE_UTILITY) {
    // CFTimeZoneCopyZone() tries to read /etc and /private/etc/localtime - 10.8
    // Needed by Media Galleries API Picasa - crbug.com/151701
    CFTimeZoneCopySystem();
  }

  if (sandbox_type == SANDBOX_TYPE_PPAPI ||
      sandbox_type == SANDBOX_TYPE_PDF_COMPOSITOR) {
    // Preload AppKit color spaces used for ppapi(https://crbug.com/348304),
    // as well as pdf compositor service likely on version 10.10 or
    // older(https://crbug.com/822218).
    NSColor* color = [NSColor controlTextColor];
    [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  }
}

// Load the appropriate template for the given sandbox type.
// Returns the template as a string or an empty string on error.
std::string LoadSandboxTemplate(SandboxType sandbox_type) {
  // We use a custom sandbox definition to lock things down as tightly as
  // possible.
  auto* it = std::find_if(
      std::begin(kDefaultSandboxTypeToResourceIDMapping),
      std::end(kDefaultSandboxTypeToResourceIDMapping),
      [sandbox_type](const SandboxTypeToResourceIDMapping& element) {
        return element.sandbox_type == sandbox_type;
      });

  CHECK(it != std::end(kDefaultSandboxTypeToResourceIDMapping))
      << "Unknown sandbox type " << sandbox_type;
  base::StringPiece sandbox_definition = it->seatbelt_policy_string;

  // Prefix sandbox_data with common_sandbox_prefix_data.
  std::string sandbox_profile = kSeatbeltPolicyString_common;
  sandbox_definition.AppendToString(&sandbox_profile);
  return sandbox_profile;
}

// Turns on the OS X sandbox for this process.

// static
bool SandboxMac::Enable(SandboxType sandbox_type) {
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
          base::FieldTrialMemoryClient::GetBootstrapName())) {
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
  gSandboxIsActive = success;
  return success;
}

// static
bool SandboxMac::IsCurrentlyActive() {
  return gSandboxIsActive;
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

}  // namespace service_manager
