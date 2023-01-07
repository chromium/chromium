// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef LIBRARIES_NACL_IO_ERROR_H_
#define LIBRARIES_NACL_IO_ERROR_H_

namespace nacl_io {

struct Error {
  // TODO(binji): Add debugging constructor w/ __FILE__, __LINE__.
  // crbug.com/247816
  Error(int error) : error(error) {}
  operator int() const { return error; }

  int error;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_ERROR_H_
