// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_data_source.h"

#include <memory>

#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/font_access/font_enumeration_data_source_win.h"
#elif BUILDFLAG(IS_APPLE)
#include "content/browser/font_access/font_enumeration_data_source_mac.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/browser/font_access/font_enumeration_data_source_linux.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

namespace {

// FontEnumerationDataSource implementation for unsupported OSes.
class FontEnumerationDataSourceNull : public FontEnumerationDataSource {
 public:
  FontEnumerationDataSourceNull() { DETACH_FROM_SEQUENCE(sequence_checker_); }
  FontEnumerationDataSourceNull(const FontEnumerationDataSourceNull&) = delete;
  FontEnumerationDataSourceNull& operator=(
      const FontEnumerationDataSourceNull&) = delete;
  ~FontEnumerationDataSourceNull() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // FontEnumerationDataSource:
  blink::FontEnumerationTable GetFonts(const std::string& locale) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Ensure that this method is called on a sequence that is allowed to do
    // IO, even on OSes that don't have a FontEnumerationDataSource
    // implementation yet.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    blink::FontEnumerationTable font_enumeration_table;
    return font_enumeration_table;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

// static
std::unique_ptr<FontEnumerationDataSource> FontEnumerationDataSource::Create() {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<FontEnumerationDataSourceWin>();
#elif BUILDFLAG(IS_APPLE)
  return std::make_unique<FontEnumerationDataSourceMac>();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<FontEnumerationDataSourceLinux>();
#else
  return std::make_unique<FontEnumerationDataSourceNull>();
#endif  // BUILDFLAG(IS_WIN)
}

// static
bool FontEnumerationDataSource::IsOsSupported() {
  // The structure below parallels Create(), for ease of maintenance.

#if BUILDFLAG(IS_WIN)
  return true;
#elif BUILDFLAG(IS_APPLE)
  return true;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace content
