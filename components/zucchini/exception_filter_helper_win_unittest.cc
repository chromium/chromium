// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/exception_filter_helper_win.h"

#include <windows.h>

#include "base/test/gtest_util.h"
#include "base/win/windows_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

static constexpr ULONG_PTR kFlagRead = 0;
static constexpr ULONG_PTR kFlagWrite = 1;
static constexpr ULONG_PTR kTestNtStatus = 47;

}  // namespace

TEST(ExceptionFilterHelperTest, NotPageError) {
  ExceptionFilterHelper helper;

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION)};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_CONTINUE_SEARCH);
}

TEST(ExceptionFilterHelperTest, NoRanges) {
  ExceptionFilterHelper helper;

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 50, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_CONTINUE_SEARCH);
}

TEST(ExceptionFilterHelperTest, OneRangeBelow) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 49, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_CONTINUE_SEARCH);
}

TEST(ExceptionFilterHelperTest, OneRangeAbove) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 51, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_CONTINUE_SEARCH);
}

TEST(ExceptionFilterHelperTest, OneRangeIn) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 50, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_EXECUTE_HANDLER);
  ASSERT_FALSE(helper.is_write());
  ASSERT_EQ(helper.nt_status(), static_cast<int32_t>(kTestNtStatus));
}

TEST(ExceptionFilterHelperTest, TwoRangesBelow) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 49, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_CONTINUE_SEARCH);
}

TEST(ExceptionFilterHelperTest, TwoRangesAbove) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 102, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_CONTINUE_SEARCH);
}

TEST(ExceptionFilterHelperTest, TwoRangesBetween) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 75, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_CONTINUE_SEARCH);
}

TEST(ExceptionFilterHelperTest, TwoRangesInFirst) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 50, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_EXECUTE_HANDLER);
  ASSERT_FALSE(helper.is_write());
  ASSERT_EQ(helper.nt_status(), static_cast<int32_t>(kTestNtStatus));
}

TEST(ExceptionFilterHelperTest, TwoRangesInSecond) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagRead, 100, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_EXECUTE_HANDLER);
  ASSERT_FALSE(helper.is_write());
  ASSERT_EQ(helper.nt_status(), static_cast<int32_t>(kTestNtStatus));
}

TEST(ExceptionFilterHelperTest, IsWrite) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});

  EXCEPTION_RECORD exception_record = {
      .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
      .NumberParameters = 3,
      .ExceptionInformation{kFlagWrite, 50, kTestNtStatus}};
  ASSERT_EQ(helper.FilterPageError(&exception_record),
            EXCEPTION_EXECUTE_HANDLER);
  ASSERT_TRUE(helper.is_write());
  ASSERT_EQ(helper.nt_status(), static_cast<int32_t>(kTestNtStatus));
}

TEST(ExceptionFilterHelperTest, Sweep) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(14), 0U});
  helper.AddRange({reinterpret_cast<uint8_t*>(5), 3U});
  helper.AddRange({reinterpret_cast<uint8_t*>(3), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(10), 1U});

  std::string bitmap(16, ' ');
  for (ULONG_PTR address = 0; address < bitmap.size(); ++address) {
    EXCEPTION_RECORD exception_record = {
        .ExceptionCode = static_cast<DWORD>(EXCEPTION_IN_PAGE_ERROR),
        .NumberParameters = 3,
        .ExceptionInformation{kFlagRead, address, kTestNtStatus}};
    auto result = helper.FilterPageError(&exception_record);
    bitmap[address] = (result == EXCEPTION_EXECUTE_HANDLER) ? '*' : '.';
  }
  ASSERT_STREQ(bitmap.c_str(), "...*****..*.....");
}

TEST(ExceptionFilterHelperDeathTest, DuplicateRange) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});
  ASSERT_CHECK_DEATH({
    helper.AddRange({reinterpret_cast<uint8_t*>(50), 1U});
  });
}

TEST(ExceptionFilterHelperDeathTest, OverlappingFirstRange) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  ASSERT_CHECK_DEATH({
    helper.AddRange({reinterpret_cast<uint8_t*>(49), 2U});
  });
}

TEST(ExceptionFilterHelperDeathTest, CoveringFirstRange) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  ASSERT_CHECK_DEATH({
    helper.AddRange({reinterpret_cast<uint8_t*>(49), 4U});
  });
}

TEST(ExceptionFilterHelperDeathTest, OverlappingLastRange) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  ASSERT_CHECK_DEATH({
    helper.AddRange({reinterpret_cast<uint8_t*>(101), 2U});
  });
}

TEST(ExceptionFilterHelperDeathTest, CoveringLastRange) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  ASSERT_CHECK_DEATH({
    helper.AddRange({reinterpret_cast<uint8_t*>(99), 4U});
  });
}

TEST(ExceptionFilterHelperDeathTest, MidRanges) {
  ExceptionFilterHelper helper;
  helper.AddRange({reinterpret_cast<uint8_t*>(50), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(100), 2U});
  helper.AddRange({reinterpret_cast<uint8_t*>(75), 2U});
  ASSERT_CHECK_DEATH({
    helper.AddRange({reinterpret_cast<uint8_t*>(76), 4U});
  });
}

}  // namespace zucchini
