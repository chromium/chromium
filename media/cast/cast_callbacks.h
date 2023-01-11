// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_CALLBACKS_H_
#define MEDIA_CAST_CAST_CALLBACKS_H_

#include "base/functional/callback_forward.h"
#include "media/cast/constants.h"

namespace media {
namespace cast {

// Callback that is run to update the client with current status.  This is used
// to allow the client to wait for asynchronous initialization to complete
// before sending frames, and also to be notified of any runtime errors that
// have halted the session.
using StatusChangeCallback = base::RepeatingCallback<void(OperationalStatus)>;

// The equivalent of StatusChangeCallback when only one change is expected.
using StatusChangeOnceCallback = base::OnceCallback<void(OperationalStatus)>;

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_CALLBACKS_H_
