// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.webid;

import org.chromium.content.browser.webid.IdentityCredentialsDelegateImpl;

/** Factory class for building {@link IdentityCredentialsDelegate} objects. */
public class IdentityCredentialsDelegateFactory {
    public static IdentityCredentialsDelegate createDefault() {
        return new IdentityCredentialsDelegateImpl();
    }
}
