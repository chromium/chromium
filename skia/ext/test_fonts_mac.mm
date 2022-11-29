// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts.h"

#include <CoreText/CoreText.h>
#include <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"

namespace skia {

void InitializeSkFontMgrForTest() {
  // Load font files in the resource folder.
  static const char* const kFontFileNames[] = {"Ahem.ttf",
                                               "ChromiumAATTest.ttf"};

  NSMutableArray* font_urls = [NSMutableArray array];
  for (auto* font_file_name : kFontFileNames) {
    NSURL* font_url = base::mac::FilePathToNSURL(
        base::mac::PathForFrameworkBundleResource(font_file_name));
    [font_urls addObject:font_url.absoluteURL];
  }

  if (@available(macOS 10.15, *)) {
    CTFontManagerRegisterFontURLs(
        base::mac::NSToCFCast(font_urls), kCTFontManagerScopeProcess,
        /*enabled=*/true, ^bool(CFArrayRef errors, bool done) {
          if (CFArrayGetCount(errors)) {
            DLOG(FATAL) << "Failed to activate fonts.";
          }
          return true;
        });
  } else {
    CFArrayRef errors = nullptr;
    if (!CTFontManagerRegisterFontsForURLs(base::mac::NSToCFCast(font_urls),
                                           kCTFontManagerScopeProcess,
                                           &errors)) {
      DLOG(FATAL) << "Failed to activate fonts.";
      CFRelease(errors);
    }
  }
}

}  // namespace skia
