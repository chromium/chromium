// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/test_utils.h"
#include "media/remoting/receiver_controller.h"

namespace media {
namespace remoting {

void ResetForTesting(ReceiverController* controller) {
  controller->receiver_.reset();
  controller->media_remotee_.reset();
}

}  // namespace remoting
}  // namespace media
