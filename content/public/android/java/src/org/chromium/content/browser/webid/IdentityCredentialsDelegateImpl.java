// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import org.chromium.base.Promise;

/**
 * Public no-op implementation of IdentityCredentialsDelegate.
 *
 * TODO(crbug.com/1475970) upstream the private version of this.
 */
public class IdentityCredentialsDelegateImpl implements IdentityCredentialsDelegate {
    @Override
    public Promise<String> get(String origin, String request) {
        return null;
    }
}
