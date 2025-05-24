// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import org.chromium.build.annotations.NullMarked;

/** Callback interface for handling any errors from register or sign requests. */
@NullMarked
public interface FidoErrorResponseCallback {
    public void onError(int status);
}
