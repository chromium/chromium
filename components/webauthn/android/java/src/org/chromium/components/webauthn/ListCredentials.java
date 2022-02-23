// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import java.util.ArrayList;
import java.util.List;

/**
 * Wrapper for an external call to obtain information about discoverable credentials.
 *
 * <p>Provides a static instance of an interface that can be set by an external caller to implement
 * the listCredentials method. That method provides silent enumeration of discoverable platform
 * credentials for a given relying party, in support of WebAuthn Conditional UI.
 */
public class ListCredentials {
    /**
     * Describes a discoverable credential available on the device.
     */
    public class DiscoverableCredentialInfo {
        private String mUserName;
        private String mUserDisplayName;
        private byte[] mUserId;
        private byte[] mCredentialId;

        DiscoverableCredentialInfo(
                String userName, String userDisplayName, byte[] userId, byte[] credentialId) {
            mUserName = userName;
            mUserDisplayName = userDisplayName;
            mUserId = userId;
            mCredentialId = credentialId;
        }

        String getUserName() {
            return mUserName;
        }

        String getUserDisplayName() {
            return mUserDisplayName;
        }

        byte[] getUserId() {
            return mUserId;
        }

        byte[] getCredentialId() {
            return mCredentialId;
        }
    }

    /**
     * Abstract interface for an embedder to provide a listCredentials method.
     */
    public interface AbstractListCredentials {
        List<DiscoverableCredentialInfo> listCredentials(String rpId);
    }

    private static AbstractListCredentials sExternalListCredentials;

    /**
     * Sets an interface that will provide a listCredentials method implementation.
     *
     * @param listCredentials the external interface.
     */
    public static void setExternalListCredentials(AbstractListCredentials listCredentials) {
        sExternalListCredentials = listCredentials;
    }

    /**
     * Returns a list of discoverable credentials available on this design.
     *
     * @param rpId indicates the relying party for which this will return any available
     * discoverable credentials.
     */
    public static List<DiscoverableCredentialInfo> listCredentials(String rpId) {
        if (sExternalListCredentials != null) {
            return sExternalListCredentials.listCredentials(rpId);
        }
        return new ArrayList<DiscoverableCredentialInfo>();
    }
}
