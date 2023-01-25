// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_EXTERNAL_CLEAR_KEY_TEST_HELPER_H_
#define MEDIA_CDM_EXTERNAL_CLEAR_KEY_TEST_HELPER_H_

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "media/base/cdm_config.h"
#include "media/cdm/clear_key_cdm_common.h"

namespace media {

// This class loads the library containing External Clear Key. The library is
// loaded and initialized in the constructor, and unloaded in the destructor.
class ExternalClearKeyTestHelper {
 public:
  ExternalClearKeyTestHelper();

  ExternalClearKeyTestHelper(const ExternalClearKeyTestHelper&) = delete;
  ExternalClearKeyTestHelper& operator=(const ExternalClearKeyTestHelper&) =
      delete;

  ~ExternalClearKeyTestHelper();

  media::CdmConfig CdmConfig() {
    return {kExternalClearKeyKeySystem, false, false, false};
  }

  base::FilePath LibraryPath() { return library_path_; }

 private:
  // Methods to load and unload the library. Required as the compiler
  // doesn't like ASSERTs in the constructor/destructor.
  void LoadLibrary();
  void UnloadLibrary();

  // Keep a reference to the loaded library.
  base::FilePath library_path_;
  base::ScopedNativeLibrary library_;
};

}  // namespace media

#endif  // MEDIA_CDM_EXTERNAL_CLEAR_KEY_TEST_HELPER_H_
