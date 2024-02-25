// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import java.util.List;

/**
 * Callback interface for receiving a response from a request to retrieve matching credential ids
 * from an authenticator.
 */
public interface GetMatchingCredentialIdsResponseCallback {
    public void onResponse(List<byte[]> matchingCredentialIds);
}
