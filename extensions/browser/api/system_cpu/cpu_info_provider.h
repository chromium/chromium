// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_

#include <vector>

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "extensions/browser/api/system_info/system_info_provider.h"
#include "extensions/common/api/system_cpu.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#endif

namespace extensions {

class CpuInfoProvider : public SystemInfoProvider {
 public:
  CpuInfoProvider(const CpuInfoProvider&) = delete;
  CpuInfoProvider& operator=(const CpuInfoProvider&) = delete;

  // Return the single shared instance of CpuInfoProvider.
  static CpuInfoProvider* Get();

  const api::system_cpu::CpuInfo& cpu_info() const { return info_; }

  static void InitializeForTesting(scoped_refptr<CpuInfoProvider> provider);

 private:
  friend class MockCpuInfoProviderImpl;

  CpuInfoProvider();
  ~CpuInfoProvider() override;

  // Platform specific implementation for querying the CPU time information
  // for each processor.
  virtual bool QueryCpuTimePerProcessor(
      std::vector<api::system_cpu::ProcessorInfo>* infos);

  // Overriden from SystemInfoProvider.
  bool QueryInfo() override;

  // Creates a list of codenames for currently active features.
  std::vector<std::string> GetFeatures() const;

  // The last information filled up by QueryInfo and is accessed on multiple
  // threads, but the whole class is being guarded by SystemInfoProvider base
  // class.
  //
  // |info_| is accessed on the UI thread while |is_waiting_for_completion_| is
  // false and on the sequenced worker pool while |is_waiting_for_completion_|
  // is true.
  api::system_cpu::CpuInfo info_;

  static base::LazyInstance<scoped_refptr<CpuInfoProvider>>::DestructorAtExit
      provider_;
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu_;
#endif
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_CPU_CPU_INFO_PROVIDER_H_
