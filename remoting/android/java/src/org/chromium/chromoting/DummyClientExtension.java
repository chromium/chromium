// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;

/**
 * A dummy implementation of ClientExtension.
 */
public class DummyClientExtension implements ClientExtension {

    @Override
    public String getCapability() {
        return "";
    }

    @Override
    public boolean onExtensionMessage(String type, String data) {
        return false;
    }

    @Override
    public ActivityLifecycleListener onActivityAcceptingListener(Activity activity) {
        return null;
    }
}
