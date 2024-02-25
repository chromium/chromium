// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/** Authenticator service for testing SPNEGO (Kerberos) support. */
public class DummySpnegoAuthenticatorService extends Service {
    private static DummySpnegoAuthenticator sAuthenticator;

    @Override
    public IBinder onBind(Intent arg0) {
        if (sAuthenticator == null) sAuthenticator = new DummySpnegoAuthenticator(this);
        return sAuthenticator.getIBinder();
    }
}
