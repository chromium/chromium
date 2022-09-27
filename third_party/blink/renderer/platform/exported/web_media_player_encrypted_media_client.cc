// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"

// This WebMediaPlayerEncryptedMediaClient.cpp, which includes only
// and WebMediaPlayerEncryptedMediaClient.h, should be in
// Source/platform/exported, because WebMediaPlayerEncryptedMediaClient is not
// compiled without this cpp.
// So if we don't have this cpp, we will see unresolved symbol error
// when constructor/destructor's address is required.
