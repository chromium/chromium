// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn.cred_man;

import android.credentials.CreateCredentialRequest;
import android.os.Bundle;

/** Interface for code that will update the CredMan request bundles or requests. */
interface CredManRequestDecorator {
    void updateCreateCredentialRequestBundle(
            Bundle input, CredManCreateCredentialRequestHelper helper);

    void updateCreateCredentialRequestBuilder(
            CreateCredentialRequest.Builder builder, CredManCreateCredentialRequestHelper helper);
}
