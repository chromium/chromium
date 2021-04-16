// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_DATA_STREAM_H_
#define NET_BASE_TEST_DATA_STREAM_H_

#include <string.h>  // for memcpy().
#include <algorithm>
#include "net/base/net_export.h"

// This is a class for generating an infinite stream of data which can be
// verified independently to be the correct stream of data.

namespace net {

class NET_EXPORT TestDataStream {
 public:
  TestDataStream();

  // Fill |buffer| with |length| bytes of data from the stream.
  void GetBytes(char* buffer, int length);

  // Verify that |buffer| contains the expected next |length| bytes from the
  // stream.  Returns true if correct, false otherwise.
  bool VerifyBytes(const char *buffer, int length);

  // Resets all the data.
  void Reset();

 private:
  // If there is no data spilled over from the previous index, advance the
  // index and fill the buffer.
  void AdvanceIndex();

  // Consume data from the spill buffer.
  void Consume(int bytes);

  int index_;
  int bytes_remaining_;
  char buffer_[16];
  char* buffer_ptr_;
};

}  // namespace net

#endif  // NET_BASE_TEST_DATA_STREAM_H_
