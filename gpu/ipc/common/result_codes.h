// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_RESULT_CODES_H_
#define GPU_IPC_COMMON_RESULT_CODES_H_

namespace gpu {

// This is the same as content::RESULT_CODE_HUNG in
// content/public/common/result_codes.h
enum ResultCode {
  // Process hung.
  RESULT_CODE_HUNG = 2,
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_RESULT_CODES_H_
