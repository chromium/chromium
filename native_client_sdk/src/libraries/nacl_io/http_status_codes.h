// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_HTTP_STATUS_CODES_H_
#define LIBRARIES_NACL_IO_HTTP_STATUS_CODES_H_

namespace nacl_io {

const int32_t STATUSCODE_OK = 200;
const int32_t STATUSCODE_PARTIAL_CONTENT = 206;
const int32_t STATUSCODE_FORBIDDEN = 403;
const int32_t STATUSCODE_NOT_FOUND = 404;
const int32_t STATUSCODE_REQUESTED_RANGE_NOT_SATISFIABLE = 416;

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_HTTP_STATUS_CODES_H_
