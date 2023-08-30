// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import org.chromium.base.Promise;

/**
 * Delegate interface for calling into GMSCore's private identity credentials.
 *
 * TODO(crbug.com/1475970) delete this once GMSCore publishes this API.
 */
public interface IdentityCredentialsDelegate {
    // TODO(goto): replace with the actual input/output parameters
    public Promise<String> get(String origin, String request);
}
