// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/scoped_logger.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace WTF {

#if DCHECK_IS_ON()
static const int kPrinterBufferSize = 256;
static char g_buffer[kPrinterBufferSize];
static StringBuilder g_builder;

static void Vprint(const char* format, va_list args) {
  int written = vsnprintf(g_buffer, kPrinterBufferSize, format, args);
  if (written > 0 && written < kPrinterBufferSize)
    g_builder.Append(g_buffer);
}

TEST(ScopedLoggerTest, ScopedLogger) {
  ScopedLogger::SetPrintFuncForTests(Vprint);
  {
    WTF_CREATE_SCOPED_LOGGER(a, "a1");
    {
      WTF_CREATE_SCOPED_LOGGER_IF(b, false, "b1");
      {
        WTF_CREATE_SCOPED_LOGGER(c, "c");
        { WTF_CREATE_SCOPED_LOGGER(d, "d %d %s", -1, "hello"); }
      }
      WTF_APPEND_SCOPED_LOGGER(b, "b2");
    }
    WTF_APPEND_SCOPED_LOGGER(a, "a2 %.1f", 0.5);
  }

  EXPECT_EQ(
      "( a1\n"
      "  ( c\n"
      "    ( d -1 hello )\n"
      "  )\n"
      "  a2 0.5\n"
      ")\n",
      g_builder.ToString());
}
#endif  // DCHECK_IS_ON()

}  // namespace WTF
