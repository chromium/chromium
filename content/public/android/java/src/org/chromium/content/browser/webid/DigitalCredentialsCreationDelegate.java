// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import android.app.Activity;

import org.chromium.base.Promise;

/**
 * Delegate interface for calling into GMSCore's private identity credentials.
 *
 * <p>TODO(crbug.com/380039257) delete this once GMSCore publishes this API.
 */
public interface DigitalCredentialsCreationDelegate {
    public Promise<String> create(Activity window, String origin, String request);
}
