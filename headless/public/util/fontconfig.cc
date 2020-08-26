// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/fontconfig.h"

// Should be included before freetype.h.
#include <ft2build.h>

#include <dirent.h>
#include <fontconfig/fontconfig.h>
#include <freetype/freetype.h>
#include <set>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

namespace headless {
namespace {
void GetFontFileNames(const FcFontSet* font_set,
                      std::set<std::string>* file_names) {
  if (font_set == NULL)
    return;
  for (int i = 0; i < font_set->nfont; ++i) {
    FcPattern* pattern = font_set->fonts[i];
    FcValue font_file;
    if (FcPatternGet(pattern, "file", 0, &font_file) == FcResultMatch) {
      file_names->insert(reinterpret_cast<const char*>(font_file.u.s));
    } else {
      VLOG(1) << "Failed to find filename.";
      FcPatternPrint(pattern);
    }
  }
}
FcConfig* g_config = nullptr;
}  // namespace

void InitFonts(const char* fontconfig_path) {
  // The following is roughly equivalent to calling FcInit().  We jump through
  // a bunch of hoops here to avoid using fontconfig's directory scanning
  // logic. The problem with fontconfig is that it follows symlinks when doing
  // recursive directory scans.
  //
  // The approach below ignores any <dir>...</dir> entries in fonts.conf.  This
  // is deliberate.  Specifying dirs is problematic because they're either
  // absolute or relative to our process's current working directory which
  // could be anything.  Instead we assume that all font files will be in the
  // same directory as fonts.conf.  We'll scan + load them here.
  FcConfig* config = FcConfigCreate();
  g_config = config;
  CHECK(config);

  // FcConfigParseAndLoad is a seriously goofy function. Depending on whether
  // name passed in begins with a slash, it will treat it either as a file name
  // to be found in the directory where it expects to find the font
  // configuration OR it will will treat it as a directory where it expects to
  // find fonts.conf. The latter behavior is the one we want. Passing
  // fontconfig_path via the environment is a quick and dirty way to get
  // uniform behavior regardless whether it's a relative path or not.
  setenv("FONTCONFIG_PATH", fontconfig_path, 1);
  CHECK(FcConfigParseAndLoad(config, nullptr, FcTrue))
      << "Failed to load font configuration.  FONTCONFIG_PATH="
      << fontconfig_path;

  DIR* fc_dir = opendir(fontconfig_path);
  CHECK(fc_dir) << "Failed to open font directory " << fontconfig_path << ": "
                << strerror(errno);

  // The fonts must be loaded in a consistent order. This makes rendered results
  // stable across runs, otherwise replacement font picks are random
  // and cause flakiness.
  std::set<std::string> fonts;
  struct dirent *result;
  while ((result = readdir(fc_dir))) {
    fonts.insert(result->d_name);
  }
  for (const std::string& font : fonts) {
    const std::string full_path = fontconfig_path + ("/" + font);
    struct stat statbuf;
    CHECK_EQ(0, stat(full_path.c_str(), &statbuf))
        << "Failed to stat " << full_path << ": " << strerror(errno);
    if (S_ISREG(statbuf.st_mode)) {
      // FcConfigAppFontAddFile will silently ignore non-fonts.
      FcConfigAppFontAddFile(
          config, reinterpret_cast<const FcChar8*>(full_path.c_str()));
    }
  }
  closedir(fc_dir);
  CHECK(FcConfigSetCurrent(config));

  // Retrieve font from both of fontconfig's font sets for pre-loading.
  std::set<std::string> font_files;
  GetFontFileNames(FcConfigGetFonts(NULL, FcSetSystem), &font_files);
  GetFontFileNames(FcConfigGetFonts(NULL, FcSetApplication), &font_files);
  CHECK_GT(font_files.size(), 0u)
      << "Font configuration doesn't contain any fonts!";

  // Get freetype to load every font file we know about.  This will cause the
  // font files to get cached in memory.  Once that's done we shouldn't have to
  // access the file system for fonts at all.
  FT_Library library;
  FT_Init_FreeType(&library);
  for (std::set<std::string>::const_iterator iter = font_files.begin();
       iter != font_files.end(); ++iter) {
    FT_Face face;
    CHECK_EQ(0, FT_New_Face(library, iter->c_str(), 0, &face))
        << "Failed to load font face: " << *iter;
    FT_Done_Face(face);
  }
  FT_Done_FreeType(library);  // Cached stuff will stick around... ?
}

void ReleaseFonts() {
  CHECK(g_config);
  FcConfigDestroy(g_config);
  FcFini();
}

}  // namespace headless
