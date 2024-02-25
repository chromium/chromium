// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

/** Callback interface for handling any errors from register or sign requests. */
public interface FidoErrorResponseCallback {
    public void onError(int status);
}
