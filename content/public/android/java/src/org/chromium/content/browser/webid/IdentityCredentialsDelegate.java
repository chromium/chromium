// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webid;

import android.app.Activity;

import org.chromium.base.Promise;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@NullMarked
public class IdentityCredentialsDelegate {
    private static final String TAG = "IdentityCredentials";

    public static class DigitalCredential {
        @Nullable public String mProtocol;
        public String mData;

        public DigitalCredential(@Nullable String protocol, byte[] data) {
            this.mProtocol = protocol;
            this.mData = new String(data);
        }

        public DigitalCredential(@Nullable String protocol, String data) {
            this.mProtocol = protocol;
            this.mData = data;
        }
    }

    public @Nullable Promise<String> get(String origin, String request) {
        // TODO(crbug.com/40257092): implement this.
        return null;
    }

    public Promise<DigitalCredential> get(Activity window, String origin, String request) {
        DigitalCredentialsPresentationDelegate presentationDelegate =
                new DigitalCredentialsPresentationDelegate();
        return presentationDelegate.get(window, origin, request);
    }

    public Promise<String> create(Activity window, String origin, String request) {
        DigitalCredentialsCreationDelegate delegateImpl =
                ServiceLoaderUtil.maybeCreate(DigitalCredentialsCreationDelegate.class);
        if (delegateImpl != null) {
            return delegateImpl.create(window, origin, request);
        }
        return Promise.rejected();
    }
}
