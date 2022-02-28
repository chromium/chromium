// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts.h"

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace skia {

void ConfigureTestFont() {
  // Load font files in the resource folder.
  static const char* const kFontFileNames[] = {"Ahem.ttf",
                                               "ChromiumAATTest.ttf"};

  NSMutableArray* font_urls = [NSMutableArray array];
  for (unsigned i = 0; i < std::size(kFontFileNames); ++i) {
    base::ScopedCFTypeRef<CFStringRef> file_name(
        base::SysUTF8ToCFStringRef(kFontFileNames[i]));
    NSURL* font_url = base::mac::FilePathToNSURL(
        base::mac::PathForFrameworkBundleResource(file_name));
    [font_urls addObject:[font_url absoluteURL]];
  }

  CFArrayRef errors = 0;
  if (!CTFontManagerRegisterFontsForURLs((CFArrayRef)font_urls,
                                         kCTFontManagerScopeProcess, &errors)) {
    DLOG(FATAL) << "Fail to activate fonts.";
    CFRelease(errors);
  }
}

}  // namespace skia
