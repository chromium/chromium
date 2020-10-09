// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CREATE_WEB_MESSAGE_H_
#define FUCHSIA_RUNNERS_CAST_CREATE_WEB_MESSAGE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <memory>

#include "base/strings/string_piece.h"
#include "components/cast/message_port/message_port.h"

// Utility function for creating a fuchsia.web.WebMessage with the payload
// |message| and an optional transferred |port|.
fuchsia::web::WebMessage CreateWebMessage(
    base::StringPiece message,
    std::unique_ptr<cast_api_bindings::MessagePort> port);

#endif  // FUCHSIA_RUNNERS_CAST_CREATE_WEB_MESSAGE_H_
