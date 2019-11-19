// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_URL_CONVERSION_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_URL_CONVERSION_H_

#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/platform/web_common.h"

class GURL;

namespace blink {

class WebString;

BLINK_PLATFORM_EXPORT GURL WebStringToGURL(const WebString&);

// Convert a data url to a message pipe handle that corresponds to a remote
// blob, so that it can be passed across processes.
BLINK_PLATFORM_EXPORT mojo::ScopedMessagePipeHandle DataURLToMessagePipeHandle(
    const WebString&);

}  // namespace blink

#endif
