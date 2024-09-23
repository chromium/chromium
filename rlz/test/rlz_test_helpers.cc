// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main entry point for all unit tests.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz_test_helpers.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "rlz/lib/rlz_lib.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "base/win/shlwapi.h"
#include "rlz/lib/machine_deal_win.h"
#elif BUILDFLAG(IS_POSIX)
#include "base/files/file_path.h"
#include "rlz/lib/rlz_value_store.h"
#endif

#if BUILDFLAG(IS_WIN)

namespace {

// Path to recursively copy into the replacemment hives.  These are needed
// to make sure certain win32 APIs continue to run correctly once the real
// hives are replaced.
const wchar_t kHKLMAccessProviders[] =
    L"System\\CurrentControlSet\\Control\\Lsa\\AccessProviders";

struct RegistryValue {
  std::wstring name;
  DWORD type;
  std::vector<uint8_t> data;
};

struct RegistryKeyData {
  std::vector<RegistryValue> values;
  std::map<std::wstring, RegistryKeyData> keys;
};

void ReadRegistryTree(const base::win::RegKey& src, RegistryKeyData* data) {
  // First read values.
  {
    base::win::RegistryValueIterator i(src.Handle(), L"");
    data->values.clear();
    data->values.reserve(i.ValueCount());
    for (; i.Valid(); ++i) {
      RegistryValue& value = *data->values.insert(data->values.end(),
                                                  RegistryValue());
      const uint8_t* value_bytes = reinterpret_cast<const uint8_t*>(i.Value());
      value.name.assign(i.Name());
      value.type = i.Type();
      value.data.assign(value_bytes, value_bytes + i.ValueSize());
    }
  }

  // Next read subkeys recursively.
  for (base::win::RegistryKeyIterator i(src.Handle(), L"");
       i.Valid(); ++i) {
    ReadRegistryTree(base::win::RegKey(src.Handle(), i.Name(), KEY_READ),
                     &data->keys[std::wstring(i.Name())]);
  }
}

void WriteRegistryTree(const RegistryKeyData& data, base::win::RegKey* dest) {
  // First write values.
  for (size_t i = 0; i < data.values.size(); ++i) {
    const RegistryValue& value = data.values[i];
    dest->WriteValue(value.name.c_str(),
                     value.data.size() ? &value.data[0] : NULL,
                     static_cast<DWORD>(value.data.size()),
                     value.type);
  }

  // Next write values recursively.
  for (std::map<std::wstring, RegistryKeyData>::const_iterator iter =
           data.keys.begin();
       iter != data.keys.end(); ++iter) {
    base::win::RegKey key(dest->Handle(), iter->first.c_str(), KEY_ALL_ACCESS);
    WriteRegistryTree(iter->second, &key);
  }
}

// Initialize temporary HKLM/HKCU registry hives used for testing.
// Testing RLZ requires reading and writing to the Windows registry.  To keep
// the tests isolated from the machine's state, as well as to prevent the tests
// from causing side effects in the registry, HKCU and HKLM are overridden for
// the duration of the tests. RLZ tests don't expect the HKCU and KHLM hives to
// be empty though, and this function initializes the minimum value needed so
// that the test will run successfully.
void InitializeRegistryOverridesForTesting(
    registry_util::RegistryOverrideManager* override_manager) {
  // For the moment, the HKCU hive requires no initialization.
  RegistryKeyData data;

  // Copy the following HKLM subtrees to the temporary location so that the
  // win32 APIs used by the tests continue to work:
  //
  //    HKLM\System\CurrentControlSet\Control\Lsa\AccessProviders
  //
  // This seems to be required since Win7.
  ReadRegistryTree(base::win::RegKey(HKEY_LOCAL_MACHINE,
                                     kHKLMAccessProviders,
                                     KEY_READ), &data);

  ASSERT_NO_FATAL_FAILURE(
      override_manager->OverrideRegistry(HKEY_LOCAL_MACHINE));
  ASSERT_NO_FATAL_FAILURE(
      override_manager->OverrideRegistry(HKEY_CURRENT_USER));

  base::win::RegKey key(
      HKEY_LOCAL_MACHINE, kHKLMAccessProviders, KEY_ALL_ACCESS);
  WriteRegistryTree(data, &key);
}

}  // namespace

#endif  // BUILDFLAG(IS_WIN)

void RlzLibTestNoMachineStateHelper::SetUp() {
#if BUILDFLAG(IS_WIN)
  ASSERT_NO_FATAL_FAILURE(
      InitializeRegistryOverridesForTesting(&override_manager_));
#elif BUILDFLAG(IS_APPLE)
  base::apple::ScopedNSAutoreleasePool pool;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_POSIX)
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  rlz_lib::testing::SetRlzStoreDirectory(temp_dir_.GetPath());
#endif  // BUILDFLAG(IS_POSIX)
}

void RlzLibTestNoMachineStateHelper::TearDown() {
#if BUILDFLAG(IS_POSIX)
  rlz_lib::testing::SetRlzStoreDirectory(base::FilePath());
#endif  // BUILDFLAG(IS_POSIX)
}

void RlzLibTestNoMachineStateHelper::Reset() {
#if BUILDFLAG(IS_POSIX)
  ASSERT_TRUE(temp_dir_.Delete());
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  rlz_lib::testing::SetRlzStoreDirectory(temp_dir_.GetPath());
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_POSIX)
}

void RlzLibTestNoMachineState::SetUp() {
  m_rlz_test_helper_.SetUp();
}

void RlzLibTestNoMachineState::TearDown() {
  m_rlz_test_helper_.TearDown();
}

RlzLibTestBase::RlzLibTestBase() = default;

RlzLibTestBase::~RlzLibTestBase() = default;

void RlzLibTestBase::SetUp() {
  RlzLibTestNoMachineState::SetUp();
#if BUILDFLAG(IS_WIN)
  rlz_lib::CreateMachineState();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX)
  // Make sure the values of RLZ strings for access points used in tests start
  // out not set, since on Chrome OS RLZ string can only be set once.
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IETB_SEARCH_BOX, ""));
  EXPECT_TRUE(rlz_lib::SetAccessPointRlz(rlz_lib::IE_HOME_PAGE, ""));
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  statistics_provider_ =
      std::make_unique<ash::system::FakeStatisticsProvider>();
  ash::system::StatisticsProvider::SetTestProvider(statistics_provider_.get());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void RlzLibTestBase::TearDown() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::StatisticsProvider::SetTestProvider(nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
