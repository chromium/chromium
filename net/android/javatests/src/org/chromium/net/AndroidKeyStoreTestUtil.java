// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

import java.security.KeyFactory;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;
import java.security.spec.PKCS8EncodedKeySpec;

/** Utility functions to create Android platform keys in tests. */
@JNINamespace("net::android")
public class AndroidKeyStoreTestUtil {

    private static final String TAG = "AndroidKeyStoreTestUtil";

    /**
     * Called from native code to create a PrivateKey object from its encoded PKCS#8 representation.
     *
     * @param algorithm The Java KeyFactory algorithm to use when importing the key.
     * @return new PrivateKey handle, or null in case of error.
     */
    @CalledByNative
    public static PrivateKey createPrivateKeyFromPKCS8(String algorithm, byte[] encodedKey) {
        try {
            @SuppressWarnings("InsecureCryptoUsage") // This util class is for test only.
            KeyFactory factory = KeyFactory.getInstance(algorithm);
            KeySpec ks = new PKCS8EncodedKeySpec(encodedKey);
            PrivateKey key = factory.generatePrivate(ks);
            return key;

        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG, "Could not create " + algorithm + " factory instance!");
            return null;
        } catch (InvalidKeySpecException e) {
            Log.e(TAG, "Could not load " + algorithm + " private key from bytes!");
            return null;
        }
    }
}
