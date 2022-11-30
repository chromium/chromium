// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ZIP_TEST_DATA_H
#define CRAZY_LINKER_ZIP_TEST_DATA_H

namespace crazy {
namespace testing {

extern const unsigned char empty_archive_zip[];
extern const unsigned int empty_archive_zip_len;

extern const unsigned char hello_zip[];
extern const unsigned int hello_zip_len;

extern const unsigned char hello_compressed_zip[];
extern const unsigned int hello_compressed_zip_len;

extern const unsigned char lib_archive_zip[];
extern const unsigned int lib_archive_zip_len;

}  // namespace testing
}  // namespace crazy

#endif  // CRAZY_LINKER_ZIP_TEST_DATA_H
