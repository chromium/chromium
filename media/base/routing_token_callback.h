// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ROUTING_TOKEN_CALLBACK_H_
#define MEDIA_BASE_ROUTING_TOKEN_CALLBACK_H_

#include "base/callback.h"
#include "base/unguessable_token.h"

namespace media {

// Handy callback type to provide a routing token.
using RoutingTokenCallback =
    base::OnceCallback<void(const base::UnguessableToken&)>;

// Callback to register a RoutingTokenCallback with something that can provide
// it.  For example, RenderFrame(Impl) will provide this, while WMPI can choose
// to call it if it would like to be called back with a routing token.
using RequestRoutingTokenCallback =
    base::RepeatingCallback<void(RoutingTokenCallback)>;

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ROUTING_TOKEN_CALLBACK_H_
