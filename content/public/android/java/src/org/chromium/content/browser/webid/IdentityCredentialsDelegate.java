// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import android.app.Activity;

import org.chromium.base.Promise;

/**
 * Delegate interface for calling into GMSCore's private identity credentials.
 *
 * <p>There are two implementations of this interface, in two different repositories. To update this
 * interface without breaking the builds independently, you have to:
 *
 * <p>Step 0) Current state
 *
 * <p>Upstream: Interface: get(int) PublicImpl: get(int)
 *
 * <p>Downstream: Impl: get(int)
 *
 * <p>Step 1) CL#1 in chromium
 *
 * <p>Upstream: Interface: default get(int), default get(int, string) PublicImpl: get(int, string)
 *
 * <p>Downstream: Impl: get(int)
 *
 * <p>Step 2) CL#2 in //clank
 *
 * <p>Upstream: Interface: default get(int), default get(int, string) PublicImpl: get(int, string)
 *
 * <p>Downstream: Impl: get(int, string)
 *
 * <p>Step 3) CL#3 in chromium
 *
 * <p>Upstream: Interface: get(int, string) PublicImpl: get(int, string)
 *
 * <p>Downstream: Impl: get(int, string)
 *
 * <p>Once GMSCore publishes this API publicly, we can have a single implementation.
 *
 * <p>TODO(crbug.com/40279841) delete this once GMSCore publishes this API.
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
