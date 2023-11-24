// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/** A {@link android.app.Service} that creates a new {@link EmbeddedTestServer} when bound. */
public class EmbeddedTestServerService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return new EmbeddedTestServerImpl(this);
    }
}
