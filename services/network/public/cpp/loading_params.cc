// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/loading_params.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "net/base/mime_sniffer.h"

namespace network {

namespace {

// The default Mojo ring buffer size, used to send the content body.
constexpr uint32_t kDefaultDataPipeAllocationSize = 512 * 1024;

// The larger ring buffer size, used primarily for network::URLLoader loads.
// This value was optimized via Finch: see crbug.com/1041006.
constexpr uint32_t kLargerDataPipeAllocationSize = 2 * 1024 * 1024;

// The smallest buffer size must be larger than the maximum MIME sniffing
// chunk size. This is assumed several places in content/browser/loader.
static_assert(kDefaultDataPipeAllocationSize < kLargerDataPipeAllocationSize);
static_assert(kDefaultDataPipeAllocationSize >= net::kMaxBytesToSniff,
              "Smallest data pipe size must be at least as large as a "
              "MIME-type sniffing buffer.");

}  // namespace

uint32_t GetDataPipeDefaultAllocationSize(DataPipeAllocationSize option) {
#if BUILDFLAG(IS_CHROMEOS) || defined(ARCH_CPU_32_BITS)
  // TODO(crbug.com/1306998): ChromeOS experiences a much higher OOM crash
  // rate if the larger data pipe size is used.
  //
  // On 32 bit architectures, address space and address space fragmentation are
  // both concerns. There, don't attempt to have a 2MiB buffer. See
  // crbug/385632154 for an example where this was causing crashes.
  return kDefaultDataPipeAllocationSize;
#else
  switch (option) {
    case DataPipeAllocationSize::kDefaultSizeOnly:
      return kDefaultDataPipeAllocationSize;
    case DataPipeAllocationSize::kLargerSizeIfPossible:
      return kLargerDataPipeAllocationSize;
  }
#endif
}

}  // namespace network
