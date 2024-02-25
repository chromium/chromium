// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import android.app.Activity;

import org.chromium.base.Promise;

/**
 * Delegate interface for calling into GMSCore's private identity credentials.
 *
 * There are two implementations of this interface, in two different repositories.
 * To update this interface without breaking the builds independently, you have to:
 *
 * Step 0) Current state
 *
 *   Upstream:
 *     Interface: get(int)
 *     PublicImpl: get(int)
 *
 *   Downstream:
 *     Impl: get(int)
 *
 * Step 1) CL#1 in chromium
 *
 *   Upstream:
 *     Interface: default get(int), default get(int, string)
 *     PublicImpl: get(int, string)
 *
 *   Downstream:
 *     Impl: get(int)
 *
 * Step 2) CL#2 in //clank
 *
 *   Upstream:
 *     Interface: default get(int), default get(int, string)
 *     PublicImpl: get(int, string)
 *
 *   Downstream:
 *     Impl: get(int, string)
 *
 * Step 3) CL#3 in chromium
 *
 *    Upstream:
 *     Interface: get(int, string)
 *     PublicImpl: get(int, string)
 *
 *   Downstream:
 *     Impl: get(int, string)
 *
 * Once GMSCore publishes this API publicly, we can have a single implementation.
 *
 * TODO(crbug.com/1475970) delete this once GMSCore publishes this API.
 *
 */
public interface IdentityCredentialsDelegate {
    public default Promise<String> get(String origin, String request) {
        return Promise.rejected();
    }

    public default Promise<byte[]> get(Activity activity, String origin, String request) {
        return Promise.rejected();
    }

    public default Promise<Void> register(Activity activity, byte[] credential, byte[] matcher) {
        return Promise.rejected();
    }
}
